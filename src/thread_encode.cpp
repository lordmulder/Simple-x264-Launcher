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

#include "thread_encode.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "job_object.h"
#include "binaries.h"

//Encoders
#include "encoder_x264.h"
#include "encoder_x265.h"

//Source
#include "source_avisynth.h"
#include "source_vapoursynth.h"

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
		return; \
	} \
	else if(!(OK_FLAG)) \
	{ \
		setStatus(JobStatus_Failed); \
		if(QFileInfo(m_outputFileName).exists() && (QFileInfo(m_outputFileName).size() == 0)) QFile::remove(m_outputFileName); \
		return; \
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

/*
 * Input types
 */
typedef enum
{
	INPUT_NATIVE = 0,
	INPUT_AVISYN = 1,
	INPUT_VAPOUR = 2
};

/*
 * Static vars
 */
//static const char *VPS_TEST_FILE = "import vapoursynth as vs\ncore = vs.get_core()\nv = core.std.BlankClip()\nv.set_output()\n";

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
	switch(options->encType())
	{
	case OptionsModel::EncType_X264:
		m_encoder = new X264Encoder(m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName, m_outputFileName);
		break;
	case OptionsModel::EncType_X265:
		m_encoder = new X265Encoder(m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName, m_outputFileName);
		break;
	default:
		throw "Unknown encoder type encountered!";
	}

	//Create input handler object
	switch(getInputType(QFileInfo(m_sourceFileName).suffix()))
	{
	case INPUT_AVISYN:
		m_pipedSource = new AvisynthSource   (m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName);
		break;
	case INPUT_VAPOUR:
		m_pipedSource = new VapoursynthSource(m_jobObject, m_options, m_sysinfo, m_preferences, m_status, &m_abort, &m_pause, &m_semaphorePaused, m_sourceFileName);
		break;
	}

	//Establish connections
	CONNECT(m_encoder);
	CONNECT(m_pipedSource);
}

EncodeThread::~EncodeThread(void)
{
	X264_DELETE(m_encoder);
	X264_DELETE(m_jobObject);
	X264_DELETE(m_options);
}

///////////////////////////////////////////////////////////////////////////////
// Thread entry point
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::run(void)
{
#if !defined(_DEBUG)
	__try
	{
		checkedRun();
	}
	__except(1)
	{
		qWarning("STRUCTURED EXCEPTION ERROR IN ENCODE THREAD !!!");
	}
#else
	checkedRun();
#endif

	if(m_jobObject)
	{
		m_jobObject->terminateJob(42);
		X264_DELETE(m_jobObject);
	}
}

void EncodeThread::checkedRun(void)
{
	m_progress = 0;
	m_status = JobStatus_Starting;

	try
	{
		try
		{
			ExecutionStateHandler executionStateHandler;
			encode();
		}
		catch(char *msg)
		{
			log(tr("EXCEPTION ERROR IN THREAD: ").append(QString::fromLatin1(msg)));
			setStatus(JobStatus_Failed);
		}
		catch(...)
		{
			log(tr("UNHANDLED EXCEPTION ERROR IN THREAD !!!"));
			setStatus(JobStatus_Failed);
		}
	}
	catch(...)
	{
		x264_fatal_exit(L"Unhandeled exception error in encode thread!");
	}
}

void EncodeThread::start(Priority priority)
{
	qDebug("Thread starting...");

	m_abort = false;
	m_pause = false;

	while(m_semaphorePaused.tryAcquire(1, 0));
	QThread::start(priority);
}

///////////////////////////////////////////////////////////////////////////////
// Encode functions
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::encode(void)
{
	QDateTime startTime = QDateTime::currentDateTime();

	// -----------------------------------------------------------------------------------
	// Print Information
	// -----------------------------------------------------------------------------------

	//Print some basic info
	log(tr("Simple x264 Launcher (Build #%1), built %2\n").arg(QString::number(x264_version_build()), x264_version_date().toString(Qt::ISODate)));
	log(tr("Job started at %1, %2.\n").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
	log(tr("Source file: %1").arg(QDir::toNativeSeparators(m_sourceFileName)));
	log(tr("Output file: %1").arg(QDir::toNativeSeparators(m_outputFileName)));
	
	//Print system info
	log(tr("\n--- SYSTEMINFO ---\n"));
	log(tr("Binary Path: %1").arg(QDir::toNativeSeparators(m_sysinfo->getAppPath())));
	log(tr("Avisynth OK: %1").arg(m_sysinfo->hasAVSSupport() ? tr("Yes") : tr("No")));
	log(tr("VapourSynth: %1").arg(m_sysinfo->hasVPSSupport() ? QDir::toNativeSeparators(m_sysinfo->getVPSPath()) : tr("N/A")));

	//Print encoder settings
	log(tr("\n--- SETTINGS ---\n"));
	log(tr("RC Mode: %1").arg(OptionsModel::rcMode2String(m_options->rcMode())));
	log(tr("Preset:  %1").arg(m_options->preset()));
	log(tr("Tuning:  %1").arg(m_options->tune()));
	log(tr("Profile: %1").arg(m_options->profile()));
	log(tr("Custom:  %1").arg(m_options->customEncParams().isEmpty() ? tr("(None)") : m_options->customEncParams()));
	
	bool ok = false;
	unsigned int frames = 0;
	
	// -----------------------------------------------------------------------------------
	// Check Versions
	// -----------------------------------------------------------------------------------
	
	log(tr("\n--- CHECK VERSION ---\n"));

	//Check encoder version
	bool encoderModified = false;
	const unsigned int encoderRevision = m_encoder->checkVersion(encoderModified);
	CHECK_STATUS(m_abort, (ok = (encoderRevision != UINT_MAX)));
	
	//Print source versions
	m_encoder->printVersion(encoderRevision, encoderModified);

	//Is encoder version suppoprted?
	CHECK_STATUS(m_abort, (ok = m_encoder->isVersionSupported(encoderRevision, encoderModified)));

	if(m_pipedSource)
	{
		//Is source type available?
		CHECK_STATUS(m_abort, (ok = m_pipedSource->isSourceAvailable()));

		//Checking source version
		bool sourceModified = false;
		const unsigned int sourceRevision = m_pipedSource->checkVersion(sourceModified);
		CHECK_STATUS(m_abort, (ok = (sourceRevision != UINT_MAX)));

		//Print source versions
		m_pipedSource->printVersion(sourceModified, sourceModified);

		//Is source version supported?
		CHECK_STATUS(m_abort, (ok = m_pipedSource->isVersionSupported(sourceRevision, sourceModified)));
	}

	// -----------------------------------------------------------------------------------
	// Detect Source Info
	// -----------------------------------------------------------------------------------

	//Detect source info
	if(m_pipedSource)
	{
		log(tr("\n--- GET SOURCE INFO ---\n"));
		ok = m_pipedSource->checkSourceProperties(frames);
		CHECK_STATUS(m_abort, ok);
	}

	// -----------------------------------------------------------------------------------
	// Encoding Passes
	// -----------------------------------------------------------------------------------

	//Run encoding passes
	if(m_options->rcMode() == OptionsModel::RCMode_2Pass)
	{
		const QString passLogFile = getPasslogFile(m_outputFileName);
		
		log(tr("\n--- ENCODING PASS #1 ---\n"));
		ok = m_encoder->runEncodingPass(m_pipedSource, m_outputFileName, frames, 1, passLogFile);
		CHECK_STATUS(m_abort, ok);

		log(tr("\n--- ENCODING PASS #2 ---\n"));
		ok = m_encoder->runEncodingPass(m_pipedSource, m_outputFileName, frames, 2, passLogFile);
		CHECK_STATUS(m_abort, ok);
	}
	else
	{
		log(tr("\n--- ENCODING VIDEO ---\n"));
		ok = m_encoder->runEncodingPass(m_pipedSource, m_outputFileName, frames);
		CHECK_STATUS(m_abort, ok);
	}

	// -----------------------------------------------------------------------------------
	// Encoding complete
	// -----------------------------------------------------------------------------------

	log(tr("\n--- COMPLETED ---\n"));

	int timePassed = startTime.secsTo(QDateTime::currentDateTime());
	log(tr("Job finished at %1, %2. Process took %3 minutes, %4 seconds.").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString(Qt::ISODate), QString::number(timePassed / 60), QString::number(timePassed % 60)));
	setStatus(JobStatus_Completed);
}

///////////////////////////////////////////////////////////////////////////////
// Misc functions
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::log(const QString &text)
{
	emit messageLogged(m_jobId, text);
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

int EncodeThread::getInputType(const QString &fileExt)
{
	int type = INPUT_NATIVE;

	if(fileExt.compare("avs",  Qt::CaseInsensitive) == 0) type = INPUT_AVISYN;
	if(fileExt.compare("avsi", Qt::CaseInsensitive) == 0) type = INPUT_AVISYN;
	if(fileExt.compare("vpy",  Qt::CaseInsensitive) == 0) type = INPUT_VAPOUR;
	if(fileExt.compare("py",   Qt::CaseInsensitive) == 0) type = INPUT_VAPOUR;

	return type;
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




// ==========================================
// DISABLED
// ==========================================

/*
unsigned int EncodeThread::checkVersionAvs2yuv(void)
{
	if(!m_sysinfo->hasAVSSupport())
	{
		log(tr("\nAVS INPUT REQUIRES VAPOURSYNTH, BUT IT IS *NOT* AVAILABLE !!!"));
		return false;
	}

	QProcess process;

	log("\nCreating process:");
	if(!startProcess(process, AVS_BINARY(m_sysinfo, m_preferences), QStringList()))
	{
		return false;;
	}

	QRegExp regExpVersionMod("\\bAvs2YUV (\\d+).(\\d+)bm(\\d)\\b", Qt::CaseInsensitive);
	QRegExp regExpVersionOld("\\bAvs2YUV (\\d+).(\\d+)\\b", Qt::CaseInsensitive);
	
	bool bTimeout = false;
	bool bAborted = false;

	unsigned int ver_maj = UINT_MAX;
	unsigned int ver_min = UINT_MAX;
	unsigned int ver_mod = 0;

	while(process.state() != QProcess::NotRunning)
	{
		if(m_abort)
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
				qWarning("Avs2YUV process timed out <-- killing!");
				log("\nPROCESS TIMEOUT !!!");
				bTimeout = true;
				break;
			}
		}
		while(process.bytesAvailable() > 0)
		{
			QList<QByteArray> lines = process.readLine().split('\r');
			while(!lines.isEmpty())
			{
				QString text = QString::fromUtf8(lines.takeFirst().constData()).simplified();
				int offset = -1;
				if((ver_maj == UINT_MAX) || (ver_min == UINT_MAX) || (ver_mod == UINT_MAX))
				{
					if(!text.isEmpty())
					{
						log(text);
					}
				}
				if((offset = regExpVersionMod.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false, ok3 = false;
					unsigned int temp1 = regExpVersionMod.cap(1).toUInt(&ok1);
					unsigned int temp2 = regExpVersionMod.cap(2).toUInt(&ok2);
					unsigned int temp3 = regExpVersionMod.cap(3).toUInt(&ok3);
					if(ok1) ver_maj = temp1;
					if(ok2) ver_min = temp2;
					if(ok3) ver_mod = temp3;
				}
				else if((offset = regExpVersionOld.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false;
					unsigned int temp1 = regExpVersionOld.cap(1).toUInt(&ok1);
					unsigned int temp2 = regExpVersionOld.cap(2).toUInt(&ok2);
					if(ok1) ver_maj = temp1;
					if(ok2) ver_min = temp2;
				}
			}
		}
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}

	if(bTimeout || bAborted || ((process.exitCode() != EXIT_SUCCESS) && (process.exitCode() != 2)))
	{
		if(!(bTimeout || bAborted))
		{
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(process.exitCode())));
		}
		return UINT_MAX;
	}

	if((ver_maj == UINT_MAX) || (ver_min == UINT_MAX))
	{
		log(tr("\nFAILED TO DETERMINE AVS2YUV VERSION !!!"));
		return UINT_MAX;
	}
	
	return (ver_maj * REV_MULT) + ((ver_min % REV_MULT) * 10) + (ver_mod % 10);
}

bool EncodeThread::checkVersionVapoursynth(void)
{
	//Is VapourSynth available at all?
	if((!m_sysinfo->hasVPSSupport()) || (!QFileInfo(VPS_BINARY(m_sysinfo, m_preferences)).isFile()))
	{
		log(tr("\nVPY INPUT REQUIRES VAPOURSYNTH, BUT IT IS *NOT* AVAILABLE !!!"));
		return false;
	}

	QProcess process;

	log("\nCreating process:");
	if(!startProcess(process, VPS_BINARY(m_sysinfo, m_preferences), QStringList()))
	{
		return false;;
	}

	QRegExp regExpSignature("\\bVSPipe\\s+usage\\b", Qt::CaseInsensitive);
	
	bool bTimeout = false;
	bool bAborted = false;

	bool vspipeSignature = false;

	while(process.state() != QProcess::NotRunning)
	{
		if(m_abort)
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
				qWarning("VSPipe process timed out <-- killing!");
				log("\nPROCESS TIMEOUT !!!");
				bTimeout = true;
				break;
			}
		}
		while(process.bytesAvailable() > 0)
		{
			QList<QByteArray> lines = process.readLine().split('\r');
			while(!lines.isEmpty())
			{
				QString text = QString::fromUtf8(lines.takeFirst().constData()).simplified();
				if(regExpSignature.lastIndexIn(text) >= 0)
				{
					vspipeSignature = true;
				}
				if(!text.isEmpty())
				{
					log(text);
				}
			}
		}
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}

	if(bTimeout || bAborted || ((process.exitCode() != EXIT_SUCCESS) && (process.exitCode() != 1)))
	{
		if(!(bTimeout || bAborted))
		{
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(process.exitCode())));
		}
		return false;
	}

	if(!vspipeSignature)
	{
		log(tr("\nFAILED TO DETECT VSPIPE SIGNATURE !!!"));
		return false;
	}
	
	return vspipeSignature;
}

bool EncodeThread::checkPropertiesAVS(unsigned int &frames)
{
	QProcess process;
	QStringList cmdLine;

	if(!m_options->customAvs2YUV().isEmpty())
	{
		cmdLine.append(splitParams(m_options->customAvs2YUV()));
	}

	cmdLine << "-frames" << "1";
	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFileName, true)) << "NUL";

	log("Creating process:");
	if(!startProcess(process, AVS_BINARY(m_sysinfo, m_preferences), cmdLine))
	{
		return false;;
	}

	QRegExp regExpInt(": (\\d+)x(\\d+), (\\d+) fps, (\\d+) frames");
	QRegExp regExpFrc(": (\\d+)x(\\d+), (\\d+)/(\\d+) fps, (\\d+) frames");
	
	QTextCodec *localCodec = QTextCodec::codecForName("System");

	bool bTimeout = false;
	bool bAborted = false;

	frames = 0;
	
	unsigned int fpsNom = 0;
	unsigned int fpsDen = 0;
	unsigned int fSizeW = 0;
	unsigned int fSizeH = 0;
	
	unsigned int waitCounter = 0;

	while(process.state() != QProcess::NotRunning)
	{
		if(m_abort)
		{
			process.kill();
			bAborted = true;
			break;
		}
		if(!process.waitForReadyRead(m_processTimeoutInterval))
		{
			if(process.state() == QProcess::Running)
			{
				if(++waitCounter > m_processTimeoutMaxCounter)
				{
					if(m_preferences->getAbortOnTimeout())
					{
						process.kill();
						qWarning("Avs2YUV process timed out <-- killing!");
						log("\nPROCESS TIMEOUT !!!");
						log("\nAvisynth has encountered a deadlock or your script takes EXTREMELY long to initialize!");
						bTimeout = true;
						break;
					}
				}
				else if(waitCounter == m_processTimeoutWarning)
				{
					unsigned int timeOut = (waitCounter * m_processTimeoutInterval) / 1000U;
					log(tr("Warning: Avisynth did not respond for %1 seconds, potential deadlock...").arg(QString::number(timeOut)));
				}
			}
			continue;
		}
		
		waitCounter = 0;
		
		while(process.bytesAvailable() > 0)
		{
			QList<QByteArray> lines = process.readLine().split('\r');
			while(!lines.isEmpty())
			{
				QString text = localCodec->toUnicode(lines.takeFirst().constData()).simplified();
				int offset = -1;
				if((offset = regExpInt.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false;
					bool ok3 = false, ok4 = false;
					unsigned int temp1 = regExpInt.cap(1).toUInt(&ok1);
					unsigned int temp2 = regExpInt.cap(2).toUInt(&ok2);
					unsigned int temp3 = regExpInt.cap(3).toUInt(&ok3);
					unsigned int temp4 = regExpInt.cap(4).toUInt(&ok4);
					if(ok1) fSizeW = temp1;
					if(ok2) fSizeH = temp2;
					if(ok3) fpsNom = temp3;
					if(ok4) frames = temp4;
				}
				else if((offset = regExpFrc.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false;
					bool ok3 = false, ok4 = false, ok5 = false;
					unsigned int temp1 = regExpFrc.cap(1).toUInt(&ok1);
					unsigned int temp2 = regExpFrc.cap(2).toUInt(&ok2);
					unsigned int temp3 = regExpFrc.cap(3).toUInt(&ok3);
					unsigned int temp4 = regExpFrc.cap(4).toUInt(&ok4);
					unsigned int temp5 = regExpFrc.cap(5).toUInt(&ok5);
					if(ok1) fSizeW = temp1;
					if(ok2) fSizeH = temp2;
					if(ok3) fpsNom = temp3;
					if(ok4) fpsDen = temp4;
					if(ok5) frames = temp5;
				}
				if(!text.isEmpty())
				{
					log(text);
				}
				if(text.contains("failed to load avisynth.dll", Qt::CaseInsensitive))
				{
					log(tr("\nWarning: It seems that %1-Bit Avisynth is not currently installed !!!").arg(m_preferences->getUseAvisyth64Bit() ? "64" : "32"));
				}
				if(text.contains(QRegExp("couldn't convert input clip to (YV16|YV24)", Qt::CaseInsensitive)))
				{
					log(tr("\nWarning: YV16 (4:2:2) and YV24 (4:4:4) color-spaces only supported in Avisynth 2.6 !!!"));
				}
			}
		}
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}

	if(bTimeout || bAborted || process.exitCode() != EXIT_SUCCESS)
	{
		if(!(bTimeout || bAborted))
		{
			const int exitCode = process.exitCode();
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(exitCode)));
			if((exitCode < 0) || (exitCode >= 32))
			{
				log(tr("\nIMPORTANT: The Avs2YUV process terminated abnormally. This means Avisynth or one of your Avisynth-Plugin's just crashed."));
				log(tr("IMPORTANT: Please fix your Avisynth script and try again! If you use Avisynth-MT, try using a *stable* Avisynth instead!"));
			}
		}
		return false;
	}

	if(frames == 0)
	{
		log(tr("\nFAILED TO DETERMINE AVS PROPERTIES !!!"));
		return false;
	}
	
	log("");

	if((fSizeW > 0) && (fSizeH > 0))
	{
		log(tr("Resolution: %1x%2").arg(QString::number(fSizeW), QString::number(fSizeH)));
	}
	if((fpsNom > 0) && (fpsDen > 0))
	{
		log(tr("Frame Rate: %1/%2").arg(QString::number(fpsNom), QString::number(fpsDen)));
	}
	if((fpsNom > 0) && (fpsDen == 0))
	{
		log(tr("Frame Rate: %1").arg(QString::number(fpsNom)));
	}
	if(frames > 0)
	{
		log(tr("No. Frames: %1").arg(QString::number(frames)));
	}

	return true;
}

bool EncodeThread::checkPropertiesVPS(unsigned int &frames)
{
	QProcess process;
	QStringList cmdLine;

	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFileName, true));
	cmdLine << "-" << "-info";

	log("Creating process:");
	if(!startProcess(process, VPS_BINARY(m_sysinfo, m_preferences), cmdLine))
	{
		return false;;
	}

	QRegExp regExpFrm("\\bFrames:\\s+(\\d+)\\b");
	QRegExp regExpSzW("\\bWidth:\\s+(\\d+)\\b");
	QRegExp regExpSzH("\\bHeight:\\s+(\\d+)\\b");
	
	QTextCodec *localCodec = QTextCodec::codecForName("System");

	bool bTimeout = false;
	bool bAborted = false;

	frames = 0;
	
	unsigned int fSizeW = 0;
	unsigned int fSizeH = 0;
	
	unsigned int waitCounter = 0;

	while(process.state() != QProcess::NotRunning)
	{
		if(m_abort)
		{
			process.kill();
			bAborted = true;
			break;
		}
		if(!process.waitForReadyRead(m_processTimeoutInterval))
		{
			if(process.state() == QProcess::Running)
			{
				if(++waitCounter > m_processTimeoutMaxCounter)
				{
					if(m_preferences->getAbortOnTimeout())
					{
						process.kill();
						qWarning("VSPipe process timed out <-- killing!");
						log("\nPROCESS TIMEOUT !!!");
						log("\nVapoursynth has encountered a deadlock or your script takes EXTREMELY long to initialize!");
						bTimeout = true;
						break;
					}
				}
				else if(waitCounter == m_processTimeoutWarning)
				{
					unsigned int timeOut = (waitCounter * m_processTimeoutInterval) / 1000U;
					log(tr("Warning: nVapoursynth did not respond for %1 seconds, potential deadlock...").arg(QString::number(timeOut)));
				}
			}
			continue;
		}
		
		waitCounter = 0;
		
		while(process.bytesAvailable() > 0)
		{
			QList<QByteArray> lines = process.readLine().split('\r');
			while(!lines.isEmpty())
			{
				QString text = localCodec->toUnicode(lines.takeFirst().constData()).simplified();
				int offset = -1;
				if((offset = regExpFrm.lastIndexIn(text)) >= 0)
				{
					bool ok = false;
					unsigned int temp = regExpFrm.cap(1).toUInt(&ok);
					if(ok) frames = temp;
				}
				if((offset = regExpSzW.lastIndexIn(text)) >= 0)
				{
					bool ok = false;
					unsigned int temp = regExpSzW.cap(1).toUInt(&ok);
					if(ok) fSizeW = temp;
				}
				if((offset = regExpSzH.lastIndexIn(text)) >= 0)
				{
					bool ok = false;
					unsigned int temp = regExpSzH.cap(1).toUInt(&ok);
					if(ok) fSizeH = temp;
				}
				if(!text.isEmpty())
				{
					log(text);
				}
			}
		}
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}

	if(bTimeout || bAborted || process.exitCode() != EXIT_SUCCESS)
	{
		if(!(bTimeout || bAborted))
		{
			const int exitCode = process.exitCode();
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(exitCode)));
			if((exitCode < 0) || (exitCode >= 32))
			{
				log(tr("\nIMPORTANT: The Vapoursynth process terminated abnormally. This means Vapoursynth or one of your Vapoursynth-Plugin's just crashed."));
			}
		}
		return false;
	}

	if(frames == 0)
	{
		log(tr("\nFAILED TO DETERMINE VPY PROPERTIES !!!"));
		return false;
	}
	
	log("");

	if((fSizeW > 0) && (fSizeH > 0))
	{
		log(tr("Resolution: %1x%2").arg(QString::number(fSizeW), QString::number(fSizeH)));
	}
	if(frames > 0)
	{
		log(tr("No. Frames: %1").arg(QString::number(frames)));
	}

	return true;
}
*/
