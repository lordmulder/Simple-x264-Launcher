///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2016 LoRd_MuldeR <MuldeR2@GMX.de>
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

//Internal
#include "global.h"
#include "model_jobList.h"
#include "thread_encode.h"
#include "encoder_factory.h"
#include "model_options.h"
#include "model_preferences.h"
#include "resource.h"

//MUtils
#include <MUtils/Sound.h>

//Qt
#include <QIcon>
#include <QFileInfo>
#include <QSettings>

static const char *KEY_ENTRY_COUNT = "entry_count";
static const char *KEY_SOURCE_FILE = "source_file";
static const char *KEY_OUTPUT_FILE = "output_file";
static const char *KEY_ENC_OPTIONS = "enc_options";

static const char *JOB_TEMPLATE = "job_%08x";

#define VALID_INDEX(INDEX) ((INDEX).isValid() && ((INDEX).row() >= 0) && ((INDEX).row() < m_jobs.count()))

JobListModel::JobListModel(PreferencesModel *preferences)
{
	m_preferences = preferences;
}

JobListModel::~JobListModel(void)
{
	while(!m_jobs.isEmpty())
	{
		QUuid id = m_jobs.takeFirst();
		EncodeThread *thread = m_threads.value(id, NULL);
		LogFileModel *logFile = m_logFile.value(id, NULL);
		MUTILS_DELETE(thread);
		MUTILS_DELETE(logFile);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Model interface
///////////////////////////////////////////////////////////////////////////////

int JobListModel::columnCount(const QModelIndex &parent) const
{
	return 4;
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
		case 3:
			return QVariant::fromValue<QString>(tr("Details"));
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
				return m_name.value(m_jobs.at(index.row()));
				break;
			case 1:
				switch(m_status.value(m_jobs.at(index.row())))
				{
				case JobStatus_Enqueued:
					return QVariant::fromValue<QString>(tr("Enqueued."));
					break;
				case JobStatus_Starting:
					return QVariant::fromValue<QString>(tr("Starting..."));
					break;
				case JobStatus_Indexing:
					return QVariant::fromValue<QString>(tr("Indexing..."));
					break;
				case JobStatus_Running:
					return QVariant::fromValue<QString>(tr("Running..."));
					break;
				case JobStatus_Running_Pass1:
					return QVariant::fromValue<QString>(tr("Running... (Pass 1)"));
					break;
				case JobStatus_Running_Pass2:
					return QVariant::fromValue<QString>(tr("Running... (Pass 2)"));
					break;
				case JobStatus_Completed:
					return QVariant::fromValue<QString>(tr("Completed."));
					break;
				case JobStatus_Failed:
					return QVariant::fromValue<QString>(tr("Failed!"));
					break;
				case JobStatus_Pausing:
					return QVariant::fromValue<QString>(tr("Pausing..."));
					break;
				case JobStatus_Paused:
					return QVariant::fromValue<QString>(tr("Paused."));
					break;
				case JobStatus_Resuming:
					return QVariant::fromValue<QString>(tr("Resuming..."));
					break;
				case JobStatus_Aborting:
					return QVariant::fromValue<QString>(tr("Aborting..."));
					break;
				case JobStatus_Aborted:
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
			case 3:
				return m_details.value(m_jobs.at(index.row()));
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
			case JobStatus_Enqueued:
				return QIcon(":/buttons/hourglass.png");
				break;
			case JobStatus_Starting:
				return QIcon(":/buttons/lightning.png");
				break;
			case JobStatus_Indexing:
				return QIcon(":/buttons/find.png");
				break;
			case JobStatus_Running:
			case JobStatus_Running_Pass1:
			case JobStatus_Running_Pass2:
				return QIcon(":/buttons/play.png");
				break;
			case JobStatus_Completed:
				return QIcon(":/buttons/accept.png");
				break;
			case JobStatus_Failed:
				return QIcon(":/buttons/exclamation.png");
				break;
			case JobStatus_Pausing:
				return QIcon(":/buttons/clock_pause.png");
				break;
			case JobStatus_Paused:
				return QIcon(":/buttons/suspended.png");
				break;
			case JobStatus_Resuming:
				return QIcon(":/buttons/clock_play.png");
				break;
			case JobStatus_Aborting:
				return QIcon(":/buttons/clock_stop.png");
				break;
			case JobStatus_Aborted:
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

QModelIndex JobListModel::insertJob(EncodeThread *thread)
{
	const QUuid id = thread->getId();
	if(m_jobs.contains(id))
	{
		return QModelIndex();
	}
	
	const AbstractEncoderInfo &encoderInfo = EncoderFactory::getEncoderInfo(thread->options()->encType());
	QString config = encoderInfo.getName();
	switch(encoderInfo.rcModeToType(thread->options()->rcMode()))
	{
	case AbstractEncoderInfo::RC_TYPE_QUANTIZER:
		config.append(QString(", %1@%2").arg(encoderInfo.rcModeToString(thread->options()->rcMode()), QString::number(qRound(thread->options()->quantizer()))));
		break;
	case AbstractEncoderInfo::RC_TYPE_RATE_KBPS:
	case AbstractEncoderInfo::RC_TYPE_MULTIPASS:
		config.append(QString(", %1@%2").arg(encoderInfo.rcModeToString(thread->options()->rcMode()), QString::number(thread->options()->bitrate())));
		break;
	}

	int n = 2;
	QString jobName = QString("%1 (%2)").arg(QFileInfo(thread->sourceFileName()).completeBaseName().simplified(), config);
	forever
	{
		bool unique = true;
		for(int i = 0; i < m_jobs.count(); i++)
		{
			if(m_name.value(m_jobs.at(i)).compare(jobName, Qt::CaseInsensitive) == 0)
			{
				unique = false;
				break;
			}
		}
		if(!unique)
		{
			jobName = QString("%1 %2 (%3)").arg(QFileInfo(thread->sourceFileName()).completeBaseName().simplified(), QString::number(n++), config);
			continue;
		}
		break;
	}
	
	LogFileModel *logFile = new LogFileModel(thread->sourceFileName(), thread->outputFileName(), config);
	
	beginInsertRows(QModelIndex(), m_jobs.count(), m_jobs.count());
	m_jobs.append(id);
	m_name.insert(id, jobName);
	m_status.insert(id, JobStatus_Enqueued);
	m_progress.insert(id, 0);
	m_threads.insert(id, thread);
	m_logFile.insert(id, logFile);
	m_details.insert(id, tr("Not started yet."));
	endInsertRows();

	connect(thread, SIGNAL(statusChanged(QUuid, JobStatus)), this, SLOT(updateStatus(QUuid, JobStatus)), Qt::QueuedConnection);
	connect(thread, SIGNAL(progressChanged(QUuid, unsigned int)), this, SLOT(updateProgress(QUuid, unsigned int)), Qt::QueuedConnection);
	connect(thread, SIGNAL(messageLogged(QUuid, QString)), logFile, SLOT(addLogMessage(QUuid, QString)), Qt::QueuedConnection);
	connect(thread, SIGNAL(detailsChanged(QUuid, QString)), this, SLOT(updateDetails(QUuid, QString)), Qt::QueuedConnection);
	
	return createIndex(m_jobs.count() - 1, 0, NULL);
}

bool JobListModel::startJob(const QModelIndex &index)
{
	if(VALID_INDEX(index))
	{
		QUuid id = m_jobs.at(index.row());
		if(m_status.value(id) == JobStatus_Enqueued)
		{
			updateStatus(id, JobStatus_Starting);
			updateDetails(id, tr("Starting up, please wait..."));
			m_threads.value(id)->start();
			return true;
		}
	}

	return false;
}

bool JobListModel::pauseJob(const QModelIndex &index)
{
	if(VALID_INDEX(index))
	{
		QUuid id = m_jobs.at(index.row());
		JobStatus status = m_status.value(id);
		if((status == JobStatus_Indexing) || (status == JobStatus_Running) ||
			(status == JobStatus_Running_Pass1) || (status == JobStatus_Running_Pass2))
		{
			updateStatus(id, JobStatus_Pausing);
			m_threads.value(id)->pauseJob();
			return true;
		}
	}

	return false;
}

bool JobListModel::resumeJob(const QModelIndex &index)
{
	if(VALID_INDEX(index))
	{
		QUuid id = m_jobs.at(index.row());
		JobStatus status = m_status.value(id);
		if(status == JobStatus_Paused)
		{
			updateStatus(id, JobStatus_Resuming);
			m_threads.value(id)->resumeJob();
			return true;
		}
	}

	return false;
}

bool JobListModel::abortJob(const QModelIndex &index)
{
	if(VALID_INDEX(index))
	{
		QUuid id = m_jobs.at(index.row());
		if(m_status.value(id) == JobStatus_Indexing || m_status.value(id) == JobStatus_Running ||
			m_status.value(id) == JobStatus_Running_Pass1 || JobStatus_Running_Pass2)
		{
			updateStatus(id, JobStatus_Aborting);
			m_threads.value(id)->abortJob();
			return true;
		}
	}

	return false;
}

bool JobListModel::deleteJob(const QModelIndex &index)
{
	if(VALID_INDEX(index))
	{
		QUuid id = m_jobs.at(index.row());
		if(m_status.value(id) == JobStatus_Completed || m_status.value(id) == JobStatus_Failed ||
			m_status.value(id) == JobStatus_Aborted || m_status.value(id) == JobStatus_Enqueued)
		{
			int idx = index.row();
			QUuid id = m_jobs.at(idx);
			EncodeThread *thread = m_threads.value(id, NULL);
			LogFileModel *logFile = m_logFile.value(id, NULL);
			if((thread == NULL) || (!thread->isRunning()))
			{
				
				beginRemoveRows(QModelIndex(), idx, idx);
				m_jobs.removeAt(index.row());
				m_name.remove(id);
				m_threads.remove(id);
				m_status.remove(id);
				m_progress.remove(id);
				m_logFile.remove(id);
				m_details.remove(id);
				endRemoveRows();
				MUTILS_DELETE(thread);
				MUTILS_DELETE(logFile);
				return true;
			}
		}
	}

	return false;
}

bool JobListModel::moveJob(const QModelIndex &index, const int &direction)
{
	if(VALID_INDEX(index))
	{
		if((direction == MOVE_UP) && (index.row() > 0))
		{
			beginMoveRows(QModelIndex(), index.row(), index.row(), QModelIndex(), index.row() - 1);
			m_jobs.swap(index.row(), index.row() - 1);
			endMoveRows();
			return true;
		}
		if((direction == MOVE_DOWN) && (index.row() < m_jobs.size() - 1))
		{
			beginMoveRows(QModelIndex(), index.row(), index.row(), QModelIndex(), index.row() + 2);
			m_jobs.swap(index.row(), index.row() + 1);
			endMoveRows();
			return true;
		}
	}

	return false;
}

LogFileModel *JobListModel::getLogFile(const QModelIndex &index)
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		return m_logFile.value(m_jobs.at(index.row()));
	}

	return NULL;
}

const QString &JobListModel::getJobSourceFile(const QModelIndex &index)
{
	static QString nullStr;
	
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		EncodeThread *thread = m_threads.value(m_jobs.at(index.row()));
		return (thread != NULL) ? thread->sourceFileName() : nullStr;
	}

	return nullStr;
}

const QString &JobListModel::getJobOutputFile(const QModelIndex &index)
{
	static QString nullStr;
	
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		EncodeThread *thread = m_threads.value(m_jobs.at(index.row()));
		return (thread != NULL) ? thread->outputFileName() : nullStr;
	}

	return nullStr;
}

JobStatus JobListModel::getJobStatus(const QModelIndex &index)
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		return m_status.value(m_jobs.at(index.row()));
	}

	return static_cast<JobStatus>(-1);
}

unsigned int JobListModel::getJobProgress(const QModelIndex &index)
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		return m_progress.value(m_jobs.at(index.row()));
	}

	return 0;
}

const OptionsModel *JobListModel::getJobOptions(const QModelIndex &index)
{
	static QString nullStr;
	
	if(index.isValid() && index.row() >= 0 && index.row() < m_jobs.count())
	{
		EncodeThread *thread = m_threads.value(m_jobs.at(index.row()));
		return (thread != NULL) ? thread->options() : NULL;
	}

	return NULL;
}

QModelIndex JobListModel::getJobIndexById(const QUuid &id)
{
	if(m_jobs.contains(id))
	{
		return createIndex(m_jobs.indexOf(id), 0);
	}

	return QModelIndex();
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void JobListModel::updateStatus(const QUuid &jobId, JobStatus newStatus)
{
	int index = -1;
	
	if((index = m_jobs.indexOf(jobId)) >= 0)
	{
		m_status.insert(jobId, newStatus);
		emit dataChanged(createIndex(index, 0), createIndex(index, 1));

		if(m_preferences->getEnableSounds())
		{
			switch(newStatus)
			{
			case JobStatus_Completed:
				MUtils::Sound::play_sound("tada", true);
				break;
			case JobStatus_Aborted:
				MUtils::Sound::play_sound("shattering", true);
				break;
			case JobStatus_Failed:
				MUtils::Sound::play_sound("failure", true);
				break;
			}
		}
	}
}

void JobListModel::updateProgress(const QUuid &jobId, unsigned int newProgress)
{
	int index = -1;

	if((index = m_jobs.indexOf(jobId)) >= 0)
	{
		m_progress.insert(jobId, qBound(0U, newProgress, 100U));
		emit dataChanged(createIndex(index, 2), createIndex(index, 2));
	}
}

void JobListModel::updateDetails(const QUuid &jobId, const QString &details)
{
	int index = -1;

	if((index = m_jobs.indexOf(jobId)) >= 0)
	{
		m_details.insert(jobId, details);
		emit dataChanged(createIndex(index, 3), createIndex(index, 3));
	}
}

size_t JobListModel::saveQueuedJobs(void)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/queue.ini").arg(appDir), QSettings::IniFormat);
	
	settings.clear();
	settings.setValue(KEY_ENTRY_COUNT, 0);
	size_t jobCounter = 0;

	for(QList<QUuid>::ConstIterator iter = m_jobs.constBegin(); iter != m_jobs.constEnd(); iter++)
	{
		if(m_status.value(*iter) == JobStatus_Enqueued)
		{
			if(const EncodeThread *thread = m_threads.value(*iter))
			{
				settings.beginGroup(QString().sprintf(JOB_TEMPLATE, jobCounter++));
				settings.setValue(KEY_SOURCE_FILE, thread->sourceFileName());
				settings.setValue(KEY_OUTPUT_FILE, thread->outputFileName());

				settings.beginGroup(KEY_ENC_OPTIONS);
				OptionsModel::saveOptions(thread->options(), settings);

				settings.endGroup();
				settings.endGroup();

				settings.setValue(KEY_ENTRY_COUNT, jobCounter);
			}
		}
	}

	settings.sync();
	return jobCounter;
}

size_t JobListModel::loadQueuedJobs(const SysinfoModel *sysinfo)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/queue.ini").arg(appDir), QSettings::IniFormat);

	bool ok = false;
	const size_t jobCounter = settings.value(KEY_ENTRY_COUNT, 0).toUInt(&ok);

	if((!ok) || (jobCounter < 1))
	{
		return 0;
	}

	const QStringList groups = settings.childGroups();
	for(size_t i = 0; i < jobCounter; i++)
	{
		if(!groups.contains(QString().sprintf(JOB_TEMPLATE, i)))
		{
			return 0;
		}
	}

	size_t jobsCreated = 0;
	for(size_t i = 0; i < jobCounter; i++)
	{
		settings.beginGroup(QString().sprintf(JOB_TEMPLATE, i));
		const QString sourceFileName = settings.value(KEY_SOURCE_FILE, QString()).toString().trimmed();
		const QString outputFileName = settings.value(KEY_OUTPUT_FILE, QString()).toString().trimmed();

		if(sourceFileName.isEmpty() || outputFileName.isEmpty())
		{
			settings.endGroup();
			continue;
		}

		settings.beginGroup(KEY_ENC_OPTIONS);
		OptionsModel options(sysinfo);
		const bool okay = OptionsModel::loadOptions(&options, settings);

		settings.endGroup();
		settings.endGroup();

		if(okay)
		{
			EncodeThread *thread = new EncodeThread(sourceFileName, outputFileName, &options, sysinfo, m_preferences);
			insertJob(thread);
			jobsCreated++;
		}
	}

	return jobsCreated;
}

void JobListModel::clearQueuedJobs(void)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/queue.ini").arg(appDir), QSettings::IniFormat);
	settings.clear();
	settings.setValue(KEY_ENTRY_COUNT, 0);
	settings.sync();
}
