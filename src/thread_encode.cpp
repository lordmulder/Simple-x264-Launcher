///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2023 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "thread_encode.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "model_clipInfo.h"
#include "job_object.h"
#include "mediainfo.h"

//Encoders
#include "encoder_factory.h"

//Source
#include "source_factory.h"

//MUtils
#include <MUtils/OSSupport.h>
#include <MUtils/Version.h>

//Qt Framework
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QMutex>
#include <QTextCodec>
#include <QLocale>
#include <QCryptographicHash>

/*
 * RAII execution state handler
 */
class ExecutionStateHandler
{
public:
	ExecutionStateHandler(void)
	{
		x264_set_thread_execution_state(true);
	}
	~ExecutionStateHandler(void)
	{
		x264_set_thread_execution_state(false);
	}
private:
	//Disable copy constructor and assignment
	ExecutionStateHandler(const ExecutionStateHandler &other) {}
	ExecutionStateHandler &operator=(const ExecutionStateHandler &) {}

	//Prevent object allocation on the heap
	void *operator new(size_t);   void *operator new[](size_t);
	void operator delete(void *); void operator delete[](void*);
};

/*
 * Macros
 */
#define CHECK_STATUS(ABORT_FLAG, OK_FLAG) do \
{ \
	if(ABORT_FLAG) \
	{ \
		log("\nPROCESS ABORTED BY USER !!!"); \
		setStatus(JobStatus_Aborted); \
		if(QFileInfo(m_outputFileName).exists() && (QFileInfo(m_outputFileName).size() == 0)) QFile::remove(m_outputFileName); \
		return 0; \
	} \
	else if(!(OK_FLAG)) \
	{ \
		setStatus(JobStatus_Failed); \
		if(QFileInfo(m_outputFileName).exists() && (QFileInfo(m_outputFileName).size() == 0)) QFile::remove(m_outputFileName); \
		return 0; \
	} \
} \
while(0)

#define CONNECT(OBJ) do \
{ \
	if((OBJ)) \
	{ \
		connect((OBJ), SIGNAL(statusChanged(JobStatus)),      this, SLOT(setStatus(JobStatus)),      Qt::DirectConnection); \
		connect((OBJ), SIGNAL(progressChanged(unsigned int)), this, SLOT(setProgress(unsigned int)), Qt::DirectConnection); \
		connect((OBJ), SIGNAL(detailsChanged(QString)),       this, SLOT(setDetails(QString)),       Qt::DirectConnection); \
		connect((OBJ), SIGNAL(messageLogged(QString)),        this, SLOT(log(QString)),              Qt::DirectConnection); \
	} \
} \
while(0)

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

EncodeThread::EncodeThread(const QString &sourceFileName, const QString &outputFileName, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences)
:
	m_jobId(QUuid::createUuid()),
	m_sourceFileName(sourceFileName),
	m_outputFileName(outputFileName),
	m_options(new OptionsModel(*options)),
	m_sysinfo(sysinfo),
	m_preferences(preferences),
	m_jobObject(new JobObject),
	m_semaphorePaused(0),
	m_encoder(NULL),
	m_pipedSource(NULL)
{
	m_abort = false;
	m_pause = false;

	//Create encoder object
	m_encoder = EncoderFactory::createEncoder(m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName, m_outputFileName);

	//Create input handler object
	switch(MediaInfo::analyze(m_sourceFileName))
	{
	case MediaInfo::FILETYPE_AVISYNTH:
		if(m_sysinfo->hasAvisynth())
		{
			m_pipedSource = SourceFactory::createSource(SourceFactory::SourceType_AVS, m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName);
		}
		break;
	case MediaInfo::FILETYPE_VAPOURSYNTH:
		if(m_sysinfo->hasVapourSynth())
		{
			m_pipedSource = SourceFactory::createSource(SourceFactory::SourceType_VPS, m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName);
		}
		break;
	}

	//Establish connections
	CONNECT(m_encoder);
	CONNECT(m_pipedSource);
}

EncodeThread::~EncodeThread(void)
{
	MUTILS_DELETE(m_encoder);
	MUTILS_DELETE(m_jobObject);
	MUTILS_DELETE(m_options);
	MUTILS_DELETE(m_pipedSource);
}

///////////////////////////////////////////////////////////////////////////////
// Thread entry point
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::run(void)
{
	m_progress = 0;
	m_status = JobStatus_Starting;

	AbstractThread::run();

	if (m_exception)
	{
		log(tr("UNHANDLED EXCEPTION ERROR IN THREAD !!!"));
		setStatus(JobStatus_Failed);
	}

	if(m_jobObject)
	{
		m_jobObject->terminateJob(42);
		MUTILS_DELETE(m_jobObject);
	}
}

void EncodeThread::start(Priority priority)
{
	qDebug("Thread starting...");

	m_abort = false;
	m_pause = false;

	while(m_semaphorePaused.tryAcquire(1, 0));
	AbstractThread::start(priority);
}

///////////////////////////////////////////////////////////////////////////////
// Encode functions
///////////////////////////////////////////////////////////////////////////////

int EncodeThread::threadMain(void)
{
	QDateTime startTime = QDateTime::currentDateTime();

	// -----------------------------------------------------------------------------------
	// Print Information
	// -----------------------------------------------------------------------------------

	//Print some basic info
	log(tr("Simple x264 Launcher (Build #%1), built %2\n").arg(QString::number(x264_version_build()), MUtils::Version::app_build_date().toString(Qt::ISODate)));
	log(tr("Job started at %1, %2.\n").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
	log(tr("Source file : %1").arg(QDir::toNativeSeparators(m_sourceFileName)));
	log(tr("Output file : %1").arg(QDir::toNativeSeparators(m_outputFileName)));
	
	//Print system info
	log(tr("\n--- SYSTEMINFO ---\n"));
	log(tr("Binary Path : %1").arg(QDir::toNativeSeparators(m_sysinfo->getAppPath())));
	log(tr("Avisynth    : %1").arg(m_sysinfo->hasAvisynth() ? tr("Yes") : tr("No")));
	log(tr("VapourSynth : %1").arg(m_sysinfo->hasVapourSynth() ? tr("Yes") : tr("No")));

	//Print encoder settings
	log(tr("\n--- SETTINGS ---\n"));
	log(tr("Encoder : %1").arg(m_encoder->getName()));
	log(tr("Source  : %1").arg(m_pipedSource ? m_pipedSource->getName() : tr("Native")));
	log(tr("RC Mode : %1").arg(m_encoder->getEncoderInfo().rcModeToString(m_options->rcMode())));
	log(tr("Preset  : %1").arg(m_options->preset()));
	log(tr("Tuning  : %1").arg(m_options->tune()));
	log(tr("Profile : %1").arg(m_options->profile()));
	log(tr("Custom  : %1").arg(m_options->customEncParams().isEmpty() ? tr("<None>") : m_options->customEncParams()));
	
	bool ok = false;
	ClipInfo clipInfo;
	
	// -----------------------------------------------------------------------------------
	// Check Versions
	// -----------------------------------------------------------------------------------
	
	log(tr("\n--- CHECK VERSION ---\n"));

	unsigned int encoderRevision = UINT_MAX, sourceRevision = UINT_MAX;
	bool encoderModified = false, sourceModified = false;

	log("Detect video encoder version:\n");

	//Check encoder version
	encoderRevision = m_encoder->checkVersion(encoderModified);
	CHECK_STATUS(m_abort, (ok = (encoderRevision != UINT_MAX)));

	//Is encoder version suppoprted?
	CHECK_STATUS(m_abort, (ok = m_encoder->isVersionSupported(encoderRevision, encoderModified)));

	if(m_pipedSource)
	{
		log("\nDetect video source version:\n");

		//Is source type available?
		CHECK_STATUS(m_abort, (ok = m_pipedSource->isSourceAvailable()));

		//Checking source version
		sourceRevision = m_pipedSource->checkVersion(sourceModified);
		CHECK_STATUS(m_abort, (ok = (sourceRevision != UINT_MAX)));

		//Is source version supported?
		CHECK_STATUS(m_abort, (ok = m_pipedSource->isVersionSupported(sourceRevision, sourceModified)));
	}

	//Print tool versions
	log(QString("\n> %1").arg(m_encoder->printVersion(encoderRevision, encoderModified)));
	if(m_pipedSource)
	{
		log(QString("> %1").arg(m_pipedSource->printVersion(sourceRevision, sourceModified)));
	}

	// -----------------------------------------------------------------------------------
	// Detect Source Info
	// -----------------------------------------------------------------------------------

	//Detect source info
	if(m_pipedSource)
	{
		log(tr("\n--- GET SOURCE INFO ---\n"));
		ok = m_pipedSource->checkSourceProperties(clipInfo);
		CHECK_STATUS(m_abort, ok);
	}

	// -----------------------------------------------------------------------------------
	// Encoding Passes
	// -----------------------------------------------------------------------------------

	//Run encoding passes
	if(m_encoder->getEncoderInfo().rcModeToType(m_options->rcMode()) == AbstractEncoderInfo::RC_TYPE_MULTIPASS)
	{
		const QString passLogFile = getPasslogFile(m_outputFileName);
		
		log(tr("\n--- ENCODING PASS #1 ---\n"));
		ok = m_encoder->runEncodingPass(m_pipedSource, m_outputFileName, clipInfo, 1, passLogFile);
		CHECK_STATUS(m_abort, ok);

		log(tr("\n--- ENCODING PASS #2 ---\n"));
		ok = m_encoder->runEncodingPass(m_pipedSource, m_outputFileName, clipInfo, 2, passLogFile);
		CHECK_STATUS(m_abort, ok);
	}
	else
	{
		log(tr("\n--- ENCODING VIDEO ---\n"));
		ok = m_encoder->runEncodingPass(m_pipedSource, m_outputFileName, clipInfo);
		CHECK_STATUS(m_abort, ok);
	}

	// -----------------------------------------------------------------------------------
	// Encoding complete
	// -----------------------------------------------------------------------------------

	log(tr("\n--- COMPLETED ---\n"));

	int timePassed = startTime.secsTo(QDateTime::currentDateTime());
	log(tr("Job finished at %1, %2. Process took %3 minutes, %4 seconds.").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString(Qt::ISODate), QString::number(timePassed / 60), QString::number(timePassed % 60)));
	setStatus(JobStatus_Completed);

	return 1; /*completed*/
}

///////////////////////////////////////////////////////////////////////////////
// Misc functions
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::log(const QString &text)
{
	emit messageLogged(m_jobId, QDateTime::currentMSecsSinceEpoch(), text);
}

void EncodeThread::setStatus(const JobStatus &newStatus)
{
	if(m_status != newStatus)
	{
		if((newStatus != JobStatus_Completed) && (newStatus != JobStatus_Failed) && (newStatus != JobStatus_Aborted) && (newStatus != JobStatus_Paused))
		{
			if(m_status != JobStatus_Paused) setProgress(0);
		}
		if(newStatus == JobStatus_Failed)
		{
			setDetails("The job has failed. See log for details!");
		}
		if(newStatus == JobStatus_Aborted)
		{
			setDetails("The job was aborted by the user!");
		}
		m_status = newStatus;
		emit statusChanged(m_jobId, newStatus);
	}
}

void EncodeThread::setProgress(const unsigned int &newProgress)
{
	if(m_progress != newProgress)
	{
		m_progress = newProgress;
		emit progressChanged(m_jobId, m_progress);
	}
}

void EncodeThread::setDetails(const QString &text)
{
	if((!text.isEmpty()) && (m_details.compare(text) != 0))
	{
		emit detailsChanged(m_jobId, text);
		m_details = text;
	}
}

QString EncodeThread::getPasslogFile(const QString &outputFile)
{
	QFileInfo info(outputFile);
	QString passLogFile = QString("%1/%2.stats").arg(info.absolutePath(), info.completeBaseName());
	int counter = 1;

	while(QFileInfo(passLogFile).exists())
	{
		passLogFile = QString("%1/%2_%3.stats").arg(info.absolutePath(), info.completeBaseName(), QString::number(++counter));
	}

	return passLogFile;
}
