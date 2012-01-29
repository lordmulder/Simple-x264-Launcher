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

#include <QDate>
#include <QTime>
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

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

EncodeThread::EncodeThread(const QString &sourceFileName, const QString &outputFileName, const OptionsModel *options, const QString &binDir)
:
	m_jobId(QUuid::createUuid()),
	m_sourceFileName(sourceFileName),
	m_outputFileName(outputFileName),
	m_options(new OptionsModel(*options)),
	m_binDir(binDir)
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
		encode();
	}
	catch(char *msg)
	{
		emit messageLogged(m_jobId, QString("EXCEPTION ERROR: ").append(QString::fromLatin1(msg)));
	}
	catch(...)
	{
		emit messageLogged(m_jobId, QString("EXCEPTION ERROR !!!"));
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

	QStringList cmdLine;
	QProcess process;

	cmdLine = buildCommandLine();

	log("Creating process:");
	if(!startProcess(process, QString("%1/x264.exe").arg(m_binDir), cmdLine))
	{
		emit statusChanged(m_jobId, JobStatus_Failed);
		return;
	}

	emit statusChanged(m_jobId, JobStatus_Running);
	QRegExp regExp("\\[(\\d+)\\.\\d+%\\].+frames");
	
	bool bTimeout = false;
	bool bAborted = false;

	while(process.state() != QProcess::NotRunning)
	{
		if(m_abort)
		{
			process.kill();
			bAborted = true;
			log("\nABORTED BY USER !!!");
			break;
		}
		process.waitForReadyRead(m_processTimeoutInterval);
		if(!process.bytesAvailable() && process.state() == QProcess::Running)
		{
			process.kill();
			qWarning("x264 process timed out <-- killing!");
			log("\nPROCESS TIMEOUT !!!");
			bTimeout = true;
			break;
		}
		while(process.bytesAvailable() > 0)
		{
			QByteArray line = process.readLine();
			QString text = QString::fromUtf8(line.constData()).simplified();
			if(regExp.lastIndexIn(text) >= 0)
			{
				bool ok = false;
				unsigned int progress = regExp.cap(1).toUInt(&ok);
				if(ok)
				{
					emit progressChanged(m_jobId, progress);
					emit detailsChanged(m_jobId, line);
				}
			}
			else if(!text.isEmpty())
			{
				log(text);
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
		emit statusChanged(m_jobId, JobStatus_Failed);
		return;
	}
	
	emit progressChanged(m_jobId, 100);
	emit statusChanged(m_jobId, JobStatus_Completed);
}

QStringList EncodeThread::buildCommandLine(void)
{
	QStringList cmdLine;

	cmdLine << "--crf" << QString::number(m_options->quantizer());
	
	if(m_options->tune().compare("none", Qt::CaseInsensitive))
	{
		cmdLine << "--tune" << m_options->tune().toLower();
	}
	
	cmdLine << "--preset" << m_options->preset().toLower();
	cmdLine << "--output" << m_outputFileName;
	cmdLine << m_sourceFileName;

	return cmdLine;
}

///////////////////////////////////////////////////////////////////////////////
// Misc functions
///////////////////////////////////////////////////////////////////////////////

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
