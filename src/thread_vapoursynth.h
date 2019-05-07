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

#pragma once

#include "thread_startup.h"

//Qt
#include <QMutex>

class QFile;
class SysinfoModel;

class VapourSynthCheckThread : public StarupThread
{
	Q_OBJECT

public:
	static bool detect(SysinfoModel *sysinfo);

protected:
	VapourSynthCheckThread(void);
	~VapourSynthCheckThread(void);

	bool getException(void) { return m_exception; }
	int getSuccess(void) { return m_success; }
	QString getPath(void) { return m_vpsPath; }

	typedef enum _VapourSynthFlags
	{
		VAPOURSYNTH_X86 = 0x1,
		VAPOURSYNTH_X64 = 0x2
	}
	VapourSynthFlags;

private slots:
	void start(Priority priority = InheritPriority) { QThread::start(priority); }

private:
	volatile bool m_exception;
	int m_success;
	QString m_vpsPath;

	static QMutex m_vpsLock;
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];
	
	//Entry point
	virtual void run(void);

	//Functions
	static void detectVapourSynthPath1(int &success, QString &path, volatile bool *exception);
	static void detectVapourSynthPath2(int &success, QString &path, volatile bool *exception);
	static void detectVapourSynthPath3(int &success, QString &path);

	//Internal functions
	static bool isVapourSynthComplete(const QString &vsCorePath, QFile *&vpsExeFile, QFile *&vpsDllFile);
	static bool checkVapourSynth(const QString &vspipePath);
};
