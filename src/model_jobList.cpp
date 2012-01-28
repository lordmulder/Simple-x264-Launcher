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

#include "model_jobList.h"
#include "thread_encode.h"

#include <QIcon>

JobListModel::JobListModel(void)
{
}

JobListModel::~JobListModel(void)
{
}

///////////////////////////////////////////////////////////////////////////////
// Model interface
///////////////////////////////////////////////////////////////////////////////

int JobListModel::columnCount(const QModelIndex &parent) const
{
	return 3;
}

int JobListModel::rowCount(const QModelIndex &parent) const
{
	return m_jobs.count();
}

QVariant JobListModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
	if((orientation == Qt::Horizontal) && (role == Qt::DisplayRole))
	{
		switch(section)
		{
		case 0:
			return QVariant::fromValue<QString>(tr("Job"));
			break;
		case 1:
			return QVariant::fromValue<QString>(tr("Status"));
			break;
		case 2:
			return QVariant::fromValue<QString>(tr("Progress"));
			break;
		default:
			return QVariant();
			break;
		}
	}

	return QVariant();
}

QModelIndex JobListModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column, NULL);
}

QModelIndex JobListModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

QVariant JobListModel::data(const QModelIndex &index, int role) const
{
	if(role == Qt::DisplayRole)
	{
		if(index.row() >= 0 && index.row() < m_jobs.count())
		{
			switch(index.column())
			{
			case 0:
				return m_jobs.at(index.row()).toString();
				break;
			case 1:
				switch(m_status.value(m_jobs.at(index.row())))
				{
				case EncodeThread::JobStatus_Enqueued:
					return QVariant::fromValue<QString>(tr("Enqueued."));
					break;
				case EncodeThread::JobStatus_Starting:
					return QVariant::fromValue<QString>(tr("Starting..."));
					break;
				case EncodeThread::JobStatus_Indexing:
					return QVariant::fromValue<QString>(tr("Indexing..."));
					break;
				case EncodeThread::JobStatus_Running:
					return QVariant::fromValue<QString>(tr("Running..."));
					break;
				case EncodeThread::JobStatus_Completed:
					return QVariant::fromValue<QString>(tr("Completed."));
					break;
				case EncodeThread::JobStatus_Failed:
					return QVariant::fromValue<QString>(tr("Failed!"));
					break;
				case EncodeThread::JobStatus_Aborted:
					return QVariant::fromValue<QString>(tr("Aborted!"));
					break;
				default:
					return QVariant::fromValue<QString>(tr("(Unknown)"));
					break;
				}
				break;
			case 2:
				return QString().sprintf("%d%%", m_progress.value(m_jobs.at(index.row())));
				break;
			default:
				return QVariant();
				break;
			}
		}
	}
	else if(role == Qt::DecorationRole)
	{
		if(index.row() >= 0 && index.row() < m_jobs.count() && index.column() == 0)
		{
			switch(m_status.value(m_jobs.at(index.row())))
			{
			case EncodeThread::JobStatus_Enqueued:
				return QIcon(":/buttons/clock_pause.png");
				break;
			case EncodeThread::JobStatus_Starting:
				return QIcon(":/buttons/lightning.png");
				break;
			case EncodeThread::JobStatus_Indexing:
				return QIcon(":/buttons/find.png");
				break;
			case EncodeThread::JobStatus_Running:
				return QIcon(":/buttons/play.png");
				break;
			case EncodeThread::JobStatus_Completed:
				return QIcon(":/buttons/accept.png");
				break;
			case EncodeThread::JobStatus_Failed:
				return QIcon(":/buttons/exclamation.png");
				break;
			case EncodeThread::JobStatus_Aborted:
				return QIcon(":/buttons/error.png");
				break;
			default:
				return QVariant();
				break;
			}
		}
	}

	return QVariant();
}

///////////////////////////////////////////////////////////////////////////////
// Public interface
///////////////////////////////////////////////////////////////////////////////

bool JobListModel::insertJob(EncodeThread *thread)
{
	QUuid id = thread->getId();
	LogFileModel *logFile = NULL;

	if(m_jobs.contains(id))
	{
		return false;
	}
		
	beginInsertRows(QModelIndex(), m_jobs.count(), m_jobs.count());
	m_jobs.append(id);
	m_status.insert(id, EncodeThread::JobStatus_Enqueued);
	m_progress.insert(id, 0);
	m_threads.insert(id, thread);
	m_logFile.insert(id, (logFile = new LogFileModel));
	endInsertRows();

	connect(thread, SIGNAL(statusChanged(QUuid, EncodeThread::JobStatus)), this, SLOT(updateStatus(QUuid, EncodeThread::JobStatus)), Qt::QueuedConnection);
	connect(thread, SIGNAL(progressChanged(QUuid, unsigned int)), this, SLOT(updateProgress(QUuid, unsigned int)), Qt::QueuedConnection);
	connect(thread, SIGNAL(messageLogged(QUuid, QString)), logFile, SLOT(addLogMessage(QUuid, QString)), Qt::QueuedConnection);
	
	return true;
}

LogFileModel *JobListModel::getLogFile(const QModelIndex &index)
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		return m_logFile.value(m_jobs.at(index.row()));
	}
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void JobListModel::updateStatus(const QUuid &jobId, EncodeThread::JobStatus newStatus)
{
	int index = -1;
	
	if((index = m_jobs.indexOf(jobId)) >= 0)
	{
		m_status.insert(jobId, newStatus);
		emit dataChanged(createIndex(index, 0), createIndex(index, 1));
	}
}

void JobListModel::updateProgress(const QUuid &jobId, unsigned int newProgress)
{
	int index = -1;

	if((index = m_jobs.indexOf(jobId)) >= 0)
	{
		m_progress.insert(jobId, newProgress);
		emit dataChanged(createIndex(index, 2), createIndex(index, 2));
	}
}
