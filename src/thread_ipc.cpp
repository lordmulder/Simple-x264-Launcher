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

#include "thread_ipc.h"
#include "global.h"

#include <QSystemSemaphore>
#include <QSharedMemory>

static const char* s_key[3] = 
{
	"{53F0CAC8-75BC-449A-91DF-2417B2867087}",
	"{20BD6A5F-B510-4764-937D-EE6598271801}",
	"{F09C3D10-EC27-4F4D-BF9C-9957A23B8A70}"
};

IPCThread::IPCThread(void)
{
	m_ipcReady = m_firstInstance = m_abort = false;

	m_semaphore_r = new QSystemSemaphore(QString::fromLatin1(s_key[0]), 0);
	m_semaphore_w = new QSystemSemaphore(QString::fromLatin1(s_key[1]), 1);
	m_sharedMem = new QSharedMemory(QString::fromLatin1(s_key[2]));
}

IPCThread::~IPCThread(void)
{
	X264_DELETE(m_semaphore_r);
	X264_DELETE(m_semaphore_w);
	X264_DELETE(m_sharedMem);
}

bool IPCThread::initialize(bool *firstInstance)
{
	if(!m_ipcReady)
	{
		if((m_semaphore_r->error() == 0) && (m_semaphore_w->error() == 0) && (m_sharedMem->error() == 0))
		{
			if(m_sharedMem->create(sizeof(DWORD), QSharedMemory::ReadWrite))
			{
				m_firstInstance = m_ipcReady = true;
			}
			else
			{
				if(m_sharedMem->attach(QSharedMemory::ReadWrite))
				{
					m_ipcReady = true;
				}
			}
		}
	}
	
	if(firstInstance)
	{
		*firstInstance = m_firstInstance;
	}

	return m_ipcReady;
}

void IPCThread::notifyOtherInstance(void)
{
	if(!m_firstInstance)
	{
		m_semaphore_w->acquire();
		DWORD *pidPtr = reinterpret_cast<DWORD*>(m_sharedMem->data());
		*pidPtr = GetCurrentProcessId();
		m_semaphore_r->release();
	}
}

void IPCThread::start(Priority priority)
{
	m_abort = false;

	if(!m_ipcReady)
	{
		throw "IPC not initialized yet !!!";
		return;
	}
	
	if(!m_firstInstance)
	{
		qWarning("This is NOT the first instance!");
		return;
	}

	QThread::start(priority);
}

void IPCThread::setAbort(void)
{
	m_abort = true;
	if(m_semaphore_r)
	{
		m_semaphore_r->release();
	}
}

void IPCThread::run(void)
{
	forever
	{
		if(!m_semaphore_r->acquire())
		{
			qWarning("IPCThread: System error !!!");
			qWarning("IPCThread: %s\n", m_semaphore_r->errorString().toLatin1().constData());
			break;
		}
		if(m_abort)
		{
			break;
		}
		const DWORD procId = *reinterpret_cast<const DWORD*>(m_sharedMem->constData());
		m_semaphore_w->release();
		emit instanceCreated(procId);
	}
}
