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

#include "ipc.h"

#include "global.h"

#include <QSharedMemory>
#include <QSystemSemaphore>
#include <QMutexLocker>
#include <QStringList>

///////////////////////////////////////////////////////////////////////////////
// Constants
///////////////////////////////////////////////////////////////////////////////

static const size_t MAX_STR_LEN = 1024;
static const size_t MAX_ARG_CNT = 3;
static const size_t MAX_ENTRIES = 16;

static const char *s_key_smemory = "{C10A332B-31F2-4A12-B521-420C7CCFFF1D}";
static const char *s_key_sema_wr = "{E20F7E3B-084F-45CF-8448-EBAF25D21BDD}";
static const char *s_key_sema_rd = "{8B816115-E846-4E2A-9E6B-4DAD400DB93D}";

static const wchar_t *EMPTY_STRING = L"";
static unsigned long TIMEOUT_MS = 12000;

typedef struct
{
	size_t versTag;
	size_t posRd;
	size_t posWr;
	size_t counter;
	struct
	{
		int command;
		wchar_t args[MAX_ARG_CNT][MAX_STR_LEN];
		unsigned int flags;
	}
	data[MAX_ENTRIES];
}
x264_ipc_t;

static size_t versionTag(void)
{
	return ((x264_version_major() & 0xFF) << 24) | ((x264_version_minor() & 0xFF) << 16) | (x264_version_build() & 0xFFFF);
}

#define IS_FIRST_INSTANCE(X) ((X) > 0)

///////////////////////////////////////////////////////////////////////////////
// IPC Base Class
///////////////////////////////////////////////////////////////////////////////

class IPCCore : public QObject
{
	friend class IPC;
	friend class IPCReceiveThread;
	friend class IPCSendThread;

public:
	bool initialize(bool &firstInstance);
	
	inline bool isInitialized(void)
	{
		return (m_initialized >= 0);
	}

protected:
	IPCCore(void);
	~IPCCore(void);

	bool popCommand(int &command, QStringList &args, unsigned int &flags);
	bool pushCommand(const int &command, const QStringList *args, const unsigned int &flags = 0);

	volatile int m_initialized;
	
	QSharedMemory *m_sharedMemory;
	QSystemSemaphore *m_semaphoreRd;
	QSystemSemaphore *m_semaphoreWr;
};

///////////////////////////////////////////////////////////////////////////////
// Send Thread
///////////////////////////////////////////////////////////////////////////////

IPCSendThread::IPCSendThread(IPCCore *ipc, const int &command, const QStringList &args, const unsigned int &flags)
:
	m_ipc(ipc), m_command(command), m_args(&args), m_flags(flags)
{
	m_result = false;
}

IPCSendThread::~IPCSendThread(void)
{
	/*nothing to do here*/
}

void IPCSendThread::run(void)
{
	try
	{
		m_result = m_ipc->pushCommand(m_command, m_args, m_flags);
	}
	catch(...)
	{
		m_result = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
// Receive Thread
///////////////////////////////////////////////////////////////////////////////

IPCReceiveThread::IPCReceiveThread(IPCCore *ipc)
:
	m_ipc(ipc)
{
	m_stopped = false;
}

IPCReceiveThread::~IPCReceiveThread(void)
{
	/*nothing to do here*/
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
		int command;
		unsigned int flags;
		QStringList args;

		if(m_ipc->popCommand(command, args, flags))
		{
			if(!m_stopped)
			{
				if((command >= 0) && (command < IPC_OPCODE_MAX))
				{
					emit receivedCommand(command, args, flags);
				}
				else
				{
					qWarning("IPC: Received the unknown opcode %d", command);
				}
			}
		}
		else
		{
			m_stopped = true;
			qWarning("IPC: Receive operation has failed -> stopping thread!");
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// IPC Core Class
///////////////////////////////////////////////////////////////////////////////

IPCCore::IPCCore(void)
{
	m_initialized  = -1;
	m_sharedMemory = NULL;
	m_semaphoreWr  = NULL;
	m_semaphoreRd  = NULL;
}

IPCCore::~IPCCore(void)
{
	X264_DELETE(m_sharedMemory);
	X264_DELETE(m_semaphoreWr);
	X264_DELETE(m_semaphoreRd);
}

bool IPCCore::initialize(bool &firstInstance)
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
		x264_ipc_t *memory = (x264_ipc_t*) m_sharedMemory->data();
		memset(memory, 0, sizeof(x264_ipc_t)); memory->versTag = versionTag();
		m_initialized = 1;
		firstInstance = IS_FIRST_INSTANCE(m_initialized);
		return true;
	}

	if(m_sharedMemory->error() == QSharedMemory::AlreadyExists)
	{
		qDebug("Not the first instance -> attaching to existing shared memory");
		if(m_sharedMemory->attach())
		{
			x264_ipc_t *memory = (x264_ipc_t*) m_sharedMemory->data();
			if(memory->versTag != versionTag())
			{
				qWarning("IPC: Version tag mismatch (0x%08x vs. 0x%08x) detected!", memory->versTag, versionTag());
				return false;
			}
			m_initialized = 0;
			firstInstance = IS_FIRST_INSTANCE(m_initialized);
			return true;
		}
	}

	qWarning("IPC: Failed to attach to the shared memory!");
	return false;
}

bool IPCCore::pushCommand(const int &command, const QStringList *args, const unsigned int &flags)
{
	if(m_initialized < 0)
	{
		THROW("IPC not initialized!");
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
				const wchar_t *current = (args && (i < args->count())) ? ((const wchar_t*)((*args)[i].utf16())) : EMPTY_STRING;
				wcsncpy_s(memory->data[memory->posWr].args[i], MAX_STR_LEN, current, _TRUNCATE);
			}
			memory->data[memory->posWr].flags = flags;
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

bool IPCCore::popCommand(int &command, QStringList &args, unsigned int &flags)
{
	command = -1;
	flags = 0;
	args.clear();

	if(m_initialized < 0)
	{
		THROW("IPC not initialized!");
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
			flags = memory->data[memory->posRd].flags;
			memory->posRd = (memory->posRd + 1) % MAX_ENTRIES;
			memory->counter--;
		}
		else
		{
			qWarning("IPC: Shared memory is empty -> cannot pop string!");
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

///////////////////////////////////////////////////////////////////////////////
// IPC Handler Class
///////////////////////////////////////////////////////////////////////////////

IPC::IPC(void)
:
	m_mutex(QMutex::Recursive)
{
	m_ipcCore = new IPCCore();
	m_recvThread = NULL;
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
	X264_DELETE(m_ipcCore);
}

bool IPC::initialize(bool &firstInstance)
{
	QMutexLocker lock(&m_mutex);
	return m_ipcCore->initialize(firstInstance);
}

bool IPC::sendAsync(const int &command, const QStringList &args, const unsigned int &flags)
{
	QMutexLocker lock(&m_mutex);

	if(!m_ipcCore->isInitialized())
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	IPCSendThread sendThread(m_ipcCore, command, args, flags);
	sendThread.start();

	if(!sendThread.wait(TIMEOUT_MS))
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
	QMutexLocker lock(&m_mutex);

	if(!m_ipcCore->isInitialized())
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	if(!m_recvThread)
	{
		m_recvThread = new IPCReceiveThread(m_ipcCore);
		connect(m_recvThread, SIGNAL(receivedCommand(int,QStringList,quint32)), this, SIGNAL(receivedCommand(int,QStringList,quint32)), Qt::QueuedConnection);
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
	QMutexLocker lock(&m_mutex);

	if(!m_ipcCore->isInitialized())
	{
		qWarning("Error: IPC not initialized yet!");
		return false;
	}

	if(m_recvThread && m_recvThread->isRunning())
	{
		m_recvThread->stop();
		sendAsync(IPC_OPCODE_MAX, QStringList()); //push dummy command to unblock thread!

		if(!m_recvThread->wait(TIMEOUT_MS))
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

bool IPC::isInitialized(void)
{
	QMutexLocker lock(&m_mutex);
	return m_ipcCore->isInitialized();
}

bool IPC::isListening(void)
{
	QMutexLocker lock(&m_mutex);
	return (isInitialized() && m_recvThread && m_recvThread->isRunning());
}
