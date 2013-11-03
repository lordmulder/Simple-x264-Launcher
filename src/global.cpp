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

#include "global.h"
#include "targetver.h"
#include "version.h"

//Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <MMSystem.h>
#include <ShellAPI.h>

//C++ includes
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

//VLD
#include <vld.h>

//Qt includes
#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QUuid>
#include <QMap>
#include <QDate>
#include <QIcon>
#include <QPlastiqueStyle>
#include <QImageReader>
#include <QSharedMemory>
#include <QSysInfo>
#include <QStringList>
#include <QSystemSemaphore>
#include <QDesktopServices>
#include <QMutex>
#include <QTextCodec>
#include <QLibrary>
#include <QRegExp>
#include <QResource>
#include <QTranslator>
#include <QEventLoop>
#include <QTimer>
#include <QLibraryInfo>
#include <QEvent>
#include <QReadLocker>
#include <QWriteLocker>
#include <QProcess>

//CRT includes
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>
#include <process.h>

//Debug only includes
#if X264_DEBUG
#include <Psapi.h>
#endif

//Global vars
static bool g_x264_console_attached = false;
static QMutex g_x264_message_mutex;
static const DWORD g_main_thread_id = GetCurrentThreadId();
static FILE *g_x264_log_file = NULL;
static QDate g_x264_version_date;

//Const
static const char *g_x264_months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *g_x264_imageformats[] = {"png", "jpg", "gif", "ico", "svg", NULL};

//Build version
static const struct
{
	unsigned int ver_major;
	unsigned int ver_minor;
	unsigned int ver_patch;
	unsigned int ver_build;
	const char* ver_date;
	const char* ver_time;
}
g_x264_version =
{
	(VER_X264_MAJOR),
	(VER_X264_MINOR),
	(VER_X264_PATCH),
	(VER_X264_BUILD),
	__DATE__,
	__TIME__
};

//CLI Arguments
static struct
{
	QStringList *list;
	QReadWriteLock lock;
}
g_x264_argv;

//OS Version
static struct
{
	bool bInitialized;
	x264_os_version_t version;
	QReadWriteLock lock;
}
g_x264_os_version;

//Portable Mode
static struct
{
	bool bInitialized;
	bool bPortableModeEnabled;
	QReadWriteLock lock;
}
g_x264_portable;

//Known Windows versions - maps marketing names to the actual Windows NT versions
const x264_os_version_t x264_winver_win2k = {5,0};
const x264_os_version_t x264_winver_winxp = {5,1};
const x264_os_version_t x264_winver_xpx64 = {5,2};
const x264_os_version_t x264_winver_vista = {6,0};
const x264_os_version_t x264_winver_win70 = {6,1};
const x264_os_version_t x264_winver_win80 = {6,2};
const x264_os_version_t x264_winver_win81 = {6,3};

//GURU MEDITATION
static const char *GURU_MEDITATION = "\n\nGURU MEDITATION !!!\n\n";

///////////////////////////////////////////////////////////////////////////////
// MACROS
///////////////////////////////////////////////////////////////////////////////

//String helper
#define CLEAN_OUTPUT_STRING(STR) do \
{ \
	const char CTRL_CHARS[3] = { '\r', '\n', '\t' }; \
	for(size_t i = 0; i < 3; i++) \
	{ \
		while(char *pos = strchr((STR), CTRL_CHARS[i])) *pos = char(0x20); \
	} \
} \
while(0)

//String helper
#define TRIM_LEFT(STR) do \
{ \
	const char WHITE_SPACE[4] = { char(0x20), '\r', '\n', '\t' }; \
	for(size_t i = 0; i < 4; i++) \
	{ \
		while(*(STR) == WHITE_SPACE[i]) (STR)++; \
	} \
} \
while(0)

#define X264_ZERO_MEMORY(X) SecureZeroMemory(&X, sizeof(X))

///////////////////////////////////////////////////////////////////////////////
// COMPILER INFO
///////////////////////////////////////////////////////////////////////////////

/*
 * Disclaimer: Parts of the following code were borrowed from MPC-HC project: http://mpc-hc.sf.net/
 */

//Compiler detection
#if defined(__INTEL_COMPILER)
	#if (__INTEL_COMPILER >= 1300)
		static const char *g_x264_version_compiler = "ICL 13." X264_MAKE_STR(__INTEL_COMPILER_BUILD_DATE);
	#elif (__INTEL_COMPILER >= 1200)
		static const char *g_x264_version_compiler = "ICL 12." X264_MAKE_STR(__INTEL_COMPILER_BUILD_DATE);
	#elif (__INTEL_COMPILER >= 1100)
		static const char *g_x264_version_compiler = "ICL 11.x";
	#elif (__INTEL_COMPILER >= 1000)
		static const char *g_x264_version_compiler = "ICL 10.x";
	#else
		#error Compiler is not supported!
	#endif
#elif defined(_MSC_VER)
	#if (_MSC_VER == 1800)
		#if (_MSC_FULL_VER < 180021005)
			static const char *g_x264_version_compiler = "MSVC 2013-Beta";
		#elif (_MSC_FULL_VER == 180021005)
			static const char *g_x264_version_compiler = "MSVC 2013";
		#else
			#error Compiler version is not supported yet!
		#endif
	#elif (_MSC_VER == 1700)
		#if (_MSC_FULL_VER < 170050727)
			static const char *g_x264_version_compiler = "MSVC 2012-Beta";
		#elif (_MSC_FULL_VER < 170051020)
			static const char *g_x264_version_compiler = "MSVC 2012";
		#elif (_MSC_FULL_VER < 170051106)
			static const char *g_x264_version_compiler = "MSVC 2012.1-CTP";
		#elif (_MSC_FULL_VER < 170060315)
			static const char *g_x264_version_compiler = "MSVC 2012.1";
		#elif (_MSC_FULL_VER < 170060610)
			static const char *g_x264_version_compiler = "MSVC 2012.2";
		#elif (_MSC_FULL_VER == 170060610)
			static const char *g_x264_version_compiler = "MSVC 2012.3";
		#else
			#error Compiler version is not supported yet!
		#endif
	#elif (_MSC_VER == 1600)
		#if (_MSC_FULL_VER < 160040219)
			static const char *g_x264_version_compiler = "MSVC 2010";
		#elif (_MSC_FULL_VER == 160040219)
			static const char *g_x264_version_compiler = "MSVC 2010-SP1";
		#else
			#error Compiler version is not supported yet!
		#endif
	#elif (_MSC_VER == 1500)
		#if (_MSC_FULL_VER >= 150030729)
			static const char *g_x264_version_compiler = "MSVC 2008-SP1";
		#else
			static const char *g_x264_version_compiler = "MSVC 2008";
		#endif
	#else
		#error Compiler is not supported!
	#endif

	// Note: /arch:SSE and /arch:SSE2 are only available for the x86 platform
	#if !defined(_M_X64) && defined(_M_IX86_FP)
		#if (_M_IX86_FP == 1)
			X264_COMPILER_WARNING("SSE instruction set is enabled!")
		#elif (_M_IX86_FP == 2)
			X264_COMPILER_WARNING("SSE2 (or higher) instruction set is enabled!")
		#endif
	#endif
#else
	#error Compiler is not supported!
#endif

//Architecture detection
#if defined(_M_X64)
	static const char *g_x264_version_arch = "x64";
#elif defined(_M_IX86)
	static const char *g_x264_version_arch = "x86";
#else
	#error Architecture is not supported!
#endif

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

static __forceinline bool x264_check_for_debugger(void);

/*
 * Suspend calling thread for N milliseconds
 */
inline void x264_sleep(const unsigned int delay)
{
	Sleep(delay);
}

/*
 * Global exception handler
 */
LONG WINAPI x264_exception_handler(__in struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	if(GetCurrentThreadId() != g_main_thread_id)
	{
		HANDLE mainThread = OpenThread(THREAD_TERMINATE, FALSE, g_main_thread_id);
		if(mainThread) TerminateThread(mainThread, ULONG_MAX);
	}

	x264_fatal_exit(L"Unhandeled exception handler invoked, application will exit!");
	return LONG_MAX;
}

/*
 * Invalid parameters handler
 */
void x264_invalid_param_handler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t)
{
	if(GetCurrentThreadId() != g_main_thread_id)
	{
		HANDLE mainThread = OpenThread(THREAD_TERMINATE, FALSE, g_main_thread_id);
		if(mainThread) TerminateThread(mainThread, ULONG_MAX);
	}

	x264_fatal_exit(L"Invalid parameter handler invoked, application will exit!");
}

/*
 * Change console text color
 */
static void x264_console_color(FILE* file, WORD attributes)
{
	const HANDLE hConsole = (HANDLE)(_get_osfhandle(_fileno(file)));
	if((hConsole != NULL) && (hConsole != INVALID_HANDLE_VALUE))
	{
		SetConsoleTextAttribute(hConsole, attributes);
	}
}

/*
 * Output logging message to console
 */
static void x264_write_console(const int type, const char *msg)
{	
	__try
	{
		if(_isatty(_fileno(stderr)))
		{
			UINT oldOutputCP = GetConsoleOutputCP();
			if(oldOutputCP != CP_UTF8) SetConsoleOutputCP(CP_UTF8);

			switch(type)
			{
			case QtCriticalMsg:
			case QtFatalMsg:
				x264_console_color(stderr, FOREGROUND_RED | FOREGROUND_INTENSITY);
				fprintf(stderr, GURU_MEDITATION);
				fprintf(stderr, "%s\n", msg);
				fflush(stderr);
				break;
			case QtWarningMsg:
				x264_console_color(stderr, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
				fprintf(stderr, "%s\n", msg);
				fflush(stderr);
				break;
			default:
				x264_console_color(stderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
				fprintf(stderr, "%s\n", msg);
				fflush(stderr);
				break;
			}
	
			x264_console_color(stderr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
			if(oldOutputCP != CP_UTF8) SetConsoleOutputCP(oldOutputCP);
		}
	}
	__except(1)
	{
		/*ignore any exception that might occur here!*/
	}
}

/*
 * Output logging message to debugger
 */
static void x264_write_dbg_out(const int type, const char *msg)
{	
	const char *FORMAT = "[sx264l][%c] %s\n";

	__try
	{
		char buffer[512];
		const char* input = msg;
		TRIM_LEFT(input);
		
		switch(type)
		{
		case QtCriticalMsg:
		case QtFatalMsg:
			_snprintf_s(buffer, 512, _TRUNCATE, FORMAT, 'C', input);
			break;
		case QtWarningMsg:
			_snprintf_s(buffer, 512, _TRUNCATE, FORMAT, 'W', input);
			break;
		default:
			_snprintf_s(buffer, 512, _TRUNCATE, FORMAT, 'I', input);
			break;
		}

		char *temp = &buffer[0];
		CLEAN_OUTPUT_STRING(temp);
		OutputDebugStringA(temp);
	}
	__except(1)
	{
		/*ignore any exception that might occur here!*/
	}
}

/*
 * Output logging message to logfile
 */
static void x264_write_logfile(const int type, const char *msg)
{	
	const char *FORMAT = "[%c][%04u] %s\r\n";

	__try
	{
		if(g_x264_log_file)
		{
			char buffer[512];
			strncpy_s(buffer, 512, msg, _TRUNCATE);

			char *temp = &buffer[0];
			TRIM_LEFT(temp);
			CLEAN_OUTPUT_STRING(temp);
			
			const unsigned int timestamp = static_cast<unsigned int>(_time64(NULL) % 3600I64);

			switch(type)
			{
			case QtCriticalMsg:
			case QtFatalMsg:
				fprintf(g_x264_log_file, FORMAT, 'C', timestamp, temp);
				break;
			case QtWarningMsg:
				fprintf(g_x264_log_file, FORMAT, 'W', timestamp, temp);
				break;
			default:
				fprintf(g_x264_log_file, FORMAT, 'I', timestamp, temp);
				break;
			}

			fflush(g_x264_log_file);
		}
	}
	__except(1)
	{
		/*ignore any exception that might occur here!*/
	}
}

/*
 * Qt message handler
 */
void x264_message_handler(QtMsgType type, const char *msg)
{
	if((!msg) || (!(msg[0])))
	{
		return;
	}

	QMutexLocker lock(&g_x264_message_mutex);

	if(g_x264_log_file)
	{
		x264_write_logfile(type, msg);
	}

	if(g_x264_console_attached)
	{
		x264_write_console(type, msg);
	}
	else
	{
		x264_write_dbg_out(type, msg);
	}

	if((type == QtCriticalMsg) || (type == QtFatalMsg))
	{
		lock.unlock();
		x264_fatal_exit(L"The application has encountered a critical error and will exit now!", QWCHAR(QString::fromUtf8(msg)));
	}
}

/*
 * Initialize the console
 */
void x264_init_console(const QStringList &argv)
{
	bool enableConsole = x264_is_prerelease() || (X264_DEBUG);

	if(_environ)
	{
		wchar_t *logfile = NULL;
		size_t logfile_len = 0;
		if(!_wdupenv_s(&logfile, &logfile_len, L"X264_LOGFILE"))
		{
			if(logfile && (logfile_len > 0))
			{
				FILE *temp = NULL;
				if(!_wfopen_s(&temp, logfile, L"wb"))
				{
					fprintf(temp, "%c%c%c", char(0xEF), char(0xBB), char(0xBF));
					g_x264_log_file = temp;
				}
				free(logfile);
			}
		}
	}

	if(!X264_DEBUG)
	{
		for(int i = 0; i < argv.count(); i++)
		{
			if(!argv.at(i).compare("--console", Qt::CaseInsensitive))
			{
				enableConsole = true;
			}
			else if(!argv.at(i).compare("--no-console", Qt::CaseInsensitive))
			{
				enableConsole = false;
			}
		}
	}

	if(enableConsole)
	{
		if(!g_x264_console_attached)
		{
			if(AllocConsole() != FALSE)
			{
				SetConsoleCtrlHandler(NULL, TRUE);
				SetConsoleTitle(L"Simple x264 Launcher | Debug Console");
				SetConsoleOutputCP(CP_UTF8);
				g_x264_console_attached = true;
			}
		}
		
		if(g_x264_console_attached)
		{
			//-------------------------------------------------------------------
			//See: http://support.microsoft.com/default.aspx?scid=kb;en-us;105305
			//-------------------------------------------------------------------
			const int flags = _O_WRONLY | _O_U8TEXT;
			int hCrtStdOut = _open_osfhandle((intptr_t) GetStdHandle(STD_OUTPUT_HANDLE), flags);
			int hCrtStdErr = _open_osfhandle((intptr_t) GetStdHandle(STD_ERROR_HANDLE),  flags);
			FILE *hfStdOut = (hCrtStdOut >= 0) ? _fdopen(hCrtStdOut, "wb") : NULL;
			FILE *hfStdErr = (hCrtStdErr >= 0) ? _fdopen(hCrtStdErr, "wb") : NULL;
			if(hfStdOut) { *stdout = *hfStdOut; std::cout.rdbuf(new std::filebuf(hfStdOut)); }
			if(hfStdErr) { *stderr = *hfStdErr; std::cerr.rdbuf(new std::filebuf(hfStdErr)); }
		}

		HWND hwndConsole = GetConsoleWindow();

		if((hwndConsole != NULL) && (hwndConsole != INVALID_HANDLE_VALUE))
		{
			HMENU hMenu = GetSystemMenu(hwndConsole, 0);
			EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
			RemoveMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);

			SetWindowPos(hwndConsole, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
			SetWindowLong(hwndConsole, GWL_STYLE, GetWindowLong(hwndConsole, GWL_STYLE) & (~WS_MAXIMIZEBOX) & (~WS_MINIMIZEBOX));
			SetWindowPos(hwndConsole, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
		}
	}
}

/*
 * Initialize the console
 */
void x264_init_console(int argc, char* argv[])
{
	bool enableConsole = x264_is_prerelease() || (X264_DEBUG);

	if(_environ)
	{
		wchar_t *logfile = NULL;
		size_t logfile_len = 0;
		if(!_wdupenv_s(&logfile, &logfile_len, L"X264_LAUNCHER_LOGFILE"))
		{
			if(logfile && (logfile_len > 0))
			{
				FILE *temp = NULL;
				if(!_wfopen_s(&temp, logfile, L"wb"))
				{
					fprintf(temp, "%c%c%c", 0xEF, 0xBB, 0xBF);
					g_x264_log_file = temp;
				}
				free(logfile);
			}
		}
	}

	if(!X264_DEBUG)
	{
		for(int i = 0; i < argc; i++)
		{
			if(!_stricmp(argv[i], "--console"))
			{
				enableConsole = true;
			}
			else if(!_stricmp(argv[i], "--no-console"))
			{
				enableConsole = false;
			}
		}
	}

	if(enableConsole)
	{
		if(!g_x264_console_attached)
		{
			if(AllocConsole())
			{
				SetConsoleCtrlHandler(NULL, TRUE);
				SetConsoleTitle(L"Simple x264 Launcher | Debug Console");
				SetConsoleOutputCP(CP_UTF8);
				g_x264_console_attached = true;
			}
		}
		
		if(g_x264_console_attached)
		{
			//-------------------------------------------------------------------
			//See: http://support.microsoft.com/default.aspx?scid=kb;en-us;105305
			//-------------------------------------------------------------------
			const int flags = _O_WRONLY | _O_U8TEXT;
			int hCrtStdOut = _open_osfhandle((intptr_t) GetStdHandle(STD_OUTPUT_HANDLE), flags);
			int hCrtStdErr = _open_osfhandle((intptr_t) GetStdHandle(STD_ERROR_HANDLE), flags);
			FILE *hfStdOut = (hCrtStdOut >= 0) ? _fdopen(hCrtStdOut, "wb") : NULL;
			FILE *hfStdErr = (hCrtStdErr >= 0) ? _fdopen(hCrtStdErr, "wb") : NULL;
			if(hfStdOut) { *stdout = *hfStdOut; std::cout.rdbuf(new std::filebuf(hfStdOut)); }
			if(hfStdErr) { *stderr = *hfStdErr; std::cerr.rdbuf(new std::filebuf(hfStdErr)); }
		}

		HWND hwndConsole = GetConsoleWindow();

		if((hwndConsole != NULL) && (hwndConsole != INVALID_HANDLE_VALUE))
		{
			HMENU hMenu = GetSystemMenu(hwndConsole, 0);
			EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
			RemoveMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);

			SetWindowPos(hwndConsole, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
			SetWindowLong(hwndConsole, GWL_STYLE, GetWindowLong(hwndConsole, GWL_STYLE) & (~WS_MAXIMIZEBOX) & (~WS_MINIMIZEBOX));
			SetWindowPos(hwndConsole, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
		}
	}
}

/*
 * Version info
 */
unsigned int x264_version_major(void)
{
	return g_x264_version.ver_major;
}

unsigned int x264_version_minor(void)
{
	return (g_x264_version.ver_minor * 10) + (g_x264_version.ver_patch % 10);
}

unsigned int x264_version_build(void)
{
	return g_x264_version.ver_build;
}

const char *x264_version_compiler(void)
{
	return g_x264_version_compiler;
}

const char *x264_version_arch(void)
{
	return g_x264_version_arch;
}

/*
 * Get CLI arguments
 */
const QStringList &x264_arguments(void)
{
	QReadLocker readLock(&g_x264_argv.lock);

	if(!g_x264_argv.list)
	{
		readLock.unlock();
		QWriteLocker writeLock(&g_x264_argv.lock);

		g_x264_argv.list = new QStringList;

		int nArgs = 0;
		LPWSTR *szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);

		if(NULL != szArglist)
		{
			for(int i = 0; i < nArgs; i++)
			{
				(*g_x264_argv.list) << WCHAR2QSTR(szArglist[i]);
			}
			LocalFree(szArglist);
		}
		else
		{
			qWarning("CommandLineToArgvW() has failed !!!");
		}
	}

	return (*g_x264_argv.list);
}

/*
 * Check for portable mode
 */
bool x264_portable(void)
{
	QReadLocker readLock(&g_x264_portable.lock);

	if(g_x264_portable.bInitialized)
	{
		return g_x264_portable.bPortableModeEnabled;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_x264_portable.lock);

	if(!g_x264_portable.bInitialized)
	{
		if(VER_X264_PORTABLE_EDITION)
		{
			qWarning("Simple x264 Launcher portable edition!\n");
			g_x264_portable.bPortableModeEnabled = true;
		}
		else
		{
			QString baseName = QFileInfo(QApplication::applicationFilePath()).completeBaseName();
			int idx1 = baseName.indexOf("x264", 0, Qt::CaseInsensitive);
			int idx2 = baseName.lastIndexOf("portable", -1, Qt::CaseInsensitive);
			g_x264_portable.bPortableModeEnabled = (idx1 >= 0) && (idx2 >= 0) && (idx1 < idx2);
		}
		g_x264_portable.bInitialized = true;
	}
	
	return g_x264_portable.bPortableModeEnabled;
}

/*
 * Get data path (i.e. path to store config files)
 */
const QString &x264_data_path(void)
{
	static QString pathCache;
	
	if(pathCache.isNull())
	{
		if(!x264_portable())
		{
			pathCache = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
		}
		if(pathCache.isEmpty() || x264_portable())
		{
			pathCache = QApplication::applicationDirPath();
		}
		if(!QDir(pathCache).mkpath("."))
		{
			qWarning("Data directory could not be created:\n%s\n", pathCache.toUtf8().constData());
			pathCache = QDir::currentPath();
		}
	}
	
	return pathCache;
}

/*
 * Get build date date
 */
const QDate &x264_version_date(void)
{
	if(!g_x264_version_date.isValid())
	{
		int date[3] = {0, 0, 0}; char temp[12] = {'\0'};
		strncpy_s(temp, 12, g_x264_version.ver_date, _TRUNCATE);

		if(strlen(temp) == 11)
		{
			temp[3] = temp[6] = '\0';
			date[2] = atoi(&temp[4]);
			date[0] = atoi(&temp[7]);
			
			for(int j = 0; j < 12; j++)
			{
				if(!_strcmpi(&temp[0], g_x264_months[j]))
				{
					date[1] = j+1;
					break;
				}
			}

			g_x264_version_date = QDate(date[0], date[1], date[2]);
		}

		if(!g_x264_version_date.isValid())
		{
			qFatal("Internal error: Date format could not be recognized!");
		}
	}

	return g_x264_version_date;
}

const char *x264_version_time(void)
{
	return g_x264_version.ver_time;
}

bool x264_is_prerelease(void)
{
	return (VER_X264_PRE_RELEASE);
}

/*
 * CPUID prototype (actual function is in ASM code)
 */
extern "C"
{
	void x264_cpu_cpuid(unsigned int op, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx);
}

/*
 * Detect CPU features
 */
x264_cpu_t x264_detect_cpu_features(const QStringList &argv)
{
	typedef BOOL (WINAPI *IsWow64ProcessFun)(__in HANDLE hProcess, __out PBOOL Wow64Process);

	x264_cpu_t features;
	SYSTEM_INFO systemInfo;
	unsigned int CPUInfo[4];
	char CPUIdentificationString[0x40];
	char CPUBrandString[0x40];

	memset(&features, 0, sizeof(x264_cpu_t));
	memset(&systemInfo, 0, sizeof(SYSTEM_INFO));
	memset(CPUIdentificationString, 0, sizeof(CPUIdentificationString));
	memset(CPUBrandString, 0, sizeof(CPUBrandString));
	
	x264_cpu_cpuid(0, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
	memcpy(CPUIdentificationString, &CPUInfo[1], sizeof(int));
	memcpy(CPUIdentificationString + 4, &CPUInfo[3], sizeof(int));
	memcpy(CPUIdentificationString + 8, &CPUInfo[2], sizeof(int));
	features.intel = (_stricmp(CPUIdentificationString, "GenuineIntel") == 0);
	strncpy_s(features.vendor, 0x40, CPUIdentificationString, _TRUNCATE);

	if(CPUInfo[0] >= 1)
	{
		x264_cpu_cpuid(1, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
		features.mmx = (CPUInfo[3] & 0x800000) || false;
		features.sse = (CPUInfo[3] & 0x2000000) || false;
		features.sse2 = (CPUInfo[3] & 0x4000000) || false;
		features.ssse3 = (CPUInfo[2] & 0x200) || false;
		features.sse3 = (CPUInfo[2] & 0x1) || false;
		features.ssse3 = (CPUInfo[2] & 0x200) || false;
		features.stepping = CPUInfo[0] & 0xf;
		features.model = ((CPUInfo[0] >> 4) & 0xf) + (((CPUInfo[0] >> 16) & 0xf) << 4);
		features.family = ((CPUInfo[0] >> 8) & 0xf) + ((CPUInfo[0] >> 20) & 0xff);
		if(features.sse) features.mmx2 = true; //MMXEXT is a subset of SSE!
	}

	x264_cpu_cpuid(0x80000000, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
	int nExIds = qMax<int>(qMin<int>(CPUInfo[0], 0x80000004), 0x80000000);

	if((_stricmp(CPUIdentificationString, "AuthenticAMD") == 0) && (nExIds >= 0x80000001U))
	{
		x264_cpu_cpuid(0x80000001, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
		features.mmx2 = features.mmx2 || (CPUInfo[3] & 0x00400000U);
	}

	for(int i = 0x80000002; i <= nExIds; ++i)
	{
		x264_cpu_cpuid(i, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
		switch(i)
		{
		case 0x80000002:
			memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
			break;
		case 0x80000003:
			memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
			break;
		case 0x80000004:
			memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
			break;
		}
	}

	strncpy_s(features.brand, 0x40, CPUBrandString, _TRUNCATE);

	if(strlen(features.brand) < 1) strncpy_s(features.brand, 0x40, "Unknown", _TRUNCATE);
	if(strlen(features.vendor) < 1) strncpy_s(features.vendor, 0x40, "Unknown", _TRUNCATE);

#if (!(defined(_M_X64) || defined(_M_IA64)))
	QLibrary Kernel32Lib("kernel32.dll");
	if(IsWow64ProcessFun IsWow64ProcessPtr = (IsWow64ProcessFun) Kernel32Lib.resolve("IsWow64Process"))
	{
		BOOL x64flag = FALSE;
		if(IsWow64ProcessPtr(GetCurrentProcess(), &x64flag))
		{
			features.x64 = (x64flag == TRUE);
		}
	}
#else
	features.x64 = true;
#endif

	DWORD_PTR procAffinity, sysAffinity;
	if(GetProcessAffinityMask(GetCurrentProcess(), &procAffinity, &sysAffinity))
	{
		for(DWORD_PTR mask = 1; mask; mask <<= 1)
		{
			features.count += ((sysAffinity & mask) ? (1) : (0));
		}
	}
	if(features.count < 1)
	{
		GetNativeSystemInfo(&systemInfo);
		features.count = qBound(1UL, systemInfo.dwNumberOfProcessors, 64UL);
	}

	if(argv.count() > 0)
	{
		bool flag = false;
		for(int i = 0; i < argv.count(); i++)
		{
			if(!argv[i].compare("--force-cpu-no-64bit", Qt::CaseInsensitive)) { flag = true; features.x64 = false; }
			if(!argv[i].compare("--force-cpu-no-sse", Qt::CaseInsensitive)) { flag = true; features.sse = features.sse2 = features.sse3 = features.ssse3 = false; }
			if(!argv[i].compare("--force-cpu-no-intel", Qt::CaseInsensitive)) { flag = true; features.intel = false; }
		}
		if(flag) qWarning("CPU flags overwritten by user-defined parameters. Take care!\n");
	}

	return features;
}

/*
 * Verify a specific Windows version
 */
static bool x264_verify_os_version(const DWORD major, const DWORD minor)
{
	OSVERSIONINFOEXW osvi;
	DWORDLONG dwlConditionMask = 0;

	//Initialize the OSVERSIONINFOEX structure
	memset(&osvi, 0, sizeof(OSVERSIONINFOEXW));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
	osvi.dwMajorVersion = major;
	osvi.dwMinorVersion = minor;
	osvi.dwPlatformId = VER_PLATFORM_WIN32_NT;

	//Initialize the condition mask
	VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
	VER_SET_CONDITION(dwlConditionMask, VER_PLATFORMID, VER_EQUAL);

	// Perform the test
	const BOOL ret = VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_PLATFORMID, dwlConditionMask);

	//Error checking
	if(!ret)
	{
		if(GetLastError() != ERROR_OLD_WIN_VERSION)
		{
			qWarning("VerifyVersionInfo() system call has failed!");
		}
	}

	return (ret != FALSE);
}

/*
 * Determine the *real* Windows version
 */
static bool x264_get_real_os_version(unsigned int *major, unsigned int *minor, bool *pbOverride)
{
	*major = *minor = 0;
	*pbOverride = false;
	
	//Initialize local variables
	OSVERSIONINFOEXW osvi;
	memset(&osvi, 0, sizeof(OSVERSIONINFOEXW));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);

	//Try GetVersionEx() first
	if(GetVersionExW((LPOSVERSIONINFOW)&osvi) == FALSE)
	{
		qWarning("GetVersionEx() has failed, cannot detect Windows version!");
		return false;
	}

	//Make sure we are running on NT
	if(osvi.dwPlatformId == VER_PLATFORM_WIN32_NT)
	{
		*major = osvi.dwMajorVersion;
		*minor = osvi.dwMinorVersion;
	}
	else
	{
		qWarning("Not running on Windows NT, unsupported operating system!");
		return false;
	}

	//Determine the real *major* version first
	forever
	{
		const DWORD nextMajor = (*major) + 1;
		if(x264_verify_os_version(nextMajor, 0))
		{
			*pbOverride = true;
			*major = nextMajor;
			*minor = 0;
			continue;
		}
		break;
	}

	//Now also determine the real *minor* version
	forever
	{
		const DWORD nextMinor = (*minor) + 1;
		if(x264_verify_os_version((*major), nextMinor))
		{
			*pbOverride = true;
			*minor = nextMinor;
			continue;
		}
		break;
	}

	return true;
}

/*
 * Get the native operating system version
 */
const x264_os_version_t &x264_get_os_version(void)
{
	QReadLocker readLock(&g_x264_os_version.lock);

	//Already initialized?
	if(g_x264_os_version.bInitialized)
	{
		return g_x264_os_version.version;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_x264_os_version.lock);

	//Detect OS version
	if(!g_x264_os_version.bInitialized)
	{
		unsigned int major, minor; bool oflag;
		if(x264_get_real_os_version(&major, &minor, &oflag))
		{
			g_x264_os_version.version.versionMajor = major;
			g_x264_os_version.version.versionMinor = minor;
			g_x264_os_version.version.overrideFlag = oflag;
			g_x264_os_version.bInitialized = true;
		}
		else
		{
			qWarning("Failed to determin the operating system version!");
		}
	}

	return g_x264_os_version.version;
}

/*
 * Check for compatibility mode
 */
static bool x264_check_compatibility_mode(const char *exportName, const char *executableName)
{
	QLibrary kernel32("kernel32.dll");

	if(exportName != NULL)
	{
		if(kernel32.resolve(exportName) != NULL)
		{
			qWarning("Function '%s' exported from 'kernel32.dll' -> Windows compatibility mode!", exportName);
			qFatal("%s", QApplication::tr("Executable '%1' doesn't support Windows compatibility mode.").arg(QString::fromLatin1(executableName)).toLatin1().constData());
			return false;
		}
	}

	return true;
}

/*
 * Check if we are running under wine
 */
bool x264_detect_wine(void)
{
	static bool isWine = false;
	static bool isWine_initialized = false;

	if(!isWine_initialized)
	{
		QLibrary ntdll("ntdll.dll");
		if(ntdll.load())
		{
			if(ntdll.resolve("wine_nt_to_unix_file_name") != NULL) isWine = true;
			if(ntdll.resolve("wine_get_version") != NULL) isWine = true;
			ntdll.unload();
		}
		isWine_initialized = true;
	}

	return isWine;
}

/*
 * Qt event filter
 */
static bool x264_event_filter(void *message, long *result)
{ 
	if((!(X264_DEBUG)) && x264_check_for_debugger())
	{
		x264_fatal_exit(L"Not a debug build. Please unload debugger and try again!");
	}
	
	//switch(reinterpret_cast<MSG*>(message)->message)
	//{
	//case WM_QUERYENDSESSION:
	//	qWarning("WM_QUERYENDSESSION message received!");
	//	*result = x264_broadcast(x264_event_queryendsession, false) ? TRUE : FALSE;
	//	return true;
	//case WM_ENDSESSION:
	//	qWarning("WM_ENDSESSION message received!");
	//	if(reinterpret_cast<MSG*>(message)->wParam == TRUE)
	//	{
	//		x264_broadcast(x264_event_endsession, false);
	//		if(QApplication *app = reinterpret_cast<QApplication*>(QApplication::instance()))
	//		{
	//			app->closeAllWindows();
	//			app->quit();
	//		}
	//		x264_finalization();
	//		exit(1);
	//	}
	//	*result = 0;
	//	return true;
	//default:
	//	/*ignore this message and let Qt handle it*/
	//	return false;
	//}

	return false;
}

/*
 * Check for process elevation
 */
static bool x264_check_elevation(void)
{
	typedef enum { x264_token_elevationType_class = 18, x264_token_elevation_class = 20 } X264_TOKEN_INFORMATION_CLASS;
	typedef enum { x264_elevationType_default = 1, x264_elevationType_full, x264_elevationType_limited } X264_TOKEN_ELEVATION_TYPE;

	HANDLE hToken = NULL;
	bool bIsProcessElevated = false;
	
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		X264_TOKEN_ELEVATION_TYPE tokenElevationType;
		DWORD returnLength;
		if(GetTokenInformation(hToken, (TOKEN_INFORMATION_CLASS) x264_token_elevationType_class, &tokenElevationType, sizeof(X264_TOKEN_ELEVATION_TYPE), &returnLength))
		{
			if(returnLength == sizeof(X264_TOKEN_ELEVATION_TYPE))
			{
				switch(tokenElevationType)
				{
				case x264_elevationType_default:
					qDebug("Process token elevation type: Default -> UAC is disabled.\n");
					break;
				case x264_elevationType_full:
					qWarning("Process token elevation type: Full -> potential security risk!\n");
					bIsProcessElevated = true;
					break;
				case x264_elevationType_limited:
					qDebug("Process token elevation type: Limited -> not elevated.\n");
					break;
				}
			}
		}
		CloseHandle(hToken);
	}
	else
	{
		qWarning("Failed to open process token!");
	}

	return !bIsProcessElevated;
}

/*
 * Initialize Qt framework
 */
bool x264_init_qt(int argc, char* argv[])
{
	static bool qt_initialized = false;
	typedef BOOL (WINAPI *SetDllDirectoryProc)(WCHAR *lpPathName);
	const QStringList &arguments = x264_arguments();

	//Don't initialized again, if done already
	if(qt_initialized)
	{
		return true;
	}

	//Secure DLL loading
	QLibrary kernel32("kernel32.dll");
	if(kernel32.load())
	{
		SetDllDirectoryProc pSetDllDirectory = (SetDllDirectoryProc) kernel32.resolve("SetDllDirectoryW");
		if(pSetDllDirectory != NULL) pSetDllDirectory(L"");
	}

	//Extract executable name from argv[] array
	QString executableName = QLatin1String("x264_launcher.exe");
	if(arguments.count() > 0)
	{
		static const char *delimiters = "\\/:?";
		executableName = arguments[0].trimmed();
		for(int i = 0; delimiters[i]; i++)
		{
			int temp = executableName.lastIndexOf(QChar(delimiters[i]));
			if(temp >= 0) executableName = executableName.mid(temp + 1);
		}
		executableName = executableName.trimmed();
		if(executableName.isEmpty())
		{
			executableName = QLatin1String("x264_launcher.exe");
		}
	}

	//Check Qt version
#ifdef QT_BUILD_KEY
	qDebug("Using Qt v%s [%s], %s, %s", qVersion(), QLibraryInfo::buildDate().toString(Qt::ISODate).toLatin1().constData(), (qSharedBuild() ? "DLL" : "Static"), QLibraryInfo::buildKey().toLatin1().constData());
	qDebug("Compiled with Qt v%s [%s], %s\n", QT_VERSION_STR, QT_PACKAGEDATE_STR, QT_BUILD_KEY);
	if(_stricmp(qVersion(), QT_VERSION_STR))
	{
		qFatal("%s", QApplication::tr("Executable '%1' requires Qt v%2, but found Qt v%3.").arg(executableName, QString::fromLatin1(QT_VERSION_STR), QString::fromLatin1(qVersion())).toLatin1().constData());
		return false;
	}
	if(QLibraryInfo::buildKey().compare(QString::fromLatin1(QT_BUILD_KEY), Qt::CaseInsensitive))
	{
		qFatal("%s", QApplication::tr("Executable '%1' was built for Qt '%2', but found Qt '%3'.").arg(executableName, QString::fromLatin1(QT_BUILD_KEY), QLibraryInfo::buildKey()).toLatin1().constData());
		return false;
	}
#else
	qDebug("Using Qt v%s [%s], %s", qVersion(), QLibraryInfo::buildDate().toString(Qt::ISODate).toLatin1().constData(), (qSharedBuild() ? "DLL" : "Static"));
	qDebug("Compiled with Qt v%s [%s]\n", QT_VERSION_STR, QT_PACKAGEDATE_STR);
#endif

	//Check the Windows version
	const x264_os_version_t &osVersionNo = x264_get_os_version();
	if(osVersionNo < x264_winver_winxp)
	{
		qFatal("%s", QApplication::tr("Executable '%1' requires Windows XP or later.").arg(executableName).toLatin1().constData());
	}

	//Supported Windows version?
	if(osVersionNo == x264_winver_winxp)
	{
		qDebug("Running on Windows XP or Windows XP Media Center Edition.\n");						//x264_check_compatibility_mode("GetLargePageMinimum", executableName);
	}
	else if(osVersionNo == x264_winver_xpx64)
	{
		qDebug("Running on Windows Server 2003, Windows Server 2003 R2 or Windows XP x64.\n");		//x264_check_compatibility_mode("GetLocaleInfoEx", executableName);
	}
	else if(osVersionNo == x264_winver_vista)
	{
		qDebug("Running on Windows Vista or Windows Server 2008.\n");								//x264_check_compatibility_mode("CreateRemoteThreadEx", executableName*/);
	}
	else if(osVersionNo == x264_winver_win70)
	{
		qDebug("Running on Windows 7 or Windows Server 2008 R2.\n");								//x264_check_compatibility_mode("CreateFile2", executableName);
	}
	else if(osVersionNo == x264_winver_win80)
	{
		qDebug("Running on Windows 8 or Windows Server 2012.\n");									//x264_check_compatibility_mode("FindPackagesByPackageFamily", executableName);
	}
	else if(osVersionNo == x264_winver_win81)
	{
		qDebug("Running on Windows 8.1 or Windows Server 2012 R2.\n");								//x264_check_compatibility_mode(NULL, executableName);
	}
	else
	{
		const QString message = QString().sprintf("Running on an unknown WindowsNT-based system (v%u.%u).", osVersionNo.versionMajor, osVersionNo.versionMinor);
		qWarning("%s\n", QUTF8(message));
		MessageBoxW(NULL, QWCHAR(message), L"Simple x264 Launcher", MB_OK | MB_TOPMOST | MB_ICONWARNING);
	}

	//Check for compat mode
	if(osVersionNo.overrideFlag && (osVersionNo <= x264_winver_win81))
	{
		qWarning("Windows compatibility mode detected!");
		if(!arguments.contains("--ignore-compat-mode", Qt::CaseInsensitive))
		{
			qFatal("%s", QApplication::tr("Executable '%1' doesn't support Windows compatibility mode.").arg(executableName).toLatin1().constData());
			return false;
		}
	}

	//Check for Wine
	if(x264_detect_wine())
	{
		qWarning("It appears we are running under Wine, unexpected things might happen!\n");
	}

	//Set text Codec for locale
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

	//Create Qt application instance
	QApplication *application = new QApplication(argc, argv);

	//Load plugins from application directory
	QCoreApplication::setLibraryPaths(QStringList() << QApplication::applicationDirPath());
	qDebug("Library Path:\n%s\n", QUTF8(QApplication::libraryPaths().first()));

	//Create Qt application instance and setup version info
	application->setApplicationName("Simple x264 Launcher");
	application->setApplicationVersion(QString().sprintf("%d.%02d", x264_version_major(), x264_version_minor())); 
	application->setOrganizationName("LoRd_MuldeR");
	application->setOrganizationDomain("mulder.at.gg");
	application->setWindowIcon(QIcon(":/icons/movie.ico"));
	application->setEventFilter(x264_event_filter);

	//Check for supported image formats
	QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
	for(int i = 0; g_x264_imageformats[i]; i++)
	{
		if(!supportedFormats.contains(g_x264_imageformats[i]))
		{
			qFatal("Qt initialization error: QImageIOHandler for '%s' missing!", g_x264_imageformats[i]);
			return false;
		}
	}
	
	//Add default translations
	/*
	QWriteLocker writeLockTranslations(&g_x264_translation.lock);
	if(!g_x264_translation.files) g_x264_translation.files = new QMap<QString, QString>();
	if(!g_x264_translation.names) g_x264_translation.names = new QMap<QString, QString>();
	g_x264_translation.files->insert(X264_DEFAULT_LANGID, "");
	g_x264_translation.names->insert(X264_DEFAULT_LANGID, "English");
	writeLockTranslations.unlock();
	*/

	//Check for process elevation
	if((!x264_check_elevation()) && (!x264_detect_wine()))
	{
		QMessageBox messageBox(QMessageBox::Warning, "Simple x264 Launcher", "<nobr>Simple x264 Launcher was started with 'elevated' rights, altough it does not need these rights.<br>Running an applications with unnecessary rights is a potential security risk!</nobr>", QMessageBox::NoButton, NULL, Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint);
		messageBox.addButton("Quit Program (Recommended)", QMessageBox::NoRole);
		messageBox.addButton("Ignore", QMessageBox::NoRole);
		if(messageBox.exec() == 0)
		{
			return false;
		}
	}

	//Update console icon, if a console is attached
#if QT_VERSION < QT_VERSION_CHECK(5,0,0)
	if(g_x264_console_attached && (!x264_detect_wine()))
	{
		typedef DWORD (__stdcall *SetConsoleIconFun)(HICON);
		QLibrary kernel32("kernel32.dll");
		if(kernel32.load())
		{
			SetConsoleIconFun SetConsoleIconPtr = (SetConsoleIconFun) kernel32.resolve("SetConsoleIcon");
			QPixmap pixmap = QIcon(":/icons/movie.ico").pixmap(16, 16);
			if((SetConsoleIconPtr != NULL) && (!pixmap.isNull())) SetConsoleIconPtr(pixmap.toWinHICON());
			kernel32.unload();
		}
	}
#endif

	//Done
	qt_initialized = true;
	return true;
}

/*
 * Suspend or resume process
 */
bool x264_suspendProcess(const QProcess *proc, const bool suspend)
{
	if(Q_PID pid = proc->pid())
	{
		if(suspend)
		{
			return (SuspendThread(pid->hThread) != ((DWORD) -1));
		}
		else
		{
			return (ResumeThread(pid->hThread) != ((DWORD) -1));
		}
	}
	else
	{
		return false;
	}
}

/*
 * Convert path to short/ANSI path
 */
QString x264_path2ansi(const QString &longPath)
{
	QString shortPath = longPath;
	
	DWORD buffSize = GetShortPathNameW(reinterpret_cast<const wchar_t*>(longPath.utf16()), NULL, NULL);
	
	if(buffSize > 0)
	{
		wchar_t *buffer = new wchar_t[buffSize];
		DWORD result = GetShortPathNameW(reinterpret_cast<const wchar_t*>(longPath.utf16()), buffer, buffSize);

		if((result > 0) && (result < buffSize))
		{
			shortPath = QString::fromUtf16(reinterpret_cast<const unsigned short*>(buffer), result);
		}

		delete[] buffer;
		buffer = NULL;
	}

	return shortPath;
}

/*
 * Set the process priority class for current process
 */
bool x264_change_process_priority(const int priority)
{
	return x264_change_process_priority(GetCurrentProcess(), priority);
}

/*
 * Set the process priority class for specified process
 */
bool x264_change_process_priority(const QProcess *proc, const int priority)
{
	if(Q_PID qPid = proc->pid())
	{
		return x264_change_process_priority(qPid->hProcess, priority);
	}
	else
	{
		return false;
	}
}

/*
 * Set the process priority class for specified process
 */
bool x264_change_process_priority(void *hProcess, const int priority)
{
	bool ok = false;

	switch(qBound(-2, priority, 2))
	{
	case 2:
		ok = (SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS) == TRUE);
		break;
	case 1:
		if(!(ok = (SetPriorityClass(hProcess, ABOVE_NORMAL_PRIORITY_CLASS) == TRUE)))
		{
			ok = (SetPriorityClass(hProcess, HIGH_PRIORITY_CLASS) == TRUE);
		}
		break;
	case 0:
		ok = (SetPriorityClass(hProcess, NORMAL_PRIORITY_CLASS) == TRUE);
		break;
	case -1:
		if(!(ok = (SetPriorityClass(hProcess, BELOW_NORMAL_PRIORITY_CLASS) == TRUE)))
		{
			ok = (SetPriorityClass(hProcess, IDLE_PRIORITY_CLASS) == TRUE);
		}
		break;
	case -2:
		ok = (SetPriorityClass(hProcess, IDLE_PRIORITY_CLASS) == TRUE);
		break;
	}

	return ok;
}

/*
 * Play a sound (from resources)
 */
bool x264_play_sound(const unsigned short uiSoundIdx, const bool bAsync, const wchar_t *alias)
{
	if(alias)
	{
		return PlaySound(alias, GetModuleHandle(NULL), (SND_ALIAS | (bAsync ? SND_ASYNC : SND_SYNC))) == TRUE;
	}
	else
	{
		return PlaySound(MAKEINTRESOURCE(uiSoundIdx), GetModuleHandle(NULL), (SND_RESOURCE | (bAsync ? SND_ASYNC : SND_SYNC))) == TRUE;
	}
}

/*
 * Current process ID
 */
unsigned int x264_process_id(void)
{
	return GetCurrentProcessId();
}

/*
 * Make a window blink (to draw user's attention)
 */
void x264_blink_window(QWidget *poWindow, unsigned int count, unsigned int delay)
{
	static QMutex blinkMutex;

	const double maxOpac = 1.0;
	const double minOpac = 0.3;
	const double delOpac = 0.1;

	if(!blinkMutex.tryLock())
	{
		qWarning("Blinking is already in progress, skipping!");
		return;
	}
	
	try
	{
		const int steps = static_cast<int>(ceil(maxOpac - minOpac) / delOpac);
		const int sleep = static_cast<int>(floor(static_cast<double>(delay) / static_cast<double>(steps)));
		const double opacity = poWindow->windowOpacity();
	
		for(unsigned int i = 0; i < count; i++)
		{
			for(double x = maxOpac; x >= minOpac; x -= delOpac)
			{
				poWindow->setWindowOpacity(x);
				QApplication::processEvents();
				Sleep(sleep);
			}

			for(double x = minOpac; x <= maxOpac; x += delOpac)
			{
				poWindow->setWindowOpacity(x);
				QApplication::processEvents();
				Sleep(sleep);
			}
		}

		poWindow->setWindowOpacity(opacity);
		QApplication::processEvents();
		blinkMutex.unlock();
	}
	catch(...)
	{
		blinkMutex.unlock();
		qWarning("Exception error while blinking!");
	}
}

/*
 * Bring the specifed window to the front
 */
bool x264_bring_to_front(const QWidget *win)
{
	const bool ret = (SetForegroundWindow(win->winId()) == TRUE);
	SwitchToThisWindow(win->winId(), TRUE);
	return ret;
}

/*
 * Check if file is a valid Win32/Win64 executable
 */
bool x264_is_executable(const QString &path)
{
	bool bIsExecutable = false;
	DWORD binaryType;
	if(GetBinaryType(QWCHAR(QDir::toNativeSeparators(path)), &binaryType))
	{
		bIsExecutable = (binaryType == SCS_32BIT_BINARY || binaryType == SCS_64BIT_BINARY);
	}
	return bIsExecutable;
}

/*
 * Read value from registry
 */
QString x264_query_reg_string(const bool bUser, const QString &path, const QString &name)
{
	QString result; HKEY hKey = NULL;
	if(RegOpenKey((bUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE), QWCHAR(path), &hKey) == ERROR_SUCCESS)
	{
		const size_t DATA_LEN = 2048; wchar_t data[DATA_LEN];
		DWORD type = REG_NONE, size = sizeof(wchar_t) * DATA_LEN;
		if(RegQueryValueEx(hKey, QWCHAR(name), NULL, &type, ((BYTE*)&data[0]), &size) == ERROR_SUCCESS)
		{
			if((type == REG_SZ) || (type == REG_EXPAND_SZ))
			{
				result = WCHAR2QSTR(&data[0]);
			}
		}
		RegCloseKey(hKey);
	}
	return result;
}

/*
 * Display the window's close button
 */
bool x264_enable_close_button(const QWidget *win, const bool bEnable)
{
	bool ok = false;

	if(HMENU hMenu = GetSystemMenu(win->winId(), FALSE))
	{
		ok = (EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | (bEnable ? MF_ENABLED : MF_GRAYED)) == TRUE);
	}

	return ok;
}

bool x264_beep(int beepType)
{
	switch(beepType)
	{
		case x264_beep_info:    return MessageBeep(MB_ICONASTERISK) == TRUE;    break;
		case x264_beep_warning: return MessageBeep(MB_ICONEXCLAMATION) == TRUE; break;
		case x264_beep_error:   return MessageBeep(MB_ICONHAND) == TRUE;        break;
		default: return false;
	}
}

/*
 * Shutdown the computer
 */
bool x264_shutdown_computer(const QString &message, const unsigned long timeout, const bool forceShutdown)
{
	HANDLE hToken = NULL;

	if(OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		TOKEN_PRIVILEGES privileges;
		memset(&privileges, 0, sizeof(TOKEN_PRIVILEGES));
		privileges.PrivilegeCount = 1;
		privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		
		if(LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid))
		{
			if(AdjustTokenPrivileges(hToken, FALSE, &privileges, NULL, NULL, NULL))
			{
				const DWORD reason = SHTDN_REASON_MAJOR_APPLICATION | SHTDN_REASON_FLAG_PLANNED;
				return InitiateSystemShutdownEx(NULL, const_cast<wchar_t*>(QWCHAR(message)), timeout, forceShutdown ? TRUE : FALSE, FALSE, reason);
			}
		}
	}
	
	return false;
}

/*
 * Check for debugger (detect routine)
 */
static __forceinline bool x264_check_for_debugger(void)
{
	__try
	{
		CloseHandle((HANDLE)((DWORD_PTR)-3));
	}
	__except(1)
	{
		return true;
	}
	__try 
	{
		__debugbreak();
	}
	__except(1) 
	{
		return IsDebuggerPresent();
	}
	return true;
}

/*
 * Check for debugger (thread proc)
 */
static unsigned int __stdcall x264_debug_thread_proc(LPVOID lpParameter)
{
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
	forever
	{
		if(x264_check_for_debugger())
		{
			x264_fatal_exit(L"Not a debug build. Please unload debugger and try again!");
			return 666;
		}
		x264_sleep(100);
	}
}

/*
 * Check for debugger (startup routine)
 */
static HANDLE x264_debug_thread_init()
{
	if(x264_check_for_debugger())
	{
		x264_fatal_exit(L"Not a debug build. Please unload debugger and try again!");
	}
	const uintptr_t h = _beginthreadex(NULL, 0, x264_debug_thread_proc, NULL, 0, NULL);
	return (HANDLE)(h^0xdeadbeef);
}

/*
 * Fatal application exit
 */
#pragma intrinsic(_InterlockedExchange)
void x264_fatal_exit(const wchar_t* exitMessage, const wchar_t* errorBoxMessage)
{
	static volatile long bFatalFlag = 0L;

	if(_InterlockedExchange(&bFatalFlag, 1L) == 0L)
	{
		if(GetCurrentThreadId() != g_main_thread_id)
		{
			HANDLE mainThread = OpenThread(THREAD_TERMINATE, FALSE, g_main_thread_id);
			if(mainThread) TerminateThread(mainThread, ULONG_MAX);
		}
	
		if(errorBoxMessage)
		{
			MessageBoxW(NULL, errorBoxMessage, L"Simple x264 Launcher - GURU MEDITATION", MB_ICONERROR | MB_TOPMOST | MB_TASKMODAL);
		}

		FatalAppExit(0, exitMessage);

		for(;;)
		{
			TerminateProcess(GetCurrentProcess(), -1);
		}
	}
}

/*
 * Entry point checks
 */
static DWORD x264_entry_check(void);
static DWORD g_x264_entry_check_result = x264_entry_check();
static DWORD g_x264_entry_check_flag = 0x789E09B2;
static DWORD x264_entry_check(void)
{
	volatile DWORD retVal = 0xA199B5AF;
	if(g_x264_entry_check_flag != 0x8761F64D)
	{
		x264_fatal_exit(L"Application initialization has failed, take care!");
	}
	return retVal;
}

/*
 * Application entry point (runs before static initializers)
 */
extern "C"
{
	int WinMainCRTStartup(void);
	
	int x264_entry_point(void)
	{
		if((!X264_DEBUG) && x264_check_for_debugger())
		{
			x264_fatal_exit(L"Not a debug build. Please unload debugger and try again!");
		}
		if(g_x264_entry_check_flag != 0x789E09B2)
		{
			x264_fatal_exit(L"Application initialization has failed, take care!");
		}

		//Zero *before* constructors are called
		X264_ZERO_MEMORY(g_x264_argv);
		X264_ZERO_MEMORY(g_x264_os_version);
		X264_ZERO_MEMORY(g_x264_portable);

		//Make sure we will pass the check
		g_x264_entry_check_flag = ~g_x264_entry_check_flag;

		//Now initialize the C Runtime library!
		return WinMainCRTStartup();
	}
}

/*
 * Initialize debug thread
 */
static const HANDLE g_debug_thread = X264_DEBUG ? NULL : x264_debug_thread_init();

/*
 * Get number private bytes [debug only]
 */
size_t x264_dbg_private_bytes(void)
{
#if X264_DEBUG
	PROCESS_MEMORY_COUNTERS_EX memoryCounters;
	memoryCounters.cb = sizeof(PROCESS_MEMORY_COUNTERS_EX);
	GetProcessMemoryInfo(GetCurrentProcess(), (PPROCESS_MEMORY_COUNTERS) &memoryCounters, sizeof(PROCESS_MEMORY_COUNTERS_EX));
	return memoryCounters.PrivateUsage;
#else
	throw "Cannot call this function in a non-debug build!";
#endif //X264_DEBUG
}

/*
 * Finalization function
 */
void x264_finalization(void)
{
	//Destroy Qt application object
	QApplication *application = dynamic_cast<QApplication*>(QApplication::instance());
	X264_DELETE(application);

	//Free STDOUT and STDERR buffers
	if(g_x264_console_attached)
	{
		if(std::filebuf *tmp = dynamic_cast<std::filebuf*>(std::cout.rdbuf()))
		{
			std::cout.rdbuf(NULL);
			X264_DELETE(tmp);
		}
		if(std::filebuf *tmp = dynamic_cast<std::filebuf*>(std::cerr.rdbuf()))
		{
			std::cerr.rdbuf(NULL);
			X264_DELETE(tmp);
		}
	}
}
