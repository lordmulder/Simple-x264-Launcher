///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2018 LoRd_MuldeR <MuldeR2@GMX.de>
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

#pragma once

#include <QThread>

namespace MUtils
{
	class IPCChannel;
}

class IPCThread_Recv: public QThread
{
	Q_OBJECT

public:
	IPCThread_Recv(MUtils::IPCChannel *const ipcChannel);
	~IPCThread_Recv(void);

	void stop(void);

signals:
	void receivedCommand(const int &command, const QStringList &args, const quint32 &flags);

protected:
	volatile bool m_stopFlag;
	MUtils::IPCChannel *const m_ipcChannel;

	void run();
};
