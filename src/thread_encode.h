///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2012 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include <QThread>
#include <QUuid>
#include <QMutex>
#include <QStringList>

class OptionsModel;
class QProcess;

class EncodeThread : public QThread
{
	Q_OBJECT

public:
	enum JobStatus
	{
		JobStatus_Enqueued = 0,
		JobStatus_Starting = 1,
		JobStatus_Indexing = 2,
		JobStatus_Running = 3,
		JobStatus_Running_Pass1 = 4,
		JobStatus_Running_Pass2 = 5,
		JobStatus_Completed = 6,
		JobStatus_Failed = 7,
		JobStatus_Aborting = 8,
		JobStatus_Aborted = 9
	};
	
	EncodeThread(const QString &sourceFileName, const QString &outputFileName, const OptionsModel *options, const QString &binDir);
	~EncodeThread(void);

	QUuid getId(void) { return this->m_jobId; };
	const QString &sourceFileName(void) { return this->m_sourceFileName; };
	const QString &outputFileName(void) { return this->m_outputFileName; };

	void abortJob(void) { m_abort = true; }

protected:
	static QMutex m_mutex_startProcess;
	static void *m_handle_jobObject;

	static const int m_processTimeoutInterval = 60000;

	//Constants
	const QUuid m_jobId;
	const QString m_sourceFileName;
	const QString m_outputFileName;
	const OptionsModel *m_options;
	const QString m_binDir;

	//Flags
	volatile bool m_abort;
	
	//Internal status values
	JobStatus m_status;
	unsigned int m_progress;

	//Entry point
	virtual void run(void);
	
	//Encode functions
	void encode(void);
	bool runEncodingPass(int pass = 0, const QString &passLogFile = QString());
	QStringList buildCommandLine(int pass = 0, const QString &passLogFile = QString());

	//Auxiallary Stuff
	void log(const QString &text) { emit messageLogged(m_jobId, text); }
	inline void setStatus(JobStatus newStatus);
	inline void setProgress(unsigned int newProgress);
	inline void setDetails(const QString &text);
	bool startProcess(QProcess &process, const QString &program, const QStringList &args);
	
	static QString commandline2string(const QString &program, const QStringList &arguments);

signals:
	void statusChanged(const QUuid &jobId, EncodeThread::JobStatus newStatus);
	void progressChanged(const QUuid &jobId, unsigned int newProgress);
	void messageLogged(const QUuid &jobId, const QString &text);
	void detailsChanged(const QUuid &jobId, const QString &details);
};

