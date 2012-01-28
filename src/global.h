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

#include "targetver.h"

//C++ includes
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

//Win32 includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//Debug build
#if defined(_DEBUG) && defined(QT_DEBUG) && !defined(NDEBUG) && !defined(QT_NO_DEBUG)
	#define X264_DEBUG (1)
#else
	#define X264_DEBUG (0)
#endif

//Memory leack checker
#if X264_DEBUG
#define X264_MEMORY_CHECK(CMD) \
{ \
	SIZE_T _privateBytesBefore = x264_dbg_private_bytes(); \
	CMD; \
	SIZE_T _privateBytesLeak = (x264_dbg_private_bytes() - _privateBytesBefore) / 1024; \
	if(_privateBytesLeak > 10) { \
		qWarning("Memory leak: Lost %u KiloBytes.", _privateBytesLeak); \
	} \
}
#else
#define X264_MEMORY_CHECK(CMD) CMD
#endif

//Helper macros
#define QWCHAR(STR) reinterpret_cast<const wchar_t*>(STR.utf16())
#define X264_BOOL(X) ((X) ? "1" : "0")
#define X264_DELETE(PTR) if(PTR) { delete PTR; PTR = NULL; }
#define X264_DELETE_ARRAY(PTR) if(PTR) { delete [] PTR; PTR = NULL; }

//Declarations
class QString;
class QStringList;
class QDate;
class QTime;
class QIcon;
class QWidget;
class LockedFile;
enum QtMsgType;

//Types definitions
typedef struct
{
	int family;
	int model;
	int stepping;
	int count;
	bool x64;
	bool mmx;
	bool sse;
	bool sse2;
	bool sse3;
	bool ssse3;
	char vendor[0x40];
	char brand[0x40];
	bool intel;
}
x264_cpu_t;

//Functions
LONG WINAPI x264_exception_handler(__in struct _EXCEPTION_POINTERS *ExceptionInfo);
void x264_invalid_param_handler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t);
void x264_message_handler(QtMsgType type, const char *msg);
unsigned int x264_version_major(void);
unsigned int x264_version_minor(void);
const QDate &x264_version_date(void);
const char *x264_version_time(void);
const char *x264_version_compiler(void);
const char *x264_version_arch(void);
void x264_init_console(int argc, char* argv[]);
bool x264_init_qt(int argc, char* argv[]);
x264_cpu_t x264_detect_cpu_features(int argc, char **argv);
SIZE_T x264_dbg_private_bytes(void);
void x264_finalization(void);
