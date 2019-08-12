///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2019 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include "thread_vapoursynth.h"

//Mutils
#include <MUtils/OSSupport.h>
#include <MUtils/Registry.h>

//Qt
#include <QEventLoop>
#include <QTimer>
#include <QApplication>
#include <QDir>
#include <QHash>
#include <QAbstractFileEngine.h>

//Internal
#include "global.h"
#include "model_sysinfo.h"

//CRT
#include <cassert>

//Static
QMutex VapourSynthCheckThread::m_vpsLock;
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];

//Const
static const char* const VPS_DLL_NAME = "vapoursynth.dll";
static const char* const VPS_EXE_NAME = "vspipe.exe";
static const char* const VPS_REG_NAME = "VapourSynthDLL";

//Default VapurSynth architecture
#if _WIN64 || __x86_64__
#define VAPOURSYNTH_DEF VAPOURSYNTH_X64
#else
#define VAPOURSYNTH_DEF VAPOURSYNTH_X86;
#endif

//Enable detection of "portabel" edition?
#define ENABLE_PORTABLE_VPS true

//Registry scope EOL flag
#define REG_SCOPE_EOL (MUtils::Registry::reg_scope_t(-1))

//Auxilary functions
#define BOOLIFY(X) ((X) ? '1' : '0')
#define VPS_BITNESS(X) (((X) + 1U) * 32U)

//-------------------------------------
// External API
//-------------------------------------

bool VapourSynthCheckThread::detect(SysinfoModel* sysinfo)
{
	QMutexLocker lock(&m_vpsLock);

	sysinfo->clearVapourSynth();
	sysinfo->clearVPS32Path();
	sysinfo->clearVPS64Path();

	QEventLoop loop;
	VapourSynthCheckThread thread;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));

	thread.start();
	QTimer::singleShot(30000, &loop, SLOT(quit()));

	qDebug("VapourSynth thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("VapourSynth thread finished.");

	QApplication::restoreOverrideCursor();

	if (!thread.wait(1000))
	{
		qWarning("VapourSynth thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return false;
	}

	if (thread.getException())
	{
		qWarning("VapourSynth thread encountered an exception !!!");
		return false;
	}

	const int success = thread.getSuccess();
	if (!success)
	{
		qWarning("VapourSynth could not be found -> VapourSynth support disabled!");
		return true;
	}

	if (success & VAPOURSYNTH_X86)
	{
		sysinfo->setVapourSynth(SysinfoModel::VapourSynth_X86, true);
		sysinfo->setVPS32Path(thread.getPath32());
	}

	if (success & VAPOURSYNTH_X64)
	{
		sysinfo->setVapourSynth(SysinfoModel::VapourSynth_X64, true);
		sysinfo->setVPS64Path(thread.getPath64());
	}

	qDebug("VapourSynth support is officially enabled now! [x86=%c, x64=%c]", BOOLIFY(sysinfo->getVapourSynth(SysinfoModel::VapourSynth_X86)), BOOLIFY(sysinfo->getVapourSynth(SysinfoModel::VapourSynth_X64)));
	return true;
}

//-------------------------------------
// Thread functions
//-------------------------------------

VapourSynthCheckThread::VapourSynthCheckThread(void)
{
	m_vpsPath[0U].clear();
	m_vpsPath[1U].clear();
}

VapourSynthCheckThread::~VapourSynthCheckThread(void)
{
}

void VapourSynthCheckThread::run(void)
{
	m_vpsPath[0U].clear();
	m_vpsPath[1U].clear();
	StarupThread::run();
}

int VapourSynthCheckThread::threadMain(void)
{
	static const int VPS_BIT_FLAG[] =
	{
		VAPOURSYNTH_X86,
		VAPOURSYNTH_X64,
		NULL
	};
	static const char* const VPS_REG_PATH[] =
	{
		"SOFTWARE\\VapourSynth",
		"SOFTWARE\\VapourSynth-32",
		NULL
	};
	static const MUtils::Registry::reg_scope_t REG_SCOPE_X86[] =
	{
		MUtils::Registry::scope_default,
		REG_SCOPE_EOL
	};
	static const MUtils::Registry::reg_scope_t REG_SCOPE_X64[] =
	{
		MUtils::Registry::scope_wow_x32,
		MUtils::Registry::scope_wow_x64,
		REG_SCOPE_EOL
	};

	QHash<int, QFileInfo> vpsDllInfo, vpsExeInfo;
	int flags = 0;

	//Look for "portable" VapourSynth version
	for (size_t i = 0; i < 2U; i++)
	{
		const QDir vpsPortableDir(QString("%1/extra/VapourSynth-%2").arg(QCoreApplication::applicationDirPath(), QString::number(VPS_BITNESS(i))));
		if (vpsPortableDir.exists())
		{
			const QFileInfo vpsPortableDll(vpsPortableDir.absoluteFilePath(VPS_DLL_NAME));
			const QFileInfo vpsPortableExe(vpsPortableDir.absoluteFilePath(VPS_EXE_NAME));
			if ((vpsPortableDll.exists() && vpsPortableDll.isFile()) || (vpsPortableExe.exists() && vpsPortableExe.isFile()))
			{
				vpsDllInfo.insert(VPS_BIT_FLAG[i], vpsPortableDll);
				vpsExeInfo.insert(VPS_BIT_FLAG[i], vpsPortableExe);
			}
		}
	}

	//Read VapourSynth path from registry
	if (vpsDllInfo.isEmpty() && vpsExeInfo.isEmpty())
	{
		//Try to detect the path from HKEY_LOCAL_MACHINE first!
		const MUtils::Registry::reg_scope_t* const scope = (MUtils::OS::os_architecture() == MUtils::OS::ARCH_X64) ? REG_SCOPE_X64 : REG_SCOPE_X86;
		for (size_t i = 0; scope[i] != REG_SCOPE_EOL; i++)
		{
			if (MUtils::Registry::reg_key_exists(MUtils::Registry::root_machine, QString::fromLatin1(VPS_REG_PATH[0U]), scope[i]))
			{
				QString vpsRegDllPath;
				if (MUtils::Registry::reg_value_read(MUtils::Registry::root_machine, QString::fromLatin1(VPS_REG_PATH[0U]), QString::fromLatin1(VPS_REG_NAME), vpsRegDllPath, scope[i]))
				{
					QFileInfo vpsRegDllInfo(QDir::fromNativeSeparators(vpsRegDllPath));
					vpsRegDllInfo.makeAbsolute();
					if (vpsRegDllInfo.exists() && vpsRegDllInfo.isFile())
					{
						const int flag = getVapourSynthType(scope[i]);
						if ((!vpsDllInfo.contains(flag)) || (!vpsExeInfo.contains(flag)))
						{
							vpsDllInfo.insert(flag, vpsRegDllInfo);
							vpsExeInfo.insert(flag, vpsRegDllInfo.absoluteDir().absoluteFilePath(VPS_EXE_NAME)); /*derive VSPipe.EXE path from VapourSynth.DLL path for now!*/
						}
					}
				}
			}
		}
		//Fall back to HKEY_CURRENT_USER, if path not found yet
		for (size_t i = 0; VPS_REG_PATH[i]; i++)
		{
			if (MUtils::Registry::reg_key_exists(MUtils::Registry::root_user, QString::fromLatin1(VPS_REG_PATH[i])))
			{
				QString vpsRegDllPath;
				if (MUtils::Registry::reg_value_read(MUtils::Registry::root_user, QString::fromLatin1(VPS_REG_PATH[i]), QString::fromLatin1(VPS_REG_NAME), vpsRegDllPath))
				{
					QFileInfo vpsRegDllInfo(QDir::fromNativeSeparators(vpsRegDllPath));
					vpsRegDllInfo.makeAbsolute();
					if (vpsRegDllInfo.exists() && vpsRegDllInfo.isFile())
					{
						const int flag = (i) ? VAPOURSYNTH_X86 : VAPOURSYNTH_X64;
						if ((!vpsDllInfo.contains(flag)) || (!vpsExeInfo.contains(flag)))
						{
							vpsDllInfo.insert(flag, vpsRegDllInfo);
							vpsExeInfo.insert(flag, vpsRegDllInfo.absoluteDir().absoluteFilePath(VPS_EXE_NAME)); /*derive VSPipe.EXE path from VapourSynth.DLL path for now!*/
						}
					}
				}
			}
		}
	}

	//Abort, if VapourSynth was *not* found
	if (vpsDllInfo.isEmpty() || vpsExeInfo.isEmpty())
	{
		qWarning("VapourSynth install path not found -> disable VapouSynth support!");
		return 0;
	}

	//Validate the VapourSynth installation now!
	for (size_t i = 0; i < 2U; i++)
	{
		qDebug("VapourSynth %u-Bit support is being tested.", VPS_BITNESS(i));
		if (vpsDllInfo.contains(VPS_BIT_FLAG[i]) && vpsExeInfo.contains(VPS_BIT_FLAG[i]))
		{
			QFile *vpsExeFile, *vpsDllFile;
			if (isVapourSynthComplete(vpsDllInfo[VPS_BIT_FLAG[i]], vpsExeInfo[VPS_BIT_FLAG[i]], vpsExeFile, vpsDllFile))
			{
				m_vpsExePath[i].reset(vpsExeFile);
				m_vpsDllPath[i].reset(vpsDllFile);
				if (checkVapourSynth(m_vpsExePath[i]->fileEngine()->fileName(QAbstractFileEngine::CanonicalName)))
				{
					qDebug("VapourSynth %u-Bit edition found!", VPS_BITNESS(i));
					m_vpsPath[i] = m_vpsExePath[i]->fileEngine()->fileName(QAbstractFileEngine::CanonicalPathName);
					flags |= VPS_BIT_FLAG[i];
				}
				else
				{
					qWarning("VapourSynth %u-Bit edition was found, but version check has failed!", VPS_BITNESS(i));
				}
			}
			else
			{
				qWarning("VapourSynth %u-Bit edition was found, but appears to be incomplete!", VPS_BITNESS(i));
			}
		}
		else
		{
			qDebug("VapourSynth %u-Bit edition *not* found!", VPS_BITNESS(i));
		}
	}

	return flags;
}

//-------------------------------------
// Internal functions
//-------------------------------------

VapourSynthCheckThread::VapourSynthFlags VapourSynthCheckThread::getVapourSynthType(const int scope)
{
	if (MUtils::OS::os_architecture() == MUtils::OS::ARCH_X64)
	{
		switch (scope)
		{
		case MUtils::Registry::scope_wow_x32:
			return VAPOURSYNTH_X86;
		case MUtils::Registry::scope_wow_x64:
			return VAPOURSYNTH_X64;
		default:
			return VAPOURSYNTH_DEF;
		}
	}
	else
	{
		return VAPOURSYNTH_X86; /*ignore scope on 32-Bit OS*/
	}
}

bool VapourSynthCheckThread::isVapourSynthComplete(const QFileInfo& vpsDllInfo, const QFileInfo& vpsExeInfo, QFile*& vpsExeFile, QFile*& vpsDllFile)
{
	bool complete = false;
	vpsExeFile = vpsDllFile = NULL;
	
	qDebug("VapourSynth EXE: %s", vpsExeInfo.absoluteFilePath().toUtf8().constData());
	qDebug("VapourSynth DLL: %s", vpsDllInfo.absoluteFilePath().toUtf8().constData());

	if (vpsDllInfo.exists() && vpsDllInfo.isFile() && vpsExeInfo.exists() && vpsExeInfo.isFile())
	{
		vpsExeFile = new QFile(vpsExeInfo.canonicalFilePath());
		vpsDllFile = new QFile(vpsDllInfo.canonicalFilePath());
		if(vpsExeFile->open(QIODevice::ReadOnly) && vpsDllFile->open(QIODevice::ReadOnly))
		{
			complete = MUtils::OS::is_executable_file(vpsExeFile->fileEngine()->fileName(QAbstractFileEngine::CanonicalName));
		}
	}

	if(!complete)
	{
		MUTILS_DELETE(vpsExeFile);
		MUTILS_DELETE(vpsDllFile);
	}

	return complete;
}

bool VapourSynthCheckThread::checkVapourSynth(const QString &vspipePath)
{
	//Try to run VSPIPE.EXE
	const QStringList output = runProcess(vspipePath, QStringList() << "--version");

	//Init regular expressions
	QRegExp vpsLogo("VapourSynth\\s+Video\\s+Processing\\s+Library");

	//Check for version info
	bool vapoursynthLogo = false;
	for(QStringList::ConstIterator iter = output.constBegin(); iter != output.constEnd(); iter++)
	{
		if(vpsLogo.lastIndexIn(*iter) >= 0)
		{
			vapoursynthLogo = true;
			break;
		}
	}

	//Minimum required version found?
	if(vapoursynthLogo)
	{
		qDebug("VapourSynth version was detected successfully.");
		return true;
	}

	//Failed to determine version
	qWarning("Failed to determine VapourSynth version!");
	return false;
}
