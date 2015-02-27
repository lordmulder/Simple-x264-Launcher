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
	static int detect(QString &path, int &vapourSynthType);

	static const int VAPOURSYNTH_OFF = 0;
	static const int VAPOURSYNTH_X86 = 1;
	static const int VAPOURSYNTH_X64 = 2;

protected:
	VapourSynthCheckThread(void);
	~VapourSynthCheckThread(void);

	int getSuccess(void) { return m_success; }
	bool getException(void) { return m_exception; }
	QString getPath(void) { return m_vpsPath; }

private slots:
	void start(Priority priority = InheritPriority) { QThread::start(priority); }

private:
	int m_success;
	volatile bool m_exception;
	QString m_vpsPath;

	static QMutex m_vpsLock;
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsExePath[2];
	static QScopedPointer<QFile> VapourSynthCheckThread::m_vpsDllPath[2];
	
	//Entry point
	virtual void run(void);

	//Functions
	static int detectVapourSynthPath1(QString &path, volatile bool *exception);
	static int detectVapourSynthPath2(QString &path, volatile bool *exception);
	static int detectVapourSynthPath3(QString &path);

	//Internal functions
	static bool isVapourSynthComplete(const QString &vsCorePath, QFile *&vpsExeFile, QFile *&vpsDllFile);
	static bool checkVapourSynth(const QString &vspipePath);
};
