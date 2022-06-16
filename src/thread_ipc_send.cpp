///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2022 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "thread_ipc_send.h"

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
// Constructor
////////////////////////////////////////////////////////////

IPCThread_Send::IPCThread_Send(MUtils::IPCChannel *const ipcChannel)
:
	m_ipcChannel(ipcChannel)
{
}

IPCThread_Send::~IPCThread_Send(void)
{
}

////////////////////////////////////////////////////////////
// Thread Main
////////////////////////////////////////////////////////////

void IPCThread_Send::run(void)
{
	setTerminationEnabled(true);
	AbstractThread::run();
}

int IPCThread_Send::threadMain(void)
{
	bool bSentFiles = false;
	const MUtils::OS::ArgumentMap &args = MUtils::OS::arguments();

	quint32 flags = 0;
	bool commandSent = false;

	//Handle flags
	if(args.contains(CLI_PARAM_FORCE_START))
	{
		flags = ((flags | IPC_FLAG_FORCE_START) & (~IPC_FLAG_FORCE_ENQUEUE));
	}
	if(args.contains(CLI_PARAM_FORCE_ENQUEUE))
	{
		flags = ((flags | IPC_FLAG_FORCE_ENQUEUE) & (~IPC_FLAG_FORCE_START));
	}

	//Process all command-line arguments
	if(args.contains(CLI_PARAM_ADD_FILE))
	{
		foreach(const QString &fileName, args.values(CLI_PARAM_ADD_FILE))
		{
			if(!m_ipcChannel->send(IPC_OPCODE_ADD_FILE, flags, QStringList() << fileName))
			{
				qWarning("Failed to send IPC message!");
			}
			commandSent = true;
		}
	}
	if(args.contains(CLI_PARAM_ADD_JOB))
	{
		foreach(const QString &options, args.values(CLI_PARAM_ADD_JOB))
		{
			const QStringList optionValues = options.split('|', QString::SkipEmptyParts);
			if(optionValues.count() == 3)
			{
				if(!m_ipcChannel->send(IPC_OPCODE_ADD_JOB, flags, optionValues))
				{
					qWarning("Failed to send IPC message!");
				}
			}
			else
			{
				qWarning("Invalid number of arguments for parameter \"--%s\" detected!", CLI_PARAM_ADD_JOB);
			}
			commandSent = true;
		}
	}

	//If no argument has been sent yet, send a ping!
	if(!commandSent)
	{
		if(!m_ipcChannel->send(IPC_OPCODE_PING, 0, QStringList()))
		{
			qWarning("Failed to send IPC message!");
		}
	}

	return 1;
}

////////////////////////////////////////////////////////////
// EVENTS
////////////////////////////////////////////////////////////

/*NONE*/