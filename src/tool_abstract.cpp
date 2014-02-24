///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2014 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "binaries.h"
#include "job_object.h"

#include <QProcess>
#include <QMutexLocker>
#include <QDir>
#include <QCryptographicHash>

QMutex AbstractTool::s_mutexStartProcess;

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

bool AbstractTool::startProcess(QProcess &process, const QString &program, const QStringList &args, bool mergeChannels)
{
	QMutexLocker lock(&s_mutexStartProcess);
	log(commandline2string(program, args) + "\n");

	process.setWorkingDirectory(QDir::tempPath());

	if(mergeChannels)
	{
		process.setProcessChannelMode(QProcess::MergedChannels);
		process.setReadChannel(QProcess::StandardOutput);
	}
	else
	{
		process.setProcessChannelMode(QProcess::SeparateChannels);
		process.setReadChannel(QProcess::StandardError);
	}

	process.start(program, args);
	
	if(process.waitForStarted())
	{
		m_jobObject->addProcessToJob(&process);
		x264_change_process_priority(&process, m_preferences->getProcessPriority());
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

QString AbstractTool::commandline2string(const QString &program, const QStringList &arguments)
{
	QString commandline = (program.contains(' ') ? QString("\"%1\"").arg(program) : program);
	
	for(int i = 0; i < arguments.count(); i++)
	{
		commandline += (arguments.at(i).contains(' ') ? QString(" \"%1\"").arg(arguments.at(i)) : QString(" %1").arg(arguments.at(i)));
	}

	return commandline;
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
