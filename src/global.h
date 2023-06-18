///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2023 LoRd_MuldeR <MuldeR2@GMX.de>
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

#define _CRT_RAND_S
#include <cstdlib>
#include <stdexcept>

//Forward declarations
class QString;
class QStringList;
class QDate;
class QTime;
class QIcon;
class QWidget;
class LockedFile;
class QProcess;

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

//Utility functions
const QString &x264_data_path(void);
QString x264_path2ansi(const QString &longPath, bool makeLowercase = false);
bool x264_set_thread_execution_state(const bool systemRequired);

//Version getters
unsigned int x264_version_major(void);
unsigned int x264_version_minor(void);
unsigned int x264_version_build(void);
bool         x264_is_prerelease(void);
bool         x264_is_portable  (void);

///////////////////////////////////////////////////////////////////////////////
// HELPER MACROS
///////////////////////////////////////////////////////////////////////////////

//Check for CPU-compatibility options
#if !defined(_M_X64) && defined(_MSC_VER) && defined(_M_IX86_FP)
	#if (_M_IX86_FP != 0)
		#error We should not enabled SSE or SSE2 in release builds!
	#endif
#endif
