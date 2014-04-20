///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2014 LoRd_MuldeR <MuldeR2@GMX.de>
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
enum QtMsgType;

///////////////////////////////////////////////////////////////////////////////
// TYPE DEFINITIONS
///////////////////////////////////////////////////////////////////////////////

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

//Known folders
typedef enum
{
	x264_folder_localappdata = 0,
	x264_folder_programfiles = 2,
	x264_folder_systemfolder = 3,
	x264_folder_systroot_dir = 4
}
x264_known_folder_t;

//Network connection types
typedef enum
{
	x264_network_err = 0,	/*unknown*/
	x264_network_non = 1,	/*not connected*/
	x264_network_yes = 2	/*connected*/
}
x264_network_t;

//Known Windows versions
extern const x264_os_version_t x264_winver_win2k;
extern const x264_os_version_t x264_winver_winxp;
extern const x264_os_version_t x264_winver_xpx64;
extern const x264_os_version_t x264_winver_vista;
extern const x264_os_version_t x264_winver_win70;
extern const x264_os_version_t x264_winver_win80;
extern const x264_os_version_t x264_winver_win81;

//Exception class
class X264Exception : public std::runtime_error
{
public:
	X264Exception(const char *message, ...);
	virtual const char* what() const { return m_message; }
private:
	static const size_t MAX_MSGLEN = 256;
	char m_message[MAX_MSGLEN];
};

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

const QStringList &x264_arguments(void);
bool x264_beep(int beepType);
void x264_blink_window(QWidget *poWindow, unsigned int count, unsigned int delay);
bool x264_bring_process_to_front(const unsigned long pid);
bool x264_bring_to_front(const QWidget *win);
bool x264_change_process_priority(const QProcess *proc, const int priority);
bool x264_change_process_priority(const int priority);
bool x264_change_process_priority(void *hProcess, const int priority);
QDate x264_current_date_safe(void);
const QString &x264_data_path(void);
void x264_dbg_output_string(const char* format, ...);
size_t x264_dbg_private_bytes(void);
x264_cpu_t x264_detect_cpu_features(const int argc, char **argv);
bool x264_enable_close_button(const QWidget *win, const bool bEnable);
void x264_fatal_exit(const wchar_t* exitMessage, const char* errorBoxMessage = NULL);
void x264_finalization(void);
void x264_init_console(const int argc, char **argv);
void x264_init_process(QProcess &process, const QString &wokringDir, const bool bReplaceTempDir = true);
bool x264_init_qt(int &argc, char **argv);
bool x264_is_executable(const QString &path);
bool x264_is_prerelease(void);
const QString &x264_known_folder(x264_known_folder_t folder_id);
void x264_message_handler(QtMsgType type, const char *msg);
int x264_network_status(void);
QString x264_path2ansi(const QString &longPath, bool makeLowercase = false);
bool x264_play_sound(const unsigned short uiSoundIdx, const bool bAsync, const wchar_t *alias = NULL);
bool x264_portable(void);
unsigned int x264_process_id(void);
unsigned int x264_process_id(QProcess &process);
QString x264_query_reg_string(const bool bUser, const QString &path, const QString &name);
unsigned int x264_rand(void);
QString x264_rand_str(const bool bLong = false);
void x264_seed_rand(void);
bool x264_set_thread_execution_state(const bool systemRequired);
bool x264_shutdown_computer(const QString &message, const unsigned long timeout, const bool forceShutdown);
void x264_sleep(const unsigned int delay);
bool x264_suspendProcess(const QProcess *proc, const bool suspend);
const QString &x264_temp_directory(void);
bool x264_user_is_admin(void);
const char *x264_version_arch(void);
unsigned int x264_version_build(void);
const char *x264_version_compiler(void);
const QDate &x264_version_date(void);
unsigned int x264_version_major(void);
unsigned int x264_version_minor(void);
const char *x264_version_time(void);

///////////////////////////////////////////////////////////////////////////////
// HELPER MACROS
///////////////////////////////////////////////////////////////////////////////

#define QWCHAR(STR) reinterpret_cast<const wchar_t*>(STR.utf16())
#define QUTF8(STR) ((STR).toUtf8().constData())
#define WCHAR2QSTR(STR) (QString::fromUtf16(reinterpret_cast<const unsigned short*>((STR))))
#define X264_BOOL(X) ((X) ? "1" : "0")
#define X264_DELETE(PTR) if(PTR) { delete PTR; PTR = NULL; }
#define X264_DELETE_ARRAY(PTR) if(PTR) { delete [] PTR; PTR = NULL; }
#define _X264_MAKE_STRING_(X) #X
#define X264_MAKE_STRING(X) _X264_MAKE_STRING_(X)
#define X264_COMPILER_WARNING(TXT) __pragma(message(__FILE__ "(" X264_MAKE_STRING(__LINE__) ") : warning: " TXT))
#define X264_STRCMP(X,Y) ((X).compare((Y), Qt::CaseInsensitive) == 0)

//Debug build
#if defined(_DEBUG) && defined(QT_DEBUG) && !defined(NDEBUG) && !defined(QT_NO_DEBUG)
	#define X264_DEBUG (1)
#else
	#define X264_DEBUG (0)
#endif

//Check for CPU-compatibility options
#if !defined(_M_X64) && defined(_MSC_VER) && defined(_M_IX86_FP)
	#if (_M_IX86_FP != 0)
		#error We should not enabled SSE or SSE2 in release builds!
	#endif
#endif

//Helper macro for throwing exceptions
#define THROW(MESSAGE, ...) do \
{ \
	throw X264Exception((MESSAGE), __VA_ARGS__); \
} \
while(0)

//Memory check
#if X264_DEBUG
	#define X264_MEMORY_CHECK(FUNC, RETV,  ...) do \
	{ \
		size_t _privateBytesBefore = x264_dbg_private_bytes(); \
		RETV = FUNC(__VA_ARGS__); \
		size_t _privateBytesLeak = (x264_dbg_private_bytes() - _privateBytesBefore) / 1024; \
		if(_privateBytesLeak > 0) { \
			x264_dbg_output_string("\nMemory leak: Lost %u KiloBytes of PrivateUsage memory!\n\n", _privateBytesLeak); \
		} \
	} \
	while(0)
#else
	#define X264_MEMORY_CHECK(FUNC, RETV,  ...) do \
	{ \
		RETV = __noop(__VA_ARGS__); \
	} \
	while(0)
#endif
