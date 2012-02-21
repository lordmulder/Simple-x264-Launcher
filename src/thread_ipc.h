///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2012 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "global.h"
#include <QThread>

class QSystemSemaphore;
class QSharedMemory;

class IPCThread : public QThread
{
	Q_OBJECT

public:
	IPCThread(void);
	~IPCThread(void);

	void setAbort(void);
	bool initialize(bool *firstInstance = NULL);

signals:
	void instanceCreated(DWORD pid);

public slots:
	void start(Priority priority = InheritPriority);

protected:
	volatile bool m_abort;

	QSystemSemaphore *m_semaphore_r;
	QSystemSemaphore *m_semaphore_w;
	QSharedMemory *m_sharedMem;

	bool m_ipcReady;
	bool m_firstInstance;

	virtual void run(void);
};
