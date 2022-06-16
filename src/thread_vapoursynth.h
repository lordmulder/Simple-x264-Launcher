///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2022 LoRd_MuldeR <MuldeR2@GMX.de>
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
class QFileInfo;
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

	QString getPath32(void) { return m_vpsPath[0U]; }
	QString getPath64(void) { return m_vpsPath[1U]; }

private:
	QString m_vpsPath[2];

	static QMutex m_vpsLock;
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];
	
	//Entry point
	virtual void run(void);

	//Thread main
	virtual int threadMain(void);

	//Internal functions
	static VapourSynthFlags getVapourSynthType(const int scope);
	static bool isVapourSynthComplete(const QFileInfo& vpsDllInfo, const QFileInfo& vpsExeInfo, QFile*& vpsExeFile, QFile*& vpsDllFile);
	static bool checkVapourSynth(const QString &vspipePath);
};
