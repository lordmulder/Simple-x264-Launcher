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
	typedef enum _VapourSynthFlags
	{
		VAPOURSYNTH_X86 = 0x1,
		VAPOURSYNTH_X64 = 0x2
	}
	VapourSynthFlags;

	VapourSynthCheckThread(void);
	~VapourSynthCheckThread(void);

	QString getPath(void) { return m_vpsPath; }

private:
	QString m_vpsPath;

	static QMutex m_vpsLock;
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];
	
	//Entry point
	virtual void run(void);

	//Thread main
	virtual int threadMain(void);

	//Internal functions
	static bool isVapourSynthComplete(const QString &vsCorePath, QFile *&vpsExeFile, QFile *&vpsDllFile);
	static bool checkVapourSynth(const QString &vspipePath);
};
