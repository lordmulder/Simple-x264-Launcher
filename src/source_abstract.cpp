///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "source_abstract.h"

//Internal
#include "global.h"
#include "model_sysinfo.h"
#include "model_options.h"
#include "model_preferences.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/OSSupport.h>
#include <MUtils/Exception.h>

//Qt
#include <QProcess>
#include <QTextCodec>
#include <QDir>
#include <QPair>

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

AbstractSource::AbstractSource(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile)
:
	AbstractTool(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause),
	m_sourceFile(sourceFile)
{
	/*Nothing to do here*/
}

AbstractSource::~AbstractSource(void)
{
	/*Nothing to do here*/
}

// ------------------------------------------------------------
// Check Source Properties
// ------------------------------------------------------------

bool AbstractSource::checkSourceProperties(ClipInfo &clipInfo)
{
	QStringList cmdLine;
	QList<QRegExp*> patterns;
	QProcess process;

	checkSourceProperties_init(patterns, cmdLine);

	log("Creating process:");
	if(!startProcess(process, getBinaryPath(), cmdLine, true, &getExtraPaths(), &getExtraEnv()))
	{
		return false;;
	}
	
	QTextCodec *localCodec = QTextCodec::codecForName("System");

	bool bTimeout = false;
	bool bAborted = false;
	
	clipInfo.reset();
		
	unsigned int waitCounter = 0;

	while(process.state() != QProcess::NotRunning)
	{
		if(*m_abort)
		{
			process.kill();
			bAborted = true;
			break;
		}
		if(!process.waitForReadyRead(m_processTimeoutInterval))
		{
			if(process.state() == QProcess::Running)
			{
				if(++waitCounter > m_processTimeoutMaxCounter)
				{
					if(m_preferences->getAbortOnTimeout())
					{
						process.kill();
						qWarning("Source process timed out <-- killing!");
						log("\nPROCESS TIMEOUT !!!");
						log("\nInput process has encountered a deadlock or your script takes EXTREMELY long to initialize!");
						bTimeout = true;
						break;
					}
				}
				else if(waitCounter == m_processTimeoutWarning)
				{
					unsigned int timeOut = (waitCounter * m_processTimeoutInterval) / 1000U;
					log(tr("Warning: Input process did not respond for %1 seconds, potential deadlock...").arg(QString::number(timeOut)));
				}
			}
			continue;
		}
		
		waitCounter = 0;
		PROCESS_PENDING_LINES(process, checkSourceProperties_parseLine, patterns, clipInfo);
	}

	if(!(bTimeout || bAborted))
	{
		PROCESS_PENDING_LINES(process, checkSourceProperties_parseLine, patterns, clipInfo);
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}

	while(!patterns.isEmpty())
	{
		QRegExp *pattern = patterns.takeFirst();
		MUTILS_DELETE(pattern);
	}

	if(bTimeout || bAborted || process.exitCode() != EXIT_SUCCESS)
	{
		if(!(bTimeout || bAborted))
		{
			const int exitCode = process.exitCode();
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(exitCode)));
			if((exitCode < 0) || (exitCode >= 32))
			{
				log(tr("\nIMPORTANT: The input process terminated abnormally. This means Avisynth/VapourSynth or one of its plugins crashed!"));
			}
		}
		return false;
	}

	if(clipInfo.getFrameCount() < 1)
	{
		log(tr("\nFAILED TO DETERMINE CLIP PROPERTIES !!!"));
		return false;
	}
	
	log("");

	const QPair<quint32, quint32> frameSize = clipInfo.getFrameSize();
	if((frameSize.first > 0) && (frameSize.second > 0))
	{
		log(tr("Resolution: %1 x %2").arg(QString::number(frameSize.first), QString::number(frameSize.second)));
	}

	const QPair<quint32, quint32> frameRate = clipInfo.getFrameRate();
	if((frameRate.first > 0) && (frameRate.second > 0))
	{
		log(tr("Frame Rate: %1/%2").arg(QString::number(frameRate.first), QString::number(frameRate.second)));
	}
	else if(frameRate.first > 0)
	{
		log(tr("Frame Rate: %1").arg(QString::number(frameRate.first)));
	}

	log(tr("No. Frames: %1").arg(QString::number(clipInfo.getFrameCount())));
	return true;
}

// ------------------------------------------------------------
// Source Processing
// ------------------------------------------------------------

bool AbstractSource::createProcess(QProcess &processEncode, QProcess&processInput)
{
	processInput.setStandardOutputProcess(&processEncode);
	
	QStringList cmdLine_Input;
	buildCommandLine(cmdLine_Input);

	log("Creating input process:");
	if(!startProcess(processInput, getBinaryPath(), cmdLine_Input, false, &getExtraPaths(), &getExtraEnv()))
	{
		return false;
	}

	return true;
}

// ------------------------------------------------------------
// Source Info
// ------------------------------------------------------------

const AbstractSourceInfo& AbstractSource::getSourceInfo(void)
{
	MUTILS_THROW("[getSourceInfo] This function must be overwritten in sub-classes!");
}

// ------------------------------------------------------------
// Auxiliary FUnctions
// ------------------------------------------------------------

QHash<QString, QString> AbstractSource::getExtraEnv(void) const
{
	QHash<QString, QString> extraEnv;

	const QString profilePath = MUtils::OS::known_folder(MUtils::OS::FOLDER_PROFILE_USER);
	if (!profilePath.isEmpty())
	{
		extraEnv.insert("USERPROFILE", QDir::toNativeSeparators(profilePath));
	}

	const QString appDataPath = MUtils::OS::known_folder(MUtils::OS::FOLDER_APPDATA_ROAM);
	if (!appDataPath.isEmpty())
	{
		extraEnv.insert("APPDATA", QDir::toNativeSeparators(appDataPath));
	}

	return extraEnv;
}