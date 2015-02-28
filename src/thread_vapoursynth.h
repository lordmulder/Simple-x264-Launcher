///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
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
class QFile;

class VapourSynthCheckThread : public QThread
{
	Q_OBJECT

public:
	typedef enum _VapourSynthType_t
	{
		VAPOURSYNTH_X86 = 0x1,
		VAPOURSYNTH_X64 = 0x2
	}
	VapourSynthType_t;

	typedef QFlags<VapourSynthType_t> VapourSynthType;
	static int detect(QString &path, VapourSynthType &type);

protected:
	VapourSynthCheckThread(void);
	~VapourSynthCheckThread(void);

	bool getException(void) { return m_exception; }
	VapourSynthType &getSuccess(void) { return m_success; }
	QString getPath(void) { return m_vpsPath; }

private slots:
	void start(Priority priority = InheritPriority) { QThread::start(priority); }

private:
	volatile bool m_exception;
	VapourSynthType m_success;
	QString m_vpsPath;

	static QMutex m_vpsLock;
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];
	
	//Entry point
	virtual void run(void);

	//Functions
	static void detectVapourSynthPath1(VapourSynthType &success, QString &path, volatile bool *exception);
	static void detectVapourSynthPath2(VapourSynthType &success, QString &path, volatile bool *exception);
	static void detectVapourSynthPath3(VapourSynthType &success, QString &path);

	//Internal functions
	static bool isVapourSynthComplete(const QString &vsCorePath, QFile *&vpsExeFile, QFile *&vpsDllFile);
	static bool checkVapourSynth(const QString &vspipePath);
};
