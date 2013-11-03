///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2013 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include <cstdlib>

//Debug build
#if defined(_DEBUG) && defined(QT_DEBUG) && !defined(NDEBUG) && !defined(QT_NO_DEBUG)
	#define X264_DEBUG (1)
#else
	#define X264_DEBUG (0)
#endif

//Memory check
#if X264_DEBUG
#define X264_MEMORY_CHECK(FUNC, RETV,  ...) \
{ \
	SIZE_T _privateBytesBefore = x264_dbg_private_bytes(); \
	RETV = FUNC(__VA_ARGS__); \
	SIZE_T _privateBytesLeak = (x264_dbg_private_bytes() - _privateBytesBefore) / 1024; \
	if(_privateBytesLeak > 0) { \
		char _buffer[128]; \
		_snprintf_s(_buffer, 128, _TRUNCATE, "Memory leak: Lost %u KiloBytes of PrivateUsage memory.\n", _privateBytesLeak); \
		OutputDebugStringA("----------\n"); \
		OutputDebugStringA(_buffer); \
		OutputDebugStringA("----------\n"); \
	} \
}
#else
#define X264_MEMORY_CHECK(FUNC, RETV,  ...) \
{ \
	RETV = __noop(__VA_ARGS__); \
}
#endif

//Helper macros
#define QWCHAR(STR) reinterpret_cast<const wchar_t*>(STR.utf16())
#define QUTF8(STR) ((STR).toUtf8().constData())
#define WCHAR2QSTR(STR) (QString::fromUtf16(reinterpret_cast<const unsigned short*>((STR))))
#define X264_BOOL(X) ((X) ? "1" : "0")
#define X264_DELETE(PTR) if(PTR) { delete PTR; PTR = NULL; }
#define X264_DELETE_ARRAY(PTR) if(PTR) { delete [] PTR; PTR = NULL; }
#define _X264_MAKE_STRING_(X) #X
#define X264_MAKE_STRING(X) _X264_MAKE_STRING_(X)
#define X264_COMPILER_WARNING(TXT) __pragma(message(__FILE__ "(" X264_MAKE_STRING(__LINE__) ") : warning: " TXT))

//Declarations
class QString;
class QStringList;
class QDate;
class QTime;
class QIcon;
class QWidget;
class LockedFile;
class QProcess;
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
	bool mmx2;
	bool sse;
	bool sse2;
	bool sse3;
	bool ssse3;
	char vendor[0x40];
	char brand[0x40];
	bool intel;
}
x264_cpu_t;

//OS version number
typedef struct _x264_os_version_t
{
	unsigned int versionMajor;
	unsigned int versionMinor;
	bool overrideFlag;

	//comparision operators
	inline bool operator== (const _x264_os_version_t &rhs) const { return (versionMajor == rhs.versionMajor) && (versionMinor == rhs.versionMinor); }
	inline bool operator!= (const _x264_os_version_t &rhs) const { return (versionMajor != rhs.versionMajor) || (versionMinor != rhs.versionMinor); }
	inline bool operator>  (const _x264_os_version_t &rhs) const { return (versionMajor > rhs.versionMajor) || ((versionMajor == rhs.versionMajor) && (versionMinor >  rhs.versionMinor)); }
	inline bool operator>= (const _x264_os_version_t &rhs) const { return (versionMajor > rhs.versionMajor) || ((versionMajor == rhs.versionMajor) && (versionMinor >= rhs.versionMinor)); }
	inline bool operator<  (const _x264_os_version_t &rhs) const { return (versionMajor < rhs.versionMajor) || ((versionMajor == rhs.versionMajor) && (versionMinor <  rhs.versionMinor)); }
	inline bool operator<= (const _x264_os_version_t &rhs) const { return (versionMajor < rhs.versionMajor) || ((versionMajor == rhs.versionMajor) && (versionMinor <= rhs.versionMinor)); }
}
x264_os_version_t;

//Beep types
typedef enum
{
	x264_beep_info = 0,
	x264_beep_warning = 1,
	x264_beep_error = 2
}
x264_beep_t;

//Known Windows versions
extern const x264_os_version_t x264_winver_win2k;
extern const x264_os_version_t x264_winver_winxp;
extern const x264_os_version_t x264_winver_xpx64;
extern const x264_os_version_t x264_winver_vista;
extern const x264_os_version_t x264_winver_win70;
extern const x264_os_version_t x264_winver_win80;
extern const x264_os_version_t x264_winver_win81;

//Functions
void x264_message_handler(QtMsgType type, const char *msg);
unsigned int x264_version_major(void);
unsigned int x264_version_minor(void);
unsigned int x264_version_build(void);
const QDate &x264_version_date(void);
bool x264_portable(void);
const QString &x264_data_path(void);
bool x264_is_prerelease(void);
const char *x264_version_time(void);
const char *x264_version_compiler(void);
const char *x264_version_arch(void);
void x264_init_console(int argc, char* argv[]);
bool x264_init_qt(int argc, char* argv[]);
x264_cpu_t x264_detect_cpu_features(const QStringList &argv);
bool x264_shutdown_computer(const QString &message, const unsigned long timeout, const bool forceShutdown);
void x264_fatal_exit(const wchar_t* exitMessage, const wchar_t* errorBoxMessage = NULL);
void x264_sleep(const unsigned int delay);
const QStringList &x264_arguments(void);
bool x264_suspendProcess(const QProcess *proc, const bool suspend);
size_t x264_dbg_private_bytes(void);
void x264_finalization(void);
QString x264_path2ansi(const QString &longPath);
bool x264_change_process_priority(const int priority);
bool x264_change_process_priority(const QProcess *proc, const int priority);
bool x264_change_process_priority(void *hProcess, const int priority);
bool x264_play_sound(const unsigned short uiSoundIdx, const bool bAsync, const wchar_t *alias = NULL);
unsigned int x264_process_id(void);
bool x264_enable_close_button(const QWidget *win, const bool bEnable);
void x264_blink_window(QWidget *poWindow, unsigned int count, unsigned int delay);
bool x264_bring_to_front(const QWidget *win);
bool x264_is_executable(const QString &path);
QString x264_query_reg_string(const bool bUser, const QString &path, const QString &name);
bool x264_beep(int beepType);
