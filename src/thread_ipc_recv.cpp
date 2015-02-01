///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version, but always including the *additional*
// restrictions defined in the "License.txt" file.
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

#include "thread_ipc_recv.h"

//Internal
#include "Global.h"
#include "cli.h"
#include "ipc.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/IPCChannel.h>
#include <MUtils/OSSupport.h>

//Qt
#include <QStringList>
#include <QApplication>
#include <QFileInfo>
#include <QDir>

//CRT
#include <limits.h>

////////////////////////////////////////////////////////////
// Constructor & Destructor
////////////////////////////////////////////////////////////

IPCThread_Recv::IPCThread_Recv(MUtils::IPCChannel *const ipcChannel)
:
	m_ipcChannel(ipcChannel)
{
	m_stopFlag = false;
}

IPCThread_Recv::~IPCThread_Recv(void)
{
}

////////////////////////////////////////////////////////////
// Thread Main
////////////////////////////////////////////////////////////

void IPCThread_Recv::run()
{
	setTerminationEnabled(true);
	QStringList params;
	quint32 command, flags;

	while(!m_stopFlag)
	{
		if(m_ipcChannel->read(command, flags, params))
		{
			if(command != IPC_OPCODE_NOOP)
			{
				emit receivedCommand(command, params, flags);
			}
		}
		else
		{
			qWarning("Failed to read next IPC message!");
			break;
		}
	}
}

////////////////////////////////////////////////////////////
// Public Methods
////////////////////////////////////////////////////////////

void IPCThread_Recv::stop(void)
{
	if(!m_stopFlag)
	{
		m_stopFlag = true;
		m_ipcChannel->send(0, 0, QStringList());
	}
}

////////////////////////////////////////////////////////////
// EVENTS
////////////////////////////////////////////////////////////

/*NONE*/