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
static const char* const VPS_REG_KEYS = "SOFTWARE\\VapourSynth";
static const char* const VPS_REG_NAME = "Path";

//Default VapurSynth architecture
#if _WIN64 || __x86_64__
#define VAPOURSYNTH_DEF VAPOURSYNTH_X64
#else
#define VAPOURSYNTH_DEF VAPOURSYNTH_X86;
#endif

//Enable detection of "portabel" edition?
#define ENABLE_PORTABLE_VPS true

//-------------------------------------
// Auxilary functions
//-------------------------------------

#define VALID_DIR(STR) ((!(STR).isEmpty()) && QDir((STR)).exists())
#define BOOLIFY(X) ((X) ? '1' : '0')
#define VPS_BITNESS(X) (((X) + 1U) * 32U)

static inline QString& cleanDir(QString& path)
{
	if (!path.isEmpty())
	{
		path = QDir::fromNativeSeparators(path);
		while (path.endsWith('/'))
		{
			path.chop(1);
		}
	}
	return path;
}

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
	static const MUtils::Registry::reg_scope_t REG_SCOPE[3] =
	{
		MUtils::Registry::scope_wow_x32,
		MUtils::Registry::scope_wow_x64,
		MUtils::Registry::scope_default
	};

	QHash<int, QString> vapoursynthPath;
	int flags = 0;

	//Look for "portable" VapourSynth version
	for (size_t i = 0; i < 2U; i++)
	{
		const QString vpsPortableDir = QString("%1/extra/VapourSynth-%2").arg(QCoreApplication::applicationDirPath(), QString::number(VPS_BITNESS(i)));
		if (VALID_DIR(vpsPortableDir))
		{
			const QFileInfo vpsPortableFile = QFileInfo(QString("%1/vspipe.exe").arg(vpsPortableDir));
			if (vpsPortableFile.exists() && vpsPortableFile.isFile())
			{
				vapoursynthPath.insert(VPS_BIT_FLAG[i], vpsPortableDir);
			}
		}
	}

	//Read VapourSynth path from the registry
	if (vapoursynthPath.isEmpty())
	{
		for (size_t i = 0; i < 3U; i++)
		{
			if (MUtils::Registry::reg_key_exists(MUtils::Registry::root_machine, QString::fromLatin1(VPS_REG_KEYS), REG_SCOPE[i]))
			{
				QString vpsInstallPath;
				if (MUtils::Registry::reg_value_read(MUtils::Registry::root_machine, QString::fromLatin1(VPS_REG_KEYS), QString::fromLatin1(VPS_REG_NAME), vpsInstallPath, REG_SCOPE[i]))
				{
					if (VALID_DIR(vpsInstallPath))
					{
						const QString vpsCorePath = QString("%1/core").arg(cleanDir(vpsInstallPath));
						if (VALID_DIR(vpsCorePath))
						{
							const int flag = getVapourSynthType(REG_SCOPE[i]);
							if (!vapoursynthPath.contains(flag))
							{
								vapoursynthPath.insert(flag, vpsCorePath);
							}
						}
					}
				}
			}
		}
	}

	//Make sure VapourSynth directory does exist
	if(vapoursynthPath.isEmpty())
	{
		qWarning("VapourSynth install path not found -> disable VapouSynth support!");
		return 0;
	}

	//Validate the VapourSynth installation now!
	for (size_t i = 0; i < 2U; i++)
	{
		if (vapoursynthPath.contains(VPS_BIT_FLAG[i]))
		{
			const QString path = vapoursynthPath[VPS_BIT_FLAG[i]];
			qDebug("VapourSynth %u-Bit \"core\" path: %s", VPS_BITNESS(i), MUTILS_UTF8(path));
			QFile *vpsExeFile, *vpsDllFile;
			if (isVapourSynthComplete(path, vpsExeFile, vpsDllFile))
			{
				if (vpsExeFile && checkVapourSynth(vpsExeFile->fileName()))
				{
					qDebug("VapourSynth %u-Bit edition found!", VPS_BITNESS(i));
					m_vpsExePath[i].reset(vpsExeFile);
					m_vpsDllPath[i].reset(vpsDllFile);
					m_vpsPath[i] = path;
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

bool VapourSynthCheckThread::isVapourSynthComplete(const QString &vsCorePath, QFile *&vpsExeFile, QFile *&vpsDllFile)
{
	bool complete = false;
	vpsExeFile = vpsDllFile = NULL;

	QFileInfo vpsExeInfo(QString("%1/vspipe.exe"     ).arg(vsCorePath));
	QFileInfo vpsDllInfo(QString("%1/vapoursynth.dll").arg(vsCorePath));
	
	qDebug("VapourSynth EXE: %s", vpsExeInfo.absoluteFilePath().toUtf8().constData());
	qDebug("VapourSynth DLL: %s", vpsDllInfo.absoluteFilePath().toUtf8().constData());

	if(vpsExeInfo.exists() && vpsDllInfo.exists())
	{
		vpsExeFile = new QFile(vpsExeInfo.canonicalFilePath());
		vpsDllFile = new QFile(vpsDllInfo.canonicalFilePath());
		if(vpsExeFile->open(QIODevice::ReadOnly) && vpsDllFile->open(QIODevice::ReadOnly))
		{
			complete = MUtils::OS::is_executable_file(vpsExeFile->fileName());
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
