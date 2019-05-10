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

#include "thread_abstract.h"

//MUtils
#include <MUtils/Global.h>

//Qt
#include <QDir>
#include <QElapsedTimer>
#include <QProcess>

//-------------------------------------
// Constructor
//-------------------------------------

AbstractThread::AbstractThread(void)
{
	m_exception = false;
	m_success = 0;
}

AbstractThread::~AbstractThread(void)
{
}

//-------------------------------------
// Thread entry point
//-------------------------------------

void AbstractThread::run(void)
{
	m_exception = false;
	m_success = 0;
	runChecked1(this, m_success, &m_exception);
}

void AbstractThread::runChecked1(AbstractThread *const thread, volatile int &success, volatile bool *exception)
{
#if !defined(_DEBUG)
	__try
	{
		return runChecked2(thread, success, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled structured exception in worker thread !!!");
	}
#else
	return runChecked2(thread, success, exception);
#endif
}

void AbstractThread::runChecked2(AbstractThread *const thread, volatile int &success, volatile bool *exception)
{
#if !defined(_DEBUG)
	try
	{
		success = thread->threadMain();
	}
	catch(const std::exception &e)
	{
		*exception = true;
		qWarning("Worker thread raised an C++ exception: %s", e.what());
	}
	catch(char *const msg)
	{
		*exception = true;
		qWarning("Worker thread raised an C++ exception: %s", msg);
	}
	catch(...)
	{
		*exception = true;
		qWarning("Worker thread raised an C++ exception!");
	}
#else
	success = thread->threadMain();
#endif
}
