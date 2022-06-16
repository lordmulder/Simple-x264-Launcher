///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2022 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "thread_abstract.h"
#include "model_status.h"

#include <QThread>
#include <QUuid>
#include <QMutex>
#include <QStringList>
#include <QSemaphore>

class SysinfoModel;
class PreferencesModel;
class OptionsModel;
class QProcess;
class JobObject;
class AbstractEncoder;
class AbstractSource;

class EncodeThread : public AbstractThread
{
	Q_OBJECT

public:
	EncodeThread(const QString &sourceFileName, const QString &outputFileName, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const m_preferences);
	~EncodeThread(void);

	QUuid getId(void) { return this->m_jobId; };
	const QString &sourceFileName(void) const { return this->m_sourceFileName; }
	const QString &outputFileName(void) const { return this->m_outputFileName; }
	const OptionsModel *options(void)   const { return m_options; }
	
	void pauseJob(void)
	{
		m_pause = true;
	}

	void resumeJob(void)
	{
		m_pause = false;
		m_semaphorePaused.release();
	}

	void abortJob(void)
	{
		m_abort = true;
		m_pause = false;
		m_semaphorePaused.release();
	}

protected:
	//Globals
	const SysinfoModel *const m_sysinfo;
	const PreferencesModel *const m_preferences;

	//Constants
	const QUuid m_jobId;
	const OptionsModel *m_options;
	const QString m_sourceFileName;
	const QString m_outputFileName;

	//Flags
	volatile bool m_abort;
	volatile bool m_pause;
	
	//Synchronization
	QSemaphore m_semaphorePaused;

	//Job Object
	JobObject *m_jobObject;

	//Internal status values
	JobStatus m_status;
	unsigned int m_progress;
	QString m_details;

	//Encoder and Source objects
	AbstractEncoder *m_encoder;
	AbstractSource *m_pipedSource;

	//Entry point
	virtual void run(void);
	
	//Thread main
	virtual int threadMain(void);

	//Static functions
	static QString getPasslogFile(const QString &outputFile);

signals:
	void statusChanged(const QUuid &jobId, const JobStatus &newStatus);
	void progressChanged(const QUuid &jobId, const unsigned int &newProgress);
	void messageLogged(const QUuid &jobId, qint64, const QString &text);
	void detailsChanged(const QUuid &jobId, const QString &details);

private slots:
	void log(const QString &text);
	void setStatus(const JobStatus &newStatus);
	void setProgress(const unsigned int &newProgress);
	void setDetails(const QString &text);

public slots:
	void start(Priority priority = InheritPriority);
};
