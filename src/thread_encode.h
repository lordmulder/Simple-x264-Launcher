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

class EncodeThread : public QThread
{
	Q_OBJECT

public:
	enum JobStatus
	{
		JobStatus_Starting = 0,
		JobStatus_Indexing = 1,
		JobStatus_Running = 2,
		JobStatus_Completed = 3,
		JobStatus_Failed = 4,
		JobStatus_Aborted = 5
	};
	
	EncodeThread(void);
	~EncodeThread(void);

	QUuid getId(void) { return this->m_jobId; };

protected:
	const QUuid m_jobId;

	virtual void run(void);

signals:
	void statusChanged(const QUuid &jobId, EncodeThread::JobStatus newStatus);
	void progressChanged(const QUuid &jobId, unsigned int newProgress);
	void messageLogged(const QUuid &jobId, const QString &text);
};

