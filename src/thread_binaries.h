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

#pragma once

#include "thread_startup.h"

//Qt
#include <QMutex>

class QLibrary;
class SysinfoModel;
class QFile;

class BinariesCheckThread : public StarupThread
{
	Q_OBJECT

public:
	static bool check(const SysinfoModel *const sysinfo, QString *const failedPath = NULL);

protected:
	BinariesCheckThread(const SysinfoModel *const sysinfo);
	~BinariesCheckThread(void);
	
	const QString& getFailedPath(void) { return m_failedPath; }

private:
	QString m_failedPath;
	const SysinfoModel *const m_sysinfo;

	static const size_t MAX_BINARIES = 32;
	static QMutex m_binLock;
	static QScopedPointer<QFile> m_binPath[MAX_BINARIES];

	//Entry point
	virtual void run(void);

	//Thread main
	virtual int threadMain(void);
};
