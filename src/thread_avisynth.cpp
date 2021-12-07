///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2021 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <QEventLoop>
#include <QTimer>
#include <QApplication>
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

//-------------------------------------
// Auxilary functions
//-------------------------------------

#define VALID_DIR(STR) ((!(STR).isEmpty()) && QDir((STR)).exists())
#define BOOLIFY(X) ((X) ? '1' : '0')

QString AVS_CHECK_BINARY(const SysinfoModel *sysinfo, const bool& x64)
{
	return QString("%1/toolset/%2/avs_check_%2.exe").arg(sysinfo->getAppPath(), (x64 ? "x64": "x86"));
}

class Wow64RedirectionDisabler
{
public:
	Wow64RedirectionDisabler(void)
	{
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
	uintptr_t m_oldValue;
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
	QTimer::singleShot(30000, &loop, SLOT(quit()));
	
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
	m_basePath.clear();
}

AvisynthCheckThread::~AvisynthCheckThread(void)
{
}

void AvisynthCheckThread::run(void)
{
	m_basePath.clear();
	StarupThread::run();
}

int AvisynthCheckThread::threadMain(void)
{
	int flags = 0;

	QFile *avsPath32;
	if(checkAvisynth(m_basePath, m_sysinfo, avsPath32, false))
	{
		m_avsDllPath[0].reset(avsPath32);
		flags |= AVISYNTH_X86;
		qDebug("Avisynth 32-Bit edition found!");
	}
	else
	{
		qDebug("Avisynth 32-Bit edition *not* found!");
	}

	if(m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64))
	{
		QFile *avsPath64;
		if(checkAvisynth(m_basePath, m_sysinfo, avsPath64, true))
		{
			m_avsDllPath[1].reset(avsPath64);
			flags |= AVISYNTH_X64;
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

	return flags;
}

//-------------------------------------
// Internal functions
//-------------------------------------

bool AvisynthCheckThread::checkAvisynth(QString &basePath, const SysinfoModel *const sysinfo, QFile *&path, const bool &x64)
{
	qDebug("Avisynth %s-Bit support is being tested.", x64 ? "64" : "32");

	//Look for "portable" Avisynth version
	static const char *const ARCH_DIR[] = { "x64", "x86" };
	const QLatin1String archSuffix = QLatin1String(ARCH_DIR[x64 ? 1 : 0]);
	if (ENABLE_PORTABLE_AVS)
	{
		const QString avsPortableDir = QString("%1/extra/Avisynth").arg(QCoreApplication::applicationDirPath());
		if (VALID_DIR(avsPortableDir))
		{
			QFileInfo avsDllFile(QString("%1/%2/avisynth.dll").arg(avsPortableDir, archSuffix)), devilDllFile(QString("%1/%2/devil.dll").arg(avsPortableDir, archSuffix));
			if (avsDllFile.exists() && devilDllFile.exists() && avsDllFile.isFile() && devilDllFile.isFile())
			{
				qWarning("Adding portable Avisynth to PATH environment variable: %s", MUTILS_UTF8(avsPortableDir));
				basePath = avsPortableDir;
			}
		}
	}

	//Get extra paths
	QStringList avisynthExtraPaths;
	if (!basePath.isEmpty())
	{
		avisynthExtraPaths << QString("%1/%2").arg(basePath, archSuffix);
	}

	//Setup process object
	const QStringList output = runProcess(AVS_CHECK_BINARY(sysinfo, x64), QStringList(), &avisynthExtraPaths);

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
