///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2019 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "thread_startup.h"

//MUtils
#include <MUtils/Global.h>

//Qt
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>

//-------------------------------------
// Constructor
//-------------------------------------

StarupThread::StarupThread(void)
{
	m_exception = false;
	m_success = 0;
}

StarupThread::~StarupThread(void)
{
}

//-------------------------------------
// Thread entry point
//-------------------------------------

void StarupThread::run(void)
{
	m_exception = false;
	m_success = 0;
	runChecked1(this, m_success, &m_exception);
}

void StarupThread::runChecked1(StarupThread *const thread, volatile int &success, volatile bool *exception)
{
	__try
	{
		return runChecked2(thread, success, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in startup thread !!!");
	}
}

void StarupThread::runChecked2(StarupThread *const thread, volatile int &success, volatile bool *exception)
{
	try
	{
		success = thread->threadMain();
	}
	catch(...)
	{
		*exception = true;
		qWarning("Startup thread raised an C++ exception!");
	}
}

//-------------------------------------
// Utility functions
//-------------------------------------

QStringList StarupThread::runProcess(const QString &exePath, const QStringList &arguments, const QStringList *const extraPaths)
{
	QProcess process;

	//Get file name
	const QString fileName = QFileInfo(exePath).fileName().toUpper();

	//Setup process object
	MUtils::init_process(process, QDir::tempPath(), true, extraPaths);

	//Try to start process
	process.start(exePath, arguments);
	if (!process.waitForStarted())
	{
		qWarning("Failed to launch %s -> %s", MUTILS_UTF8(fileName), MUTILS_UTF8(process.errorString()));
		return QStringList();
	}

	//Start the timer
	QElapsedTimer timer;
	timer.start();

	//Wait until process has finished
	QStringList processOutput;
	while (process.state() != QProcess::NotRunning)
	{
		process.waitForReadyRead(1250);
		while (process.canReadLine())
		{
			const QString line = QString::fromUtf8(process.readLine()).simplified();
			if (!line.isEmpty())
			{
				processOutput << line;
			}
		}
		if ((process.state() != QProcess::NotRunning) && timer.hasExpired(15000))
		{
			process.waitForFinished(125);
			if (process.state() != QProcess::NotRunning)
			{
				qWarning("%s process encountered a deadlock -> aborting now!", MUTILS_UTF8(fileName));
				break;
			}
		}
		else
		{
			QThread::yieldCurrentThread(); /*yield*/
		}
	}

	//Make sure process has terminated!
	process.waitForFinished(1250);
	if (process.state() != QProcess::NotRunning)
	{
		qWarning("%s process still running, going to kill it!", MUTILS_UTF8(fileName));
		process.kill();
		process.waitForFinished(-1);
	}

	//Read pending lines
	while (process.bytesAvailable() > 0)
	{
		const QString line = QString::fromUtf8(process.readLine()).simplified();
		if (!line.isEmpty())
		{
			processOutput << line;
		}
	}

	//Check exit code
	if (process.exitCode() != 0)
	{
		qWarning("%s failed with code 0x%08X -> discarding all output!", MUTILS_UTF8(fileName), process.exitCode());
		return QStringList();
	}

	return processOutput;
}
