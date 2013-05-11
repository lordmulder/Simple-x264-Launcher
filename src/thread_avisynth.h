///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2013 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <QMutex>

class QLibrary;

class AvisynthCheckThread : public QThread
{
	Q_OBJECT

public:
	AvisynthCheckThread(void);
	~AvisynthCheckThread(void);

	static int detect(volatile double *version);
	static void unload(void);

	bool getSuccess(void) { return m_success; }
	bool getException(void) { return m_exception; }
	double getVersion(void) { return m_version; }

private slots:
	void start(Priority priority = InheritPriority) { QThread::start(priority); }

private:
	volatile bool m_exception;
	volatile bool m_success;
	volatile double m_version;

	static QMutex m_avsLock;
	static QLibrary *m_avsLib;
	
	//Entry point
	virtual void run(void);

	//Functions
	static bool detectAvisynthVersion1(volatile double *version_number, volatile bool *exception);
	static bool detectAvisynthVersion2(volatile double *version_number, volatile bool *exception);
	static bool detectAvisynthVersion3(volatile double *version_number);
};
