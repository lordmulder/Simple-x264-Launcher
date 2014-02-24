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

#pragma once

#include <QObject>
#include <QUuid>
#include <QMutex>

class OptionsModel;
class SysinfoModel;
class PreferencesModel;
class JobObject;
class QProcess;
class QSemaphore;
enum JobStatus;

class AbstractTool : public QObject
{
	Q_OBJECT

public:
	AbstractTool(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause);
	virtual ~AbstractTool(void) {/*NOP*/}
	
signals:
	void statusChanged(const JobStatus &newStatus);
	void progressChanged(unsigned int newProgress);
	void messageLogged(const QString &text);
	void detailsChanged(const QString &details);

protected:
	static const unsigned int m_processTimeoutInterval = 2500;
	static const unsigned int m_processTimeoutMaxCounter = 120;
	static const unsigned int m_processTimeoutWarning = 24;

	void log(const QString &text) { emit messageLogged(text); }
	void setStatus(const JobStatus &newStatus) { emit statusChanged(newStatus); } 
	void setProgress(unsigned int newProgress) { emit progressChanged(newProgress); }
	void setDetails(const QString &text) { emit detailsChanged(text); }

	bool startProcess(QProcess &process, const QString &program, const QStringList &args, bool mergeChannels = true);

	JobObject *const m_jobObject;
	const OptionsModel *const m_options;
	const SysinfoModel *const m_sysinfo;
	const PreferencesModel *const m_preferences;
	JobStatus &m_jobStatus;
	volatile bool *const m_abort;
	volatile bool *const m_pause;
	QSemaphore *const m_semaphorePause;

	static QString commandline2string(const QString &program, const QStringList &arguments);
	static QString stringToHash(const QString &string);
	
	static QMutex s_mutexStartProcess;
};
