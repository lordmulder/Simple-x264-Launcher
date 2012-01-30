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

#include "thread_encode.h"

#include "global.h"
#include "model_options.h"
#include "version.h"

#include <QDate>
#include <QTime>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QMutex>
#include <QLibrary>

/*
 * Win32 API definitions
 */
typedef HANDLE (WINAPI *CreateJobObjectFun)(__in_opt LPSECURITY_ATTRIBUTES lpJobAttributes, __in_opt LPCSTR lpName);
typedef BOOL (WINAPI *SetInformationJobObjectFun)(__in HANDLE hJob, __in JOBOBJECTINFOCLASS JobObjectInformationClass, __in_bcount(cbJobObjectInformationLength) LPVOID lpJobObjectInformation, __in DWORD cbJobObjectInformationLength);
typedef BOOL (WINAPI *AssignProcessToJobObjectFun)(__in HANDLE hJob, __in HANDLE hProcess);

/*
 * Static vars
 */
QMutex EncodeThread::m_mutex_startProcess;
HANDLE EncodeThread::m_handle_jobObject = NULL;

/*
 * Macros
 */
#define CHECK_STATUS(ABORT_FLAG, OK_FLAG) \
{ \
	if(ABORT_FLAG) \
	{ \
		log("\nPROCESS ABORTED BY USER !!!"); \
		setStatus(JobStatus_Aborted); \
		return; \
	} \
	else if(!(OK_FLAG)) \
	{ \
		setStatus(JobStatus_Failed); \
		return; \
	} \
}

/*
 * Static vars
 */
static const unsigned int REV_MULT = 10000;

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

EncodeThread::EncodeThread(const QString &sourceFileName, const QString &outputFileName, const OptionsModel *options, const QString &binDir, bool x64)
:
	m_jobId(QUuid::createUuid()),
	m_sourceFileName(sourceFileName),
	m_outputFileName(outputFileName),
	m_options(new OptionsModel(*options)),
	m_binDir(binDir),
	m_x64(x64)
{
	m_abort = false;
}

EncodeThread::~EncodeThread(void)
{
	X264_DELETE(m_options);
}

///////////////////////////////////////////////////////////////////////////////
// Thread entry point
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::run(void)
{
	try
	{
		m_progress = 0;
		m_status = JobStatus_Starting;
		encode();
	}
	catch(char *msg)
	{
		log(tr("EXCEPTION ERROR: ").append(QString::fromLatin1(msg)));
	}
	catch(...)
	{
		log(tr("EXCEPTION ERROR !!!"));
	}
}

///////////////////////////////////////////////////////////////////////////////
// Encode functions
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::encode(void)
{
	Sleep(500);

	//Print some basic info
	log(tr("Job started at %1, %2.\n").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
	log(tr("Source file: %1").arg(m_sourceFileName));
	log(tr("Output file: %1").arg(m_outputFileName));
	log(tr("\n[Encoder Options]"));
	log(tr("RC Mode: %1").arg(OptionsModel::rcMode2String(m_options->rcMode())));
	log(tr("Preset: %1").arg(m_options->preset()));
	log(tr("Tuning: %1").arg(m_options->tune()));
	log(tr("Profile: %1").arg(m_options->profile()));
	log(tr("Custom: %1").arg(m_options->custom().isEmpty() ? tr("(None)") : m_options->custom()));
	
	//Detect source info
	log(tr("\n[Input Properties]"));
	log(tr("Not implemented yet, sorry ;-)\n"));

	bool ok = false;

	//Checking version
	log(tr("--- VERSION ---\n"));
	unsigned int revision;
	ok = ((revision = checkVersion(m_x64)) != UINT_MAX);
	CHECK_STATUS(m_abort, ok);

	//Is revision supported?
	log(tr("\nx264 revision: %1 (core #%2)").arg(QString::number(revision % REV_MULT), QString::number(revision / REV_MULT)));
	if((revision % REV_MULT) < VER_X264_MINIMUM_REV)
	{
		log(tr("\nERROR: Your revision of x264 is too old! (Minimum required revision is %2)").arg(QString::number(VER_X264_MINIMUM_REV)));
		setStatus(JobStatus_Failed);
		return;
	}
	if((revision / REV_MULT) != VER_X264_CURRENT_API)
	{
		log(tr("\nWARNING: Your revision of x264 uses an unsupported core (API) version, take care!"));
		log(tr("This application works best with x264 core (API) version %2.").arg(QString::number(VER_X264_CURRENT_API)));
	}

	//Run encoding passes
	if(m_options->rcMode() == OptionsModel::RCMode_2Pass)
	{
		QFileInfo info(m_outputFileName);
		QString passLogFile = QString("%1/%2.stats").arg(info.path(), info.completeBaseName());

		if(QFileInfo(passLogFile).exists())
		{
			int n = 2;
			while(QFileInfo(passLogFile).exists())
			{
				passLogFile = QString("%1/%2.%3.stats").arg(info.path(), info.completeBaseName(), QString::number(n++));
			}
		}
		
		log(tr("\n--- PASS 1 ---\n"));
		ok = runEncodingPass(m_x64, 1, passLogFile);
		CHECK_STATUS(m_abort, ok);

		log(tr("\n--- PASS 2 ---\n"));
		ok = runEncodingPass(m_x64,2, passLogFile);
		CHECK_STATUS(m_abort, ok);
	}
	else
	{
		log(tr("\n--- ENCODING ---\n"));
		ok = runEncodingPass(m_x64);
		CHECK_STATUS(m_abort, ok);
	}

	log(tr("\n--- DONE ---\n"));
	log(tr("Job finished at %1, %2.\n").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
	setStatus(JobStatus_Completed);
}

bool EncodeThread::runEncodingPass(bool x64, int pass, const QString &passLogFile)
{
	QProcess process;
	QStringList cmdLine = buildCommandLine(pass, passLogFile);

	log("Creating process:");
	if(!startProcess(process, QString("%1/%2.exe").arg(m_binDir, x64 ? "x264_x64" : "x264"), cmdLine))
	{
		return false;;
	}

	QRegExp regExpIndexing("indexing.+\\[(\\d+)\\.\\d+%\\]");
	QRegExp regExpProgress("\\[(\\d+)\\.\\d+%\\].+frames");
	
	bool bTimeout = false;
	bool bAborted = false;

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
				process.kill();
				qWarning("x264 process timed out <-- killing!");
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
				if((offset = regExpProgress.lastIndexIn(text)) >= 0)
				{
					bool ok = false;
					unsigned int progress = regExpProgress.cap(1).toUInt(&ok);
					setStatus((pass == 2) ? JobStatus_Running_Pass2 : ((pass == 1) ? JobStatus_Running_Pass1 : JobStatus_Running));
					setDetails(text.mid(offset).trimmed());
					if(ok) setProgress(progress);
				}
				else if((offset = regExpIndexing.lastIndexIn(text)) >= 0)
				{
					bool ok = false;
					unsigned int progress = regExpIndexing.cap(1).toUInt(&ok);
					setStatus(JobStatus_Indexing);
					setDetails(text.mid(offset).trimmed());
					if(ok) setProgress(progress);
				}
				else if(!text.isEmpty())
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
		return false;
	}
	
	setStatus((pass == 2) ? JobStatus_Running_Pass2 : ((pass == 1) ? JobStatus_Running_Pass1 : JobStatus_Running));
	setProgress(100);
	return true;
}

QStringList EncodeThread::buildCommandLine(int pass, const QString &passLogFile)
{
	QStringList cmdLine;

	switch(m_options->rcMode())
	{
	case OptionsModel::RCMode_CRF:
		cmdLine << "--crf" << QString::number(m_options->quantizer());
		break;
	case OptionsModel::RCMode_CQ:
		cmdLine << "--qp" << QString::number(m_options->quantizer());
		break;
	case OptionsModel::RCMode_2Pass:
	case OptionsModel::RCMode_ABR:
		cmdLine << "--bitrate" << QString::number(m_options->bitrate());
		break;
	default:
		throw "Bad rate-control mode !!!";
		break;
	}
	
	if((pass == 1) || (pass == 2))
	{
		cmdLine << "--pass" << QString::number(pass);
		cmdLine << "--stats" << QDir::toNativeSeparators(passLogFile);
	}

	if(m_options->tune().compare("none", Qt::CaseInsensitive))
	{
		cmdLine << "--tune" << m_options->tune().toLower();
	}
	
	cmdLine << "--preset" << m_options->preset().toLower();

	if(!m_options->custom().isEmpty())
	{
		//FIXME: Handle custom parameters that contain spaces!
		cmdLine.append(m_options->custom().split(" "));
	}

	cmdLine << "--output" << QDir::toNativeSeparators(m_outputFileName);
	cmdLine << m_sourceFileName;

	return cmdLine;
}

unsigned int EncodeThread::checkVersion(bool x64)
{
	QProcess process;
	QStringList cmdLine = QStringList() << "--version";

	log("Creating process:");
	if(!startProcess(process, QString("%1/%2.exe").arg(m_binDir, x64 ? "x264_x64" : "x264"), cmdLine))
	{
		return false;;
	}

	QRegExp regExpVersion("x264 (\\d)\\.(\\d+)\\.(\\d+) ([0-9A-Fa-f]{7})");
	
	bool bTimeout = false;
	bool bAborted = false;

	unsigned int revision = UINT_MAX;
	unsigned int coreVers = UINT_MAX;

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
				process.kill();
				qWarning("x264 process timed out <-- killing!");
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
				if((offset = regExpVersion.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false;
					unsigned int temp1 = regExpVersion.cap(2).toUInt(&ok1);
					unsigned int temp2 = regExpVersion.cap(3).toUInt(&ok2);
					if(ok1) coreVers = temp1;
					if(ok2) revision = temp2;
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
		return UINT_MAX;
	}

	if((revision == UINT_MAX) || (coreVers == UINT_MAX))
	{
		log(tr("\nFAILED TO DETERMINE X264 VERSION !!!"));
		return UINT_MAX;
	}
	
	return (coreVers * REV_MULT) + revision;
}

///////////////////////////////////////////////////////////////////////////////
// Misc functions
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::setStatus(JobStatus newStatus)
{
	if(m_status != newStatus)
	{
		m_status = newStatus;
		if((newStatus != JobStatus_Completed) && (newStatus != JobStatus_Failed) && (newStatus != JobStatus_Aborted))
		{
			setProgress(0);
		}
		if(newStatus == JobStatus_Failed)
		{
			setDetails("The job has failed. See log for details!");
		}
		if(newStatus == JobStatus_Aborted)
		{
			setDetails("The job was aborted by the user!");
		}
		emit statusChanged(m_jobId, newStatus);
	}
}

void EncodeThread::setProgress(unsigned int newProgress)
{
	if(m_progress != newProgress)
	{
		m_progress = newProgress;
		emit progressChanged(m_jobId, m_progress);
	}
}

void EncodeThread::setDetails(const QString &text)
{
	emit detailsChanged(m_jobId, text);
}

bool EncodeThread::startProcess(QProcess &process, const QString &program, const QStringList &args)
{
	static AssignProcessToJobObjectFun AssignProcessToJobObjectPtr = NULL;
	
	QMutexLocker lock(&m_mutex_startProcess);
	log(commandline2string(program, args) + "\n");
	
	if(!AssignProcessToJobObjectPtr)
	{
		QLibrary Kernel32Lib("kernel32.dll");
		AssignProcessToJobObjectPtr = (AssignProcessToJobObjectFun) Kernel32Lib.resolve("AssignProcessToJobObject");
	}
	
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setReadChannel(QProcess::StandardOutput);
	process.start(program, args);
	
	if(process.waitForStarted())
	{
		
		if(AssignProcessToJobObjectPtr)
		{
			AssignProcessToJobObjectPtr(m_handle_jobObject, process.pid()->hProcess);
		}
		if(!SetPriorityClass(process.pid()->hProcess, BELOW_NORMAL_PRIORITY_CLASS))
		{
			SetPriorityClass(process.pid()->hProcess, IDLE_PRIORITY_CLASS);
		}
		
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

QString EncodeThread::commandline2string(const QString &program, const QStringList &arguments)
{
	QString commandline = (program.contains(' ') ? QString("\"%1\"").arg(program) : program);
	
	for(int i = 0; i < arguments.count(); i++)
	{
		commandline += (arguments.at(i).contains(' ') ? QString(" \"%1\"").arg(arguments.at(i)) : QString(" %1").arg(arguments.at(i)));
	}

	return commandline;
}
