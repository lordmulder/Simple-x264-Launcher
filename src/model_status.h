///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2020 LoRd_MuldeR <MuldeR2@GMX.de>
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

enum JobStatus
{
	JobStatus_Enqueued = 0,
	JobStatus_Starting = 1,
	JobStatus_Indexing = 2,
	JobStatus_Running = 3,
	JobStatus_Running_Pass1 = 4,
	JobStatus_Running_Pass2 = 5,
	JobStatus_Completed = 6,
	JobStatus_Failed = 7,
	JobStatus_Pausing = 8,
	JobStatus_Paused = 9,
	JobStatus_Resuming = 10,
	JobStatus_Aborting = 11,
	JobStatus_Aborted = 12,
	JobStatus_Undefined = 666
};
