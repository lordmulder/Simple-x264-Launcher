///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2016 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "thread_avisynth.h"

//Qt
#include <QLibrary>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <QApplication>
#include <QProcess>
#include <QDir>

//Internal
#include "global.h"
#include "model_sysinfo.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/OSSupport.h>

//Const
static const bool ENABLE_PORTABLE_AVS = true;

//Static
QMutex AvisynthCheckThread::m_avsLock;
QScopedPointer<QFile> AvisynthCheckThread::m_avsDllPath[2];

//Helper
#define VALID_DIR(STR) ((!(STR).isEmpty()) && QDir((STR)).exists())
#define BOOLIFY(X) ((X) ? '1' : '0')

//Utility function
QString AVS_CHECK_BINARY(const SysinfoModel *sysinfo, const bool& x64)
{
	return QString("%1/toolset/%2/avs_check_%2.exe").arg(sysinfo->getAppPath(), (x64 ? "x64": "x86"));
}

class Wow64RedirectionDisabler
{
public:
	Wow64RedirectionDisabler(void)
	{
		m_oldValue = NULL;
		m_disabled = MUtils::OS::wow64fsredir_disable(m_oldValue);
	}
	~Wow64RedirectionDisabler(void)
	{
		if(m_disabled)
		{
			if(!MUtils::OS::wow64fsredir_revert(m_oldValue))
			{
				qWarning("Failed to renable WOW64 filesystem redirection!");
			}
		}
	}
private:
	bool  m_disabled;
	void* m_oldValue;
};

//-------------------------------------
// External API
//-------------------------------------

bool AvisynthCheckThread::detect(SysinfoModel *sysinfo)
{
	sysinfo->clearAvisynth();
	double version = 0.0;
	QMutexLocker lock(&m_avsLock);

	QEventLoop loop;
	AvisynthCheckThread thread(sysinfo);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(15000, &loop, SLOT(quit()));
	
	qDebug("Avisynth thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("Avisynth thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(1000))
	{
		qWarning("Avisynth thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return false;
	}

	if(thread.getException())
	{
		qWarning("Avisynth thread encountered an exception !!!");
		return false;
	}
	
	if(thread.getSuccess())
	{
		sysinfo->setAvisynth(SysinfoModel::Avisynth_X86, thread.getSuccess() & AVISYNTH_X86);
		sysinfo->setAvisynth(SysinfoModel::Avisynth_X64, thread.getSuccess() & AVISYNTH_X64);
		sysinfo->setAVSPath(thread.getPath());
		qDebug("Avisynth support is officially enabled now! [x86=%c, x64=%c]", BOOLIFY(sysinfo->getAvisynth(SysinfoModel::Avisynth_X86)), BOOLIFY(sysinfo->getAvisynth(SysinfoModel::Avisynth_X64)));
	}
	else
	{
		qWarning("Avisynth could not be found -> Avisynth support disabled!");
	}

	return true;
}

//-------------------------------------
// Thread class
//-------------------------------------

AvisynthCheckThread::AvisynthCheckThread(const SysinfoModel *const sysinfo)
:
	m_sysinfo(sysinfo)
{
	m_success = false;
	m_exception = false;
}

AvisynthCheckThread::~AvisynthCheckThread(void)
{
}

void AvisynthCheckThread::run(void)
{
	m_exception = false;
	m_success &= 0;
	m_basePath.clear();

	detectAvisynthVersion1(m_success, m_basePath, m_sysinfo, &m_exception);
}

void AvisynthCheckThread::detectAvisynthVersion1(int &success, QString &basePath, const SysinfoModel *const sysinfo, volatile bool *exception)
{
	__try
	{
		detectAvisynthVersion2(success, basePath, sysinfo, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in Avisynth thread !!!");
	}
}

void AvisynthCheckThread::detectAvisynthVersion2(int &success, QString &basePath, const SysinfoModel *const sysinfo, volatile bool *exception)
{
	try
	{
		return detectAvisynthVersion3(success, basePath, sysinfo);
	}
	catch(...)
	{
		*exception = true;
		qWarning("Avisynth initializdation raised an C++ exception!");
	}
}

void AvisynthCheckThread::detectAvisynthVersion3(int &success, QString &basePath, const SysinfoModel *const sysinfo)
{
	success &= 0;

	QFile *avsPath32;
	if(checkAvisynth(basePath, sysinfo, avsPath32, false))
	{
		m_avsDllPath[0].reset(avsPath32);
		success |= AVISYNTH_X86;
		qDebug("Avisynth 32-Bit edition found!");
	}
	else
	{
		qDebug("Avisynth 32-Bit edition *not* found!");
	}

	if(sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64))
	{
		QFile *avsPath64;
		if(checkAvisynth(basePath, sysinfo, avsPath64, true))
		{
			m_avsDllPath[1].reset(avsPath64);
			success |= AVISYNTH_X64;
			qDebug("Avisynth 64-Bit edition found!");
		}
		else
		{
			qDebug("Avisynth 64-Bit edition *not* found!");
		}
	}
	else
	{
		qWarning("Skipping 64-Bit Avisynth check on non-x64 system!");
	}
}

bool AvisynthCheckThread::checkAvisynth(QString &basePath, const SysinfoModel *const sysinfo, QFile *&path, const bool &x64)
{
	qDebug("Avisynth %s-Bit support is being tested.", x64 ? "64" : "32");

	QProcess process;
	QStringList output;
	QString extraPath;

	//Look for "portable" Avisynth version
	if (ENABLE_PORTABLE_AVS)
	{
		const QString avsPortableDir = QString("%1/extra/Avisynth").arg(QCoreApplication::applicationDirPath());
		if (VALID_DIR(avsPortableDir))
		{
			const QString archDir = x64 ? QLatin1String("x64") : QLatin1String("x86");
			QFileInfo avsDllFile(QString("%1/%2/avisynth.dll").arg(avsPortableDir, archDir)), devilDllFile(QString("%1/%2/devil.dll").arg(avsPortableDir, archDir));
			if (avsDllFile.exists() && devilDllFile.exists() && avsDllFile.isFile() && devilDllFile.isFile())
			{
				qWarning("Adding portable Avisynth to PATH environment variable: %s", MUTILS_UTF8(avsPortableDir));
				basePath = avsPortableDir;
				extraPath = QString("%1/%2").arg(avsPortableDir, archDir);
			}
		}
	}

	//Setup process object
	MUtils::init_process(process, QDir::tempPath(), true, extraPath);

	//Try to start VSPIPE.EXE
	process.start(AVS_CHECK_BINARY(sysinfo, x64), QStringList());
	if(!process.waitForStarted())
	{
		qWarning("Failed to launch AVS_CHECK.EXE -> %s", process.errorString().toUtf8().constData());
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
			qWarning("AVS_CHECK.EXE process encountered a deadlock -> aborting now!");
			break;
		}
	}

	//Make sure VSPIPE.EXE has terminated!
	process.waitForFinished(2500);
	if(process.state() != QProcess::NotRunning)
	{
		qWarning("AVS_CHECK.EXE process still running, going to kill it!");
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
		qWarning("AVS_CHECK.EXE failed with code 0x%08X -> disable Avisynth support!", process.exitCode());
		return false;
	}

	//Init regular expressions
	QRegExp avsLogo("Avisynth\\s+Checker\\s+(x86|x64)");
	QRegExp avsPath("Avisynth_DLLPath=(.+)");
	QRegExp avsVers("Avisynth_Version=(\\d+)\\.(\\d+)");
	
	//Check for version info
	bool avisynthLogo = false;
	quint32 avisynthVersion[2] = { 0, 0 };
	QString avisynthPath;
	for(QStringList::ConstIterator iter = output.constBegin(); iter != output.constEnd(); iter++)
	{
		if(avisynthLogo)
		{
			if(avsPath.indexIn(*iter) >= 0)
			{
				avisynthPath =  avsPath.cap(1).trimmed();
			}
			else if(avsVers.indexIn(*iter) >= 0)
			{
				quint32 temp[2];
				if(MUtils::regexp_parse_uint32(avsVers, temp, 2))
				{
					avisynthVersion[0] = temp[0];
					avisynthVersion[1] = temp[1];
				}
			}
		}
		else
		{
			if(avsLogo.lastIndexIn(*iter) >= 0)
			{
				avisynthLogo = true;
			}
		}
	}
	
	//Minimum required version found?
	if((avisynthVersion[0] >= 2) && (avisynthVersion[1] >= 50) && (!avisynthPath.isEmpty()))
	{
		Wow64RedirectionDisabler disableWow64Redir;
		path = new QFile(avisynthPath);
		if(!path->open(QIODevice::ReadOnly))
		{
			MUTILS_DELETE(path);
		}
		qDebug("Avisynth was detected successfully (current version: %u.%02u).", avisynthVersion[0], avisynthVersion[1]);
		qDebug("Avisynth DLL path: %s", MUTILS_UTF8(avisynthPath));
		return true;
	}
	
	//Failed to determine version
	qWarning("Failed to determine Avisynth version!");
	return false;
}
