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

#include "ipc.h"

#include "global.h"

#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QStringList>

static const size_t MAX_STR_LEN = 1024;
static const size_t MAX_ARG_CNT = 3;
static const size_t MAX_ENTRIES = 16;

static const char *s_key_smemory = "{76BA750B-007B-48BD-BC2E-2DA8E77D3C77}";
static const char *s_key_sema_wr = "{B595F47C-0F0F-4B52-9F45-FF524BC5EEBD}";
static const char *s_key_sema_rd = "{D331CBB5-8BCD-4127-9105-E22281130C77}";

static const wchar_t *EMPTY_STRING = L"";

typedef struct
{
	struct
	{
		int command;
		wchar_t args[MAX_ARG_CNT][MAX_STR_LEN];
	}
	data[MAX_ENTRIES];
	size_t posRd;
	size_t posWr;
	size_t counter;
}
x264_ipc_t;

#define IS_FIRST_INSTANCE(X) ((X) > 0)

///////////////////////////////////////////////////////////////////////////////
// Send Thread
///////////////////////////////////////////////////////////////////////////////

IPCSendThread::IPCSendThread(IPC *ipc, const int &command, const QStringList &args)
:
	m_ipc(ipc), m_command(command), m_args(new QStringList(args))
{
	m_result = false;
}

IPCSendThread::~IPCSendThread(void)
{
	X264_DELETE(m_args);
}


void IPCSendThread::run(void)
{
	try
	{
		m_result = m_ipc->pushCommand(m_command, m_args);
	}
	catch(...)
	{
		m_result = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
// Receive Thread
///////////////////////////////////////////////////////////////////////////////

IPCReceiveThread::IPCReceiveThread(IPC *ipc)
:
	m_ipc(ipc)
{
	m_stopped = false;
}
	
void IPCReceiveThread::run(void)
{
	try
	{
		receiveLoop();
	}
	catch(...)
	{
		qWarning("Exception in IPC receive thread!");
	}
}

void IPCReceiveThread::receiveLoop(void)
{
	while(!m_stopped)
	{
		QStringList args;
		int command;
		if(m_ipc->popCommand(command, args, &m_stopped))
		{
			if((command >= 0) && (command < IPC::IPC_OPCODE_MAX))
			{
				emit receivedCommand(command, args);
			}
			else
			{
				qWarning("IPC: Received the unknown opcode %d", command);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// IPC Class
///////////////////////////////////////////////////////////////////////////////

IPC::IPC(void)
{
	m_initialized  = -1;
	m_sharedMemory = NULL;
	m_semaphoreWr  = NULL;
	m_semaphoreRd  = NULL;
	m_recvThread   = NULL;
}

IPC::~IPC(void)
{
	if(m_recvThread && m_recvThread->isRunning())
	{
		qWarning("Receive thread still running -> terminating!");
		m_recvThread->terminate();
		m_recvThread->wait();
	}
	
	X264_DELETE(m_recvThread);
	X264_DELETE(m_sharedMemory);
	X264_DELETE(m_semaphoreWr);
	X264_DELETE(m_semaphoreRd);
}

bool IPC::initialize(bool &firstInstance)
{
	firstInstance = false;

	if(m_initialized >= 0)
	{
		firstInstance = IS_FIRST_INSTANCE(m_initialized);
		return true;
	}

	m_semaphoreWr = new QSystemSemaphore(s_key_sema_wr, MAX_ENTRIES);
	m_semaphoreRd = new QSystemSemaphore(s_key_sema_rd, 0);

	if((m_semaphoreWr->error() != QSystemSemaphore::NoError) || (m_semaphoreRd->error() != QSystemSemaphore::NoError))
	{
		qWarning("IPC: Failed to created system semaphores!");
		return false;
	}

	m_sharedMemory = new QSharedMemory(s_key_smemory, this);

	if(m_sharedMemory->create(sizeof(x264_ipc_t)))
	{
		memset(m_sharedMemory->data(), 0, sizeof(x264_ipc_t));
		m_initialized = 1;
		firstInstance = IS_FIRST_INSTANCE(m_initialized);
		return true;
	}

	if(m_sharedMemory->error() == QSharedMemory::AlreadyExists)
	{
		qDebug("Not the first instance -> attaching to existing shared memory");
		if(m_sharedMemory->attach())
		{
			m_initialized = 0;
			firstInstance = IS_FIRST_INSTANCE(m_initialized);
			return true;
		}
	}

	qWarning("IPC: Failed to attach to the shared memory!");
	return false;
}

bool IPC::pushCommand(const int &command, const QStringList *args)
{
	if(m_initialized < 0)
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	if(!m_semaphoreWr->acquire())
	{
		qWarning("IPC: Failed to acquire semaphore!");
		return false;
	}

	if(!m_sharedMemory->lock())
	{
		qWarning("IPC: Failed to lock shared memory!");
		return false;
	}

	bool success = true;

	try
	{
		x264_ipc_t *memory = (x264_ipc_t*) m_sharedMemory->data();
		if(memory->counter < MAX_ENTRIES)
		{
			memory->data[memory->posWr].command = command;
			for(int i = 0; i < MAX_ARG_CNT; i++)
			{
				const wchar_t *current = (i < args->count()) ? ((const wchar_t*)((*args)[i].utf16())) : EMPTY_STRING;
				wcsncpy_s(memory->data[memory->posWr].args[i], MAX_STR_LEN, current, _TRUNCATE);
			}
			memory->posWr = (memory->posWr + 1) % MAX_ENTRIES;
			memory->counter++;
		}
		else
		{
			qWarning("IPC: Shared memory is full -> cannot push string!");
			success = false;
		}
	}
	catch(...)
	{
		/*ignore any exception*/
	}

	m_sharedMemory->unlock();

	if(success)
	{
		m_semaphoreRd->release();
	}

	return success;
}

bool IPC::popCommand(int &command, QStringList &args, volatile bool *abortFlag)
{
	command = -1;
	args.clear();

	if(m_initialized < 0)
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	if(!m_semaphoreRd->acquire())
	{
		qWarning("IPC: Failed to acquire semaphore!");
		return false;
	}

	if(!m_sharedMemory->lock())
	{
		qWarning("IPC: Failed to lock shared memory!");
		return false;
	}

	bool success = true;

	try
	{
		x264_ipc_t *memory = (x264_ipc_t*) m_sharedMemory->data();
		if(memory->counter > 0)
		{
			command = memory->data[memory->posRd].command;
			for(size_t i = 0; i < MAX_ARG_CNT; i++)
			{
				memory->data[memory->posRd].args[i][MAX_STR_LEN-1] = L'\0';
				const QString str = QString::fromUtf16((const ushort*)memory->data[memory->posRd].args[i]);
				if(!str.isEmpty()) args << str; else break;
			}
			memory->posRd = (memory->posRd + 1) % MAX_ENTRIES;
			memory->counter--;
		}
		else
		{
			if(!abortFlag)
			{
				qWarning("IPC: Shared memory is empty -> cannot pop string!");
			}
			success = false;
		}
	}
	catch(...)
	{
		/*ignore any exception*/
	}

	m_sharedMemory->unlock();

	if(success)
	{
		m_semaphoreWr->release();
	}

	return success;
}

bool IPC::sendAsync(const int &command, const QStringList &args, const int timeout)
{
	if(m_initialized < 0)
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	IPCSendThread sendThread(this, command, args);
	sendThread.start();

	if(!sendThread.wait(timeout))
	{
		qWarning("IPC send operation encountered timeout!");
		sendThread.terminate();
		sendThread.wait();
		return false;
	}

	return sendThread.result();
}

bool IPC::startListening(void)
{
	if(m_initialized < 0)
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	if(!m_recvThread)
	{
		m_recvThread = new IPCReceiveThread(this);
		connect(m_recvThread, SIGNAL(receivedCommand(int,QStringList)), this, SIGNAL(receivedCommand(int,QStringList)), Qt::QueuedConnection);
	}

	if(!m_recvThread->isRunning())
	{
		m_recvThread->start();
	}
	else
	{
		qWarning("Receive thread was already running!");
	}

	return true;
}

bool IPC::stopListening(void)
{
	if(m_initialized < 0)
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	if(m_recvThread && m_recvThread->isRunning())
	{
		m_recvThread->stop();
		m_semaphoreRd->release();

		if(!m_recvThread->wait(5000))
		{
			qWarning("Receive thread seems deadlocked -> terminating!");
			m_recvThread->terminate();
			m_recvThread->wait();
		}
	}
	else
	{
		qWarning("Receive thread was not running!");
	}

	return true;
}
