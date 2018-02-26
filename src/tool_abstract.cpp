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

#include "tool_abstract.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "job_object.h"

//MUtils
#include <MUtils/OSSupport.h>

//Qt
#include <QProcess>
#include <QMutexLocker>
#include <QDir>
#include <QCryptographicHash>

QMutex AbstractTool::s_mutexStartProcess;

// ------------------------------------------------------------
// Helper Macros
// ------------------------------------------------------------

static void APPEND_AND_CLEAR(QStringList &list, QString &str)
{
	if(!str.isEmpty())
	{
		const QString temp = str.trimmed();
		if(!temp.isEmpty())
		{
			list << temp;
		}
		str.clear();
	}
}

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

AbstractTool::AbstractTool(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause)
:
	m_jobObject(jobObject),
	m_options(options),
	m_sysinfo(sysinfo),
	m_preferences(preferences),
	m_jobStatus(jobStatus),
	m_abort(abort),
	m_pause(pause),
	m_semaphorePause(semaphorePause)
{
	/*nothing to do here*/
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

unsigned int AbstractTool::checkVersion(bool &modified)
{
	if(m_preferences->getSkipVersionTest())
	{
		log("Warning: Skipping the version check this time!");
		return makeRevision(0xFFF0, 0xFFF0);
	}

	QProcess process;
	QList<QRegExp*> patterns;
	QStringList cmdLine;

	//Init encoder-specific values
	checkVersion_init(patterns, cmdLine);

	log("Creating process:");
	if(!startProcess(process, getBinaryPath(), cmdLine, true, &getExtraPaths()))
	{
		return false;
	}

	bool bTimeout = false;
	bool bAborted = false;

	unsigned int revision = UINT_MAX;
	unsigned int coreVers = UINT_MAX;
	modified = false;

	while(process.state() != QProcess::NotRunning)
	{
		if(*m_abort)
		{
			process.kill();
			bAborted = true;
			break;
		}
		if(!process.waitForReadyRead())
		{
			if(process.state() == QProcess::Running)
			{
				process.kill();
				qWarning("process timed out <-- killing!");
				log("\nPROCESS TIMEOUT !!!");
				bTimeout = true;
				break;
			}
		}
		PROCESS_PENDING_LINES(process, checkVersion_parseLine, patterns, coreVers, revision, modified);
	}

	if(!(bTimeout || bAborted))
	{
		PROCESS_PENDING_LINES(process, checkVersion_parseLine, patterns, coreVers, revision, modified);
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

	if(bTimeout || bAborted || (!checkVersion_succeeded(process.exitCode())))
	{
		if(!(bTimeout || bAborted))
		{
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(process.exitCode())));
		}
		return UINT_MAX;
	}

	if((revision == UINT_MAX) || (coreVers == UINT_MAX))
	{
		log(tr("\nFAILED TO DETERMINE VERSION INFO !!!"));
		return UINT_MAX;
	}
	
	return makeRevision(coreVers, revision);
}

bool AbstractTool::checkVersion_succeeded(const int &exitCode)
{
	return (exitCode == EXIT_SUCCESS);
}

// ------------------------------------------------------------
// Process Creation
// ------------------------------------------------------------

bool AbstractTool::startProcess(QProcess &process, const QString &program, const QStringList &args, bool mergeChannels, const QStringList *const extraPaths)
{
	QMutexLocker lock(&s_mutexStartProcess);
	log(commandline2string(program, args) + "\n");

	MUtils::init_process(process, QDir::tempPath(), true, extraPaths);
	if(!mergeChannels)
	{
		process.setProcessChannelMode(QProcess::SeparateChannels);
		process.setReadChannel(QProcess::StandardError);
	}

	process.start(program, args);
	
	if(process.waitForStarted())
	{
		m_jobObject->addProcessToJob(&process);
		MUtils::OS::change_process_priority(&process, m_preferences->getProcessPriority());
		lock.unlock();
		return true;
	}

	log("Process creation has failed :-(");
	QString errorMsg= process.errorString().trimmed();
	if(!errorMsg.isEmpty()) log(errorMsg);

	process.kill();
	process.waitForFinished(-1);
	return false;
}

// ------------------------------------------------------------
// Utilities
// ------------------------------------------------------------

QString AbstractTool::commandline2string(const QString &program, const QStringList &arguments)
{
	const QString nativeProgfram = QDir::toNativeSeparators(program);
	QString commandline = (nativeProgfram.contains(' ') ? QString("\"%1\"").arg(nativeProgfram) : nativeProgfram);
	
	for(int i = 0; i < arguments.count(); i++)
	{
		commandline += (arguments.at(i).contains(' ') ? QString(" \"%1\"").arg(arguments.at(i)) : QString(" %1").arg(arguments.at(i)));
	}

	return commandline;
}

QStringList AbstractTool::splitParams(const QString &params, const QString &sourceFile, const QString &outputFile)
{
	QStringList list; 
	bool ignoreWhitespaces = false;
	QString temp;

	for(int i = 0; i < params.length(); i++)
	{
		const QChar c = params.at(i);
		if(c == QLatin1Char('"'))
		{
			ignoreWhitespaces = (!ignoreWhitespaces);
			continue;
		}
		else if((!ignoreWhitespaces) && (c == QChar::fromLatin1(' ')))
		{
			APPEND_AND_CLEAR(list, temp);
			continue;
		}
		temp.append(c);
	}
	
	APPEND_AND_CLEAR(list, temp);

	if(!sourceFile.isEmpty())
	{
		list.replaceInStrings("$(INPUT)",  QDir::toNativeSeparators(sourceFile), Qt::CaseInsensitive);
	}

	if(!outputFile.isEmpty())
	{
		list.replaceInStrings("$(OUTPUT)", QDir::toNativeSeparators(outputFile), Qt::CaseInsensitive);
	}

	return list;
}

QString AbstractTool::stringToHash(const QString &string)
{
	QByteArray result(10, char(0));
	const QByteArray hash = QCryptographicHash::hash(string.toUtf8(), QCryptographicHash::Sha1);

	if((hash.size() == 20) && (result.size() == 10))
	{
		unsigned char *out = reinterpret_cast<unsigned char*>(result.data());
		const unsigned char *in = reinterpret_cast<const unsigned char*>(hash.constData());
		for(int i = 0; i < 10; i++)
		{
			out[i] = (in[i] ^ in[10+i]);
		}
	}

	return QString::fromLatin1(result.toHex().constData());
}

unsigned int AbstractTool::makeRevision(const unsigned int &core, const unsigned int &build)
{
	return ((core & 0x0000FFFF) << 16) | (build & 0x0000FFFF);
}

void AbstractTool::splitRevision(const unsigned int &revision, unsigned int &core, unsigned int &build)
{
	core  = (revision & 0xFFFF0000) >> 16;
	build = (revision & 0x0000FFFF);
}
