///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
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

//CRT
#include <cassert>

QMutex VapourSynthCheckThread::m_vpsLock;
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];

#define VALID_DIR(STR) ((!(STR).isEmpty()) && QDir((STR)).exists())

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

int VapourSynthCheckThread::detect(QString &path, int &vapourSynthType)
{
	path.clear();
	vapourSynthType = VAPOURSYNTH_OFF;
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
		return -1;
	}

	if(thread.getException())
	{
		qWarning("VapourSynth thread encountered an exception !!!");
		return -1;
	}
	
	if(thread.getSuccess() & (VAPOURSYNTH_X86 | VAPOURSYNTH_X64))
	{
		vapourSynthType = thread.getSuccess();
		path = thread.getPath();
		qDebug("VapourSynth check completed successfully.");
		return 1;
	}

	qWarning("VapourSynth thread failed to detect installation!");
	return 0;
}

//-------------------------------------
// Thread class
//-------------------------------------

VapourSynthCheckThread::VapourSynthCheckThread(void)
{
	m_success = VAPOURSYNTH_OFF;
	m_exception = false;
	m_vpsPath.clear();
}

VapourSynthCheckThread::~VapourSynthCheckThread(void)
{
}

void VapourSynthCheckThread::run(void)
{
	m_success = VAPOURSYNTH_OFF;
	m_exception = false;

	m_success = detectVapourSynthPath1(m_vpsPath, &m_exception);
}

int VapourSynthCheckThread::detectVapourSynthPath1(QString &path, volatile bool *exception)
{
	__try
	{
		return detectVapourSynthPath2(path, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in VapourSynth thread !!!");
		return false;
	}
}

int VapourSynthCheckThread::detectVapourSynthPath2(QString &path, volatile bool *exception)
{
	try
	{
		return detectVapourSynthPath3(path);
	}
	catch(...)
	{
		*exception = true;
		qWarning("VapourSynth initializdation raised an C++ exception!");
		return false;
	}
}

int VapourSynthCheckThread::detectVapourSynthPath3(QString &path)
{
	int success = VAPOURSYNTH_OFF;
	path.clear();

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

	//Read VapourSynth path from registry
	QString vapoursynthPath;
	for(size_t i = 0; VPS_REG_KEYS[i]; i++)
	{
		for(size_t j = 0; VPS_REG_NAME[j]; j++)
		{
			vapoursynthPath = cleanDir(x264_query_reg_string(false, VPS_REG_KEYS[i], VPS_REG_NAME[j]));
			if(VALID_DIR(vapoursynthPath)) break;
		}
		if(VALID_DIR(vapoursynthPath)) break;
	}

	//Make sure VapourSynth does exist
	if(!VALID_DIR(vapoursynthPath))
	{
		qWarning("VapourSynth install path not found -> disable VapouSynth support!");
		return VAPOURSYNTH_OFF;
	}

	qDebug("VapourSynth Dir: %s", vapoursynthPath.toUtf8().constData());

	//Look for 32-Bit edition of VapourSynth first
	QFile *vpsExeFile32, *vpsDllFile32;
	if(isVapourSynthComplete(QString("%1/core32").arg(vapoursynthPath), vpsExeFile32, vpsDllFile32))
	{
		if(vpsExeFile32 && checkVapourSynth(vpsExeFile32->fileName()))
		{
			success |= VAPOURSYNTH_X86;
			qDebug("VapourSynth 32-Bit edition found!");
			m_vpsExePath[0].reset(vpsExeFile32);
			m_vpsDllPath[0].reset(vpsDllFile32);
		}
		else
		{
			qWarning("VapourSynth 32-Bit edition was found, but version check has failed!");
		}
	}
	else
	{
		qDebug("VapourSynth 32-Bit edition *not* found!");
	}

	//Look for 64-Bit edition of VapourSynth next
	QFile *vpsExeFile64, *vpsDllFile64;
	if(isVapourSynthComplete(QString("%1/core64").arg(vapoursynthPath), vpsExeFile64, vpsDllFile64))
	{
		if(vpsExeFile64 && checkVapourSynth(vpsExeFile64->fileName()))
		{
			success |= VAPOURSYNTH_X64;
			qDebug("VapourSynth 64-Bit edition found!");
			m_vpsExePath[1].reset(vpsExeFile64);
			m_vpsDllPath[1].reset(vpsDllFile64);
		}
		else
		{
			qWarning("VapourSynth 64-Bit edition was found, but version check has failed!");
		}
	}
	else
	{
		qDebug("VapourSynth 64-Bit edition *not* found!");
	}

	//Return VapourSynth path
	if(success)
	{
		path = vapoursynthPath;
	}

	return success;
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
