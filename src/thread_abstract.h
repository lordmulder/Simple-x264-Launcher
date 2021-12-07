///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2021 LoRd_MuldeR <MuldeR2@GMX.de>
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

//Qt
#include <QThread>

class AbstractThread : public QThread
{
	Q_OBJECT

public:
	AbstractThread(void);
	~AbstractThread(void);

	bool getException(void) { return m_exception; }
	int getSuccess(void) { return m_success; }

protected:
	volatile int m_success;
	volatile bool m_exception;

	//Entry point
	virtual void run(void);
	
	//Error handling
	static void runChecked1(AbstractThread *const thread, volatile int &success, volatile bool *exception);
	static void runChecked2(AbstractThread *const thread, volatile int &success, volatile bool *exception);

	//Thread main
	virtual int threadMain(void) = 0;
};
