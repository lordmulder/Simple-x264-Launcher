///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2018 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <QLibrary>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <QApplication>
#include <QDir>
#include <QProcess>

//Internal
#include "global.h"
#include "model_sysinfo.h"

//CRT
#include <cassert>

//Const
static const bool ENABLE_PORTABLE_VPS = true;

//Static
QMutex VapourSynthCheckThread::m_vpsLock;
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];

#define VALID_DIR(STR) ((!(STR).isEmpty()) && QDir((STR)).exists())
#define BOOLIFY(X) ((X) ? '1' : '0')
#define VPS_BITNESS(X) (((X) + 1U) * 32U)

static inline QString &cleanDir(QString &path)
{
	if(!path.isEmpty())
	{
		path = QDir::fromNativeSeparators(path);
		while(path.endsWith('/'))
		{
			path.chop(1);
		}
	}
	return path;
}

//-------------------------------------
// External API
//-------------------------------------

bool VapourSynthCheckThread::detect(SysinfoModel *sysinfo)
{
	sysinfo->clearVapourSynth();
	sysinfo->clearVPSPath();
	
	QMutexLocker lock(&m_vpsLock);

	QEventLoop loop;
	VapourSynthCheckThread thread;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(15000, &loop, SLOT(quit()));
	
	qDebug("VapourSynth thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("VapourSynth thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(1000))
	{
		qWarning("VapourSynth thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return false;
	}

	if(thread.getException())
	{
		qWarning("VapourSynth thread encountered an exception !!!");
		return false;
	}

	if(thread.getSuccess())
	{
		sysinfo->setVapourSynth(SysinfoModel::VapourSynth_X86, thread.getSuccess() & VAPOURSYNTH_X86);
		sysinfo->setVapourSynth(SysinfoModel::VapourSynth_X64, thread.getSuccess() & VAPOURSYNTH_X64);
		sysinfo->setVPSPath(thread.getPath());
		qDebug("VapourSynth support is officially enabled now! [x86=%c, x64=%c]", BOOLIFY(sysinfo->getVapourSynth(SysinfoModel::VapourSynth_X86)), BOOLIFY(sysinfo->getVapourSynth(SysinfoModel::VapourSynth_X64)));
	}
	else
	{
		qWarning("VapourSynth could not be found -> VapourSynth support disabled!");
	}

	return true;
}

//-------------------------------------
// Thread class
//-------------------------------------

VapourSynthCheckThread::VapourSynthCheckThread(void)
{
	m_success &= 0;
	m_exception = false;
	m_vpsPath.clear();
}

VapourSynthCheckThread::~VapourSynthCheckThread(void)
{
}

void VapourSynthCheckThread::run(void)
{
	m_success &= 0;
	m_exception = false;
	m_vpsPath.clear();

	detectVapourSynthPath1(m_success, m_vpsPath, &m_exception);
}

void VapourSynthCheckThread::detectVapourSynthPath1(int &success, QString &path, volatile bool *exception)
{
	__try
	{
		return detectVapourSynthPath2(success, path, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in VapourSynth thread !!!");
	}
}

void VapourSynthCheckThread::detectVapourSynthPath2(int &success, QString &path, volatile bool *exception)
{
	try
	{
		return detectVapourSynthPath3(success, path);
	}
	catch(...)
	{
		*exception = true;
		qWarning("VapourSynth initializdation raised an C++ exception!");
	}
}

void VapourSynthCheckThread::detectVapourSynthPath3(int &success, QString &path)
{
	success &= 0;
	path.clear();

	static const char *VPS_CORE_DIR[] =
	{
		"core32",
		"core64",
		NULL
	};
	static const int VPS_BIT_FLAG[] =
	{
		VAPOURSYNTH_X86,
		VAPOURSYNTH_X64,
		NULL
	};
	static const char *VPS_REG_KEYS[] = 
	{
		"SOFTWARE\\VapourSynth",
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VapourSynth_is1",
		NULL
	};
	static const char *VPS_REG_NAME[] =
	{
		"Path",
		"InstallLocation",
		"Inno Setup: App Path",
		NULL
	};
	static const MUtils::Registry::reg_scope_t REG_SCOPE[3] =
	{
		MUtils::Registry::scope_default,
		MUtils::Registry::scope_wow_x32,
		MUtils::Registry::scope_wow_x64
	};

	QString vapoursynthPath;

	//Look for "portable" VapourSynth version
	if (ENABLE_PORTABLE_VPS)
	{
		const QString vpsPortableDir = QString("%1/extra/VapourSynth").arg(QCoreApplication::applicationDirPath());
		if (VALID_DIR(vpsPortableDir))
		{
			for (size_t i = 0; VPS_CORE_DIR[i]; i++)
			{
				const QFileInfo vpsPortableDll = QFileInfo(QString("%1/%2/VapourSynth.dll").arg(vpsPortableDir, QString::fromLatin1(VPS_CORE_DIR[i])));
				if (vpsPortableDll.exists() && vpsPortableDll.isFile())
				{
					vapoursynthPath = vpsPortableDir;
					break;
				}
			}
		}
	}

	//Read VapourSynth path from registry
	if (vapoursynthPath.isEmpty())
	{
		for (size_t i = 0; VPS_REG_KEYS[i]; i++)
		{
			for (size_t j = 0; VPS_REG_NAME[j]; j++)
			{
				for (size_t k = 0; k < 3; k++)
				{
					if (MUtils::Registry::reg_key_exists(MUtils::Registry::root_machine, QString::fromLatin1(VPS_REG_KEYS[i]), REG_SCOPE[k]))
					{
						QString temp;
						if (MUtils::Registry::reg_value_read(MUtils::Registry::root_machine, QString::fromLatin1(VPS_REG_KEYS[i]), QString::fromLatin1(VPS_REG_NAME[j]), temp, REG_SCOPE[k]))
						{
							temp = cleanDir(temp);
							if (VALID_DIR(temp))
							{
								vapoursynthPath = temp;
								break;
							}
						}
					}
				}
				if (!vapoursynthPath.isEmpty())
				{
					break;
				}
			}
			if (!vapoursynthPath.isEmpty())
			{
				break;
			}
		}
	}

	//Make sure VapourSynth directory does exist
	if(vapoursynthPath.isEmpty())
	{
		qWarning("VapourSynth install path not found -> disable VapouSynth support!");
		return;
	}

	//Validate the VapourSynth installation now!
	qDebug("VapourSynth Dir: %s", vapoursynthPath.toUtf8().constData());
	for (size_t i = 0; VPS_CORE_DIR[i]; i++)
	{
		QFile *vpsExeFile, *vpsDllFile;
		if (isVapourSynthComplete(QString("%1/%2").arg(vapoursynthPath, QString::fromLatin1(VPS_CORE_DIR[i])), vpsExeFile, vpsDllFile))
		{
			if (vpsExeFile && checkVapourSynth(vpsExeFile->fileName()))
			{
				success |= VPS_BIT_FLAG[i];
				qDebug("VapourSynth %u-Bit edition found!", VPS_BITNESS(i));
				m_vpsExePath[i].reset(vpsExeFile);
				m_vpsDllPath[i].reset(vpsDllFile);
			}
			else
			{
				qWarning("VapourSynth %u-Bit edition was found, but version check has failed!", VPS_BITNESS(i));
			}
		}
		else
		{
			qDebug("VapourSynth %u-Bit edition *not* found!", VPS_BITNESS(i));
		}
	}

	//Return VapourSynth path
	if(success)
	{
		path = vapoursynthPath;
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
	QProcess process;
	QStringList output;

	//Setup process object
	process.setWorkingDirectory(QDir::tempPath());
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setReadChannel(QProcess::StandardOutput);

	//Try to start VSPIPE.EXE
	process.start(vspipePath, QStringList() << "--version");
	if(!process.waitForStarted())
	{
		qWarning("Failed to launch VSPIPE.EXE -> %s", process.errorString().toUtf8().constData());
		return false;
	}

	//Wait for process to finish
	while(process.state() != QProcess::NotRunning)
	{
		if(process.waitForReadyRead(12000))
		{
			while(process.canReadLine())
			{
				output << QString::fromUtf8(process.readLine()).simplified();
			}
			continue;
		}
		if(process.state() != QProcess::NotRunning)
		{
			qWarning("VSPIPE.EXE process encountered a deadlock -> aborting now!");
			break;
		}
	}

	//Make sure VSPIPE.EXE has terminated!
	process.waitForFinished(2500);
	if(process.state() != QProcess::NotRunning)
	{
		qWarning("VSPIPE.EXE process still running, going to kill it!");
		process.kill();
		process.waitForFinished(-1);
	}

	//Read pending lines
	while(process.canReadLine())
	{
		output << QString::fromUtf8(process.readLine()).simplified();
	}

	//Check exit code
	if(process.exitCode() != 0)
	{
		qWarning("VSPIPE.EXE failed with code 0x%08X -> disable Vapousynth support!", process.exitCode());
		return false;
	}

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
