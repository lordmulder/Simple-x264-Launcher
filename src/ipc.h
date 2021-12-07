///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2021 LoRd_MuldeR <MuldeR2@GMX.de>
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

//IPC Commands
static const quint32 IPC_OPCODE_NOOP     = 0;
static const quint32 IPC_OPCODE_PING     = 1;
static const quint32 IPC_OPCODE_ADD_FILE = 2;
static const quint32 IPC_OPCODE_ADD_JOB  = 3;
static const quint32 IPC_OPCODE_MAX      = 4;

//IPC Flags
static const quint32 IPC_FLAG_FORCE_START   = 0x00000001;
static const quint32 IPC_FLAG_FORCE_ENQUEUE = 0x00000002;
