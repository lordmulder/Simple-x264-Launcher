///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2014 LoRd_MuldeR <MuldeR2@GMX.de>
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

class QSharedMemory;
class QStringList;
class QSystemSemaphore;

class IPCCore;
class IPCReceiveThread;
class IPCSendThread;

//IPC Commands
static const int IPC_OPCODE_PING     = 0;
static const int IPC_OPCODE_ADD_FILE = 1;
static const int IPC_OPCODE_ADD_JOB  = 2;
static const int IPC_OPCODE_MAX      = 3;

///////////////////////////////////////////////////////////////////////////////
// IPC Handler Class
///////////////////////////////////////////////////////////////////////////////

class IPC : public QObject
{
	Q_OBJECT

public:
	IPC(void);
	~IPC(void);

	bool initialize(bool &firstInstance);
	bool sendAsync(const int &command, const QStringList &args);
	bool isInitialized(void);
	bool isListening(void);

public slots:
	bool startListening(void);
	bool stopListening(void);

signals:
	void receivedCommand(const int &command, const QStringList &args);

protected:
	IPCCore *m_ipcCore;
	IPCReceiveThread *m_recvThread;
	QMutex m_mutex;
};

///////////////////////////////////////////////////////////////////////////////
// Utility Classes
///////////////////////////////////////////////////////////////////////////////

class IPCSendThread : public QThread
{
	Q_OBJECT
	friend class IPC;

protected:
	IPCSendThread(IPCCore *ipc, const int &command, const QStringList &args);
	IPCSendThread::~IPCSendThread(void);

	inline bool result(void) { return m_result; }
	virtual void run(void);

private:
	volatile bool m_result;
	IPCCore *const m_ipc;
	const int m_command;
	const QStringList *m_args;
};

class IPCReceiveThread : public QThread
{
	Q_OBJECT
	friend class IPC;

protected:
	IPCReceiveThread(IPCCore *ipc);

	inline void stop(void) { m_stopped = true; }
	virtual void run(void);

signals:
	void receivedCommand(const int &command, const QStringList &args);

private:
	void receiveLoop(void);
	volatile bool m_stopped;
	IPCCore *const m_ipc;
};
