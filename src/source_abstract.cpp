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

#include "source_abstract.h"

#include "global.h"
#include "model_sysinfo.h"
#include "model_options.h"
#include "model_preferences.h"

#include <QProcess>
#include <QTextCodec>
#include <QDir>

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

bool AbstractSource::checkSourceProperties(unsigned int &frames)
{
	QStringList cmdLine;
	QList<QRegExp*> patterns;
	QProcess process;

	checkSourceProperties_init(patterns, cmdLine);

	log("Creating process:");
	if(!startProcess(process, getBinaryPath(), cmdLine))
	{
		return false;;
	}
	
	QTextCodec *localCodec = QTextCodec::codecForName("System");

	bool bTimeout = false;
	bool bAborted = false;

	frames = 0;
	
	unsigned int fpsNom = 0;
	unsigned int fpsDen = 0;
	unsigned int fSizeW = 0;
	unsigned int fSizeH = 0;
	
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
		PROCESS_PENDING_LINES(process, checkSourceProperties_parseLine, patterns, frames, fSizeW, fSizeH, fpsNom, fpsDen);
	}

	if(!(bTimeout || bAborted))
	{
		PROCESS_PENDING_LINES(process, checkSourceProperties_parseLine, patterns, frames, fSizeW, fSizeH, fpsNom, fpsDen);
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
		X264_DELETE(pattern);
	}

	if(bTimeout || bAborted || process.exitCode() != EXIT_SUCCESS)
	{
		if(!(bTimeout || bAborted))
		{
			const int exitCode = process.exitCode();
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(exitCode)));
			if((exitCode < 0) || (exitCode >= 32))
			{
				log(tr("\nIMPORTANT: The Avs2YUV process terminated abnormally. This means Avisynth or one of your Avisynth-Plugin's just crashed."));
				log(tr("IMPORTANT: Please fix your Avisynth script and try again! If you use Avisynth-MT, try using a *stable* Avisynth instead!"));
			}
		}
		return false;
	}

	if(frames == 0)
	{
		log(tr("\nFAILED TO DETERMINE AVS PROPERTIES !!!"));
		return false;
	}
	
	log("");

	if((fSizeW > 0) && (fSizeH > 0))
	{
		log(tr("Resolution: %1x%2").arg(QString::number(fSizeW), QString::number(fSizeH)));
	}
	if((fpsNom > 0) && (fpsDen > 0))
	{
		log(tr("Frame Rate: %1/%2").arg(QString::number(fpsNom), QString::number(fpsDen)));
	}
	if((fpsNom > 0) && (fpsDen == 0))
	{
		log(tr("Frame Rate: %1").arg(QString::number(fpsNom)));
	}
	if(frames > 0)
	{
		log(tr("No. Frames: %1").arg(QString::number(frames)));
	}

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
	if(!startProcess(processInput, getBinaryPath(), cmdLine_Input, false))
	{
		return false;
	}

	return true;
}
