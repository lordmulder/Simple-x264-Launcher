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

#include "encoder_abstract.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "model_status.h"
#include "source_abstract.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/Exception.h>
#include <MUtils/OSSupport.h>

//Qt
#include <QProcess>
#include <QDir>
#include <QTextCodec>
#include <QSemaphore>
#include <QDate>
#include <QTime>
#include <QThread>
#include <QLocale>

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

AbstractEncoder::AbstractEncoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile)
:
	AbstractTool(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause),
	m_sourceFile(sourceFile),
	m_outputFile(outputFile),
	m_indexFile(QString("%1/~%2.ffindex").arg(QDir::tempPath(), stringToHash(m_sourceFile)))
{
	/*Nothing to do here*/
}

AbstractEncoder::~AbstractEncoder(void)
{
	/*Nothing to do here*/
}

// ------------------------------------------------------------
// Encoding Functions
// ------------------------------------------------------------

bool AbstractEncoder::runEncodingPass(AbstractSource* pipedSource, const QString outputFile, const ClipInfo &clipInfo, const int &pass, const QString &passLogFile)
{
	QProcess processEncode, processInput;
	
	if(pipedSource)
	{
		pipedSource->createProcess(processEncode, processInput);
	}

	QStringList cmdLine_Encode;
	buildCommandLine(cmdLine_Encode, (pipedSource != NULL), clipInfo, m_indexFile, pass, passLogFile);

	log("Creating encoder process:");
	if(!startProcess(processEncode, getBinaryPath(), cmdLine_Encode, true, &getExtraPaths()))
	{
		return false;
	}

	QList<QRegExp*> patterns;
	runEncodingPass_init(patterns);
	
	double last_progress = 0.0;
	double size_estimate = 0.0;
	
	bool bTimeout = false;
	bool bAborted = false;

	//Main processing loop
	while(processEncode.state() != QProcess::NotRunning)
	{
		unsigned int waitCounter = 0;

		//Wait until new output is available
		forever
		{
			if(*m_abort)
			{
				processEncode.kill();
				processInput.kill();
				bAborted = true;
				break;
			}
			if((*m_pause) && (processEncode.state() == QProcess::Running))
			{
				JobStatus previousStatus = m_jobStatus;
				setStatus(JobStatus_Paused);
				log(tr("Job paused by user at %1, %2.").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
				bool ok[2] = {false, false};
				QProcess *proc[2] = { &processEncode, &processInput };
				ok[0] = MUtils::OS::suspend_process(proc[0], true);
				ok[1] = MUtils::OS::suspend_process(proc[1], true);
				while(*m_pause) m_semaphorePause->tryAcquire(1, 5000);
				while(m_semaphorePause->tryAcquire(1, 0));
				ok[0] = MUtils::OS::suspend_process(proc[0], false);
				ok[1] = MUtils::OS::suspend_process(proc[1], false);
				if(!(*m_abort)) setStatus(previousStatus);
				log(tr("Job resumed by user at %1, %2.").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
				waitCounter = 0;
				continue;
			}
			if(!processEncode.waitForReadyRead(m_processTimeoutInterval))
			{
				if(processEncode.state() == QProcess::Running)
				{
					if(++waitCounter > m_processTimeoutMaxCounter)
					{
						if(m_preferences->getAbortOnTimeout())
						{
							processEncode.kill();
							qWarning("encoder process timed out <-- killing!");
							log("\nPROCESS TIMEOUT !!!");
							bTimeout = true;
							break;
						}
					}
					else if(waitCounter == m_processTimeoutWarning)
					{
						unsigned int timeOut = (waitCounter * m_processTimeoutInterval) / 1000U;
						log(tr("Warning: encoder did not respond for %1 seconds, potential deadlock...").arg(QString::number(timeOut)));
					}
					continue;
				}
			}
			if((*m_abort) || ((*m_pause) && (processEncode.state() == QProcess::Running)))
			{
				continue;
			}
			break;
		}
		
		//Exit main processing loop now?
		if(bAborted || bTimeout)
		{
			break;
		}

		//Process all output
		PROCESS_PENDING_LINES(processEncode, runEncodingPass_parseLine, patterns, clipInfo, pass, last_progress, size_estimate);
	}
	
	if(!(bTimeout || bAborted))
	{
		PROCESS_PENDING_LINES(processEncode, runEncodingPass_parseLine, patterns, clipInfo, pass, last_progress, size_estimate);
	}

	processEncode.waitForFinished(5000);
	if(processEncode.state() != QProcess::NotRunning)
	{
		qWarning("Encoder process still running, going to kill it!");
		processEncode.kill();
		processEncode.waitForFinished(-1);
	}
	
	if(pipedSource)
	{
		processInput.waitForFinished(5000);
		if(processInput.state() != QProcess::NotRunning)
		{
			qWarning("Input process still running, going to kill it!");
			processInput.kill();
			processInput.waitForFinished(-1);
		}
		if(!(bTimeout || bAborted))
		{
			pipedSource->flushProcess(processInput);
		}
	}

	while(!patterns.isEmpty())
	{
		QRegExp *pattern = patterns.takeFirst();
		MUTILS_DELETE(pattern);
	}

	if(bTimeout || bAborted || processEncode.exitCode() != EXIT_SUCCESS)
	{
		if(!(bTimeout || bAborted))
		{
			const int exitCode = processEncode.exitCode();
			if((exitCode < -1) || (exitCode >= 32))
			{
				log(tr("\nFATAL ERROR: The encoder process has *crashed* -> your encode probably is *incomplete* !!!"));
				log(tr("Note that this indicates a bug in the current encoder, *not* in Simple x264/x265 Launcher."));
			}
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(exitCode)));
		}
		processEncode.close();
		processInput.close();
		return false;
	}

	QThread::yieldCurrentThread();

	QFileInfo completedFileInfo(m_outputFile);
	const qint64 finalSize = (completedFileInfo.exists() && completedFileInfo.isFile()) ? completedFileInfo.size() : 0;
	log(tr("Final file size is %1 bytes.").arg(sizeToString(finalSize)));

	switch(pass)
	{
	case 1:
		setStatus(JobStatus_Running_Pass1);
		setDetails(tr("First pass completed. Preparing for second pass..."));
		break;
	case 2:
		setStatus(JobStatus_Running_Pass2);
		setDetails(tr("Second pass completed successfully. Final size is %1.").arg(sizeToString(finalSize)));
		break;
	default:
		setStatus(JobStatus_Running);
		setDetails(tr("Encode completed successfully. Final size is %1.").arg(sizeToString(finalSize)));
		break;
	}

	setProgress(100);
	processEncode.close();
	processInput.close();
	return true;
}

// ------------------------------------------------------------
// Utilities
// ------------------------------------------------------------

double AbstractEncoder::estimateSize(const QString &fileName, const double &progress)
{
	double estimatedSize = 0.0;
	if(progress >= 0.03)
	{
		QFileInfo fileInfo(fileName);
		if(fileInfo.exists() && fileInfo.isFile())
		{
			const qint64 currentSize = QFileInfo(fileName).size();
			estimatedSize = static_cast<double>(currentSize) * (1.0 / qBound(0.0, progress, 1.0));
		}
	}
	return estimatedSize;
}

QString AbstractEncoder::sizeToString(qint64 size)
{
	static char *prefix[5] = {"Byte", "KB", "MB", "GB", "TB"};

	if(size > 1024I64)
	{
		qint64 estimatedSize = size;
		qint64 remainderSize = 0I64;

		int prefixIdx = 0;
		while((estimatedSize > 1024I64) && (prefixIdx < 4))
		{
			remainderSize = estimatedSize % 1024I64;
			estimatedSize = estimatedSize / 1024I64;
			prefixIdx++;
		}
			
		double value = static_cast<double>(estimatedSize) + (static_cast<double>(remainderSize) / 1024.0);
		return QString().sprintf((value < 10.0) ? "%.2f %s" : "%.1f %s", value, prefix[prefixIdx]);
	}

	return tr("N/A");
}

// ------------------------------------------------------------
// Encoder Info
// ------------------------------------------------------------

template <class T>
static T getElementAt(const QList<T> &list, const quint32 &index)
{
	if (index >= quint32(list.count()))
	{
		MUTILS_THROW("Index is out of bounds!");
	}
	return list[index];
}

QStringList AbstractEncoderInfo::getDependencies(const SysinfoModel *sysinfo, const quint32 &encArch, const quint32 &encVariant) const
{
	return QStringList();
}

QString AbstractEncoderInfo::getFullName(const quint32 &encArch, const quint32 &encVariant) const
{
	return QString("%1, %2, %3").arg(getName(), archToString(encArch), variantToString(encVariant));
}

QString AbstractEncoderInfo::archToString(const quint32 &index) const
{
	return getElementAt(getArchitectures(), index).first;
}

AbstractEncoderInfo::ArchBit AbstractEncoderInfo::archToType(const quint32 &index) const
{
	return getElementAt(getArchitectures(), index).second;
}

QString AbstractEncoderInfo::variantToString(const quint32 &index) const
{
	return getElementAt(getVariants(), index);
}

QString AbstractEncoderInfo::rcModeToString(const quint32 &index) const
{
	return getElementAt(getRCModes(), index).first;
}

AbstractEncoderInfo::RCType AbstractEncoderInfo::rcModeToType(const quint32 &index) const
{
	return getElementAt(getRCModes(), index).second;
}
