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

//x264 includes
#include "global.h"
#include "targetver.h"

//Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <MMSystem.h>
#include <ShellAPI.h>
#include <Objbase.h>
#include <Psapi.h>
#include <SensAPI.h>

//C++ includes
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <time.h>

//VLD
#include <vld.h>

//Version
#define ENABLE_X264_VERSION_INCLUDE
#include "version.h"
#undef  ENABLE_X264_VERSION_INCLUDE

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

//Global types
typedef HRESULT (WINAPI *SHGetKnownFolderPath_t)(const GUID &rfid, DWORD dwFlags, HANDLE hToken, PWSTR *ppszPath);
typedef HRESULT (WINAPI *SHGetFolderPath_t)(HWND hwndOwner, int nFolder, HANDLE hToken, DWORD dwFlags, LPWSTR pszPath);

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
	unsigned int ver_x264_avs2yuv_ver;
}
g_x264_version =
{
	(VER_X264_MAJOR),
	(VER_X264_MINOR),
	(VER_X264_PATCH),
	(VER_X264_BUILD),
	__DATE__,
	__TIME__,
	(VER_X264_AVS2YUV_VER)
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

//Special folders
static struct
{
	QMap<size_t, QString> *knownFolders;
	SHGetKnownFolderPath_t getKnownFolderPath;
	SHGetFolderPath_t getFolderPath;
	QReadWriteLock lock;
}
g_x264_known_folder;

//%TEMP% folder
static struct
{
	QString *path;
	QReadWriteLock lock;
}
g_x264_temp_folder;

//Wine detection
static struct
{
	bool bInitialized;
	bool bIsWine;
	QReadWriteLock lock;
}
g_x264_wine;

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

//Check for CLI flag
static inline bool _CHECK_FLAG(const int argc, char **argv, const char *flag)
{
	for(int i = 1; i < argc; i++)
	{
		if(_stricmp(argv[i], flag) == 0) return true;
	}
	return false;
}

#define CHECK_FLAG(FLAG) _CHECK_FLAG(argc, argv, "--" FLAG)
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
 * Get a random string
 */
QString x264_rand_str(const bool bLong)
{
	const QUuid uuid = QUuid::createUuid().toString();

	const unsigned int u1 = uuid.data1;
	const unsigned int u2 = (((unsigned int)(uuid.data2)) << 16) | ((unsigned int)(uuid.data3));
	const unsigned int u3 = (((unsigned int)(uuid.data4[0])) << 24) | (((unsigned int)(uuid.data4[1])) << 16) | (((unsigned int)(uuid.data4[2])) << 8) | ((unsigned int)(uuid.data4[3]));
	const unsigned int u4 = (((unsigned int)(uuid.data4[4])) << 24) | (((unsigned int)(uuid.data4[5])) << 16) | (((unsigned int)(uuid.data4[6])) << 8) | ((unsigned int)(uuid.data4[7]));

	return bLong ? QString().sprintf("%08x%08x%08x%08x", u1, u2, u3, u4) : QString().sprintf("%08x%08x", (u1 ^ u2), (u3 ^ u4));
}

/*
 * Robert Jenkins' 96 bit Mix Function
 * Source: http://www.concentric.net/~Ttwang/tech/inthash.htm
 */
static unsigned int x264_mix(const unsigned int x, const unsigned int y, const unsigned int z)
{
	unsigned int a = x;
	unsigned int b = y;
	unsigned int c = z;
	
	a=a-b;  a=a-c;  a=a^(c >> 13);
	b=b-c;  b=b-a;  b=b^(a << 8); 
	c=c-a;  c=c-b;  c=c^(b >> 13);
	a=a-b;  a=a-c;  a=a^(c >> 12);
	b=b-c;  b=b-a;  b=b^(a << 16);
	c=c-a;  c=c-b;  c=c^(b >> 5);
	a=a-b;  a=a-c;  a=a^(c >> 3);
	b=b-c;  b=b-a;  b=b^(a << 10);
	c=c-a;  c=c-b;  c=c^(b >> 15);

	return c;
}

/*
 * Seeds the random number generator
 * Note: Altough rand_s() doesn't need a seed, this must be called pripr to x264_rand(), just to to be sure!
 */
void x264_seed_rand(void)
{
	qsrand(x264_mix(clock(), time(NULL), _getpid()));
}

/*
 * Returns a randum number
 * Note: This function uses rand_s() if available, but falls back to qrand() otherwise
 */
unsigned int x264_rand(void)
{
	quint32 rnd = 0;

	if(rand_s(&rnd) == 0)
	{
		return rnd;
	}

	for(size_t i = 0; i < sizeof(unsigned int); i++)
	{
		rnd = (rnd << 8) ^ qrand();
	}

	return rnd;
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
 * Determines the current date, resistant against certain manipulations
 */
QDate x264_current_date_safe(void)
{
	const DWORD MAX_PROC = 1024;
	DWORD *processes = new DWORD[MAX_PROC];
	DWORD bytesReturned = 0;
	
	if(!EnumProcesses(processes, sizeof(DWORD) * MAX_PROC, &bytesReturned))
	{
		X264_DELETE_ARRAY(processes);
		return QDate::currentDate();
	}

	const DWORD procCount = bytesReturned / sizeof(DWORD);
	ULARGE_INTEGER lastStartTime;
	memset(&lastStartTime, 0, sizeof(ULARGE_INTEGER));

	for(DWORD i = 0; i < procCount; i++)
	{
		HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processes[i]);
		if(hProc)
		{
			FILETIME processTime[4];
			if(GetProcessTimes(hProc, &processTime[0], &processTime[1], &processTime[2], &processTime[3]))
			{
				ULARGE_INTEGER timeCreation;
				timeCreation.LowPart = processTime[0].dwLowDateTime;
				timeCreation.HighPart = processTime[0].dwHighDateTime;
				if(timeCreation.QuadPart > lastStartTime.QuadPart)
				{
					lastStartTime.QuadPart = timeCreation.QuadPart;
				}
			}
			CloseHandle(hProc);
		}
	}

	X264_DELETE_ARRAY(processes);
	
	FILETIME lastStartTime_fileTime;
	lastStartTime_fileTime.dwHighDateTime = lastStartTime.HighPart;
	lastStartTime_fileTime.dwLowDateTime = lastStartTime.LowPart;

	FILETIME lastStartTime_localTime;
	if(!FileTimeToLocalFileTime(&lastStartTime_fileTime, &lastStartTime_localTime))
	{
		memcpy(&lastStartTime_localTime, &lastStartTime_fileTime, sizeof(FILETIME));
	}
	
	SYSTEMTIME lastStartTime_system;
	if(!FileTimeToSystemTime(&lastStartTime_localTime, &lastStartTime_system))
	{
		memset(&lastStartTime_system, 0, sizeof(SYSTEMTIME));
		lastStartTime_system.wYear = 1970; lastStartTime_system.wMonth = lastStartTime_system.wDay = 1;
	}

	const QDate currentDate = QDate::currentDate();
	const QDate processDate = QDate(lastStartTime_system.wYear, lastStartTime_system.wMonth, lastStartTime_system.wDay);
	return (currentDate >= processDate) ? currentDate : processDate;
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
void x264_init_console(const int argc, char **argv)
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

		if(CHECK_FLAG("console"))    enableConsole = true;
		if(CHECK_FLAG("no-console")) enableConsole = false;
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

//FIXME: Remove x264_version_x264_avs2yuv_ver!
unsigned int x264_version_x264_avs2yuv_ver(void)
{
	return g_x264_version.ver_x264_avs2yuv_ver;
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
x264_cpu_t x264_detect_cpu_features(const int argc, char **argv)
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

	bool flag = false;
	if(CHECK_FLAG("force-cpu-no-64bit")) { flag = true; features.x64 = false; }
	if(CHECK_FLAG("force-cpu-no-sse"))   { flag = true; features.sse = features.sse2 = features.sse3 = features.ssse3 = false; }
	if(CHECK_FLAG("force-cpu-no-intel")) { flag = true; features.intel = false; }
	if(flag) qWarning("CPU flags overwritten by user-defined parameters. Take care!\n");

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
	QReadLocker readLock(&g_x264_wine.lock);

	//Already initialized?
	if(g_x264_wine.bInitialized)
	{
		return g_x264_wine.bIsWine;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_x264_wine.lock);

	if(!g_x264_wine.bInitialized)
	{
		g_x264_wine.bIsWine = false;
		QLibrary ntdll("ntdll.dll");
		if(ntdll.load())
		{
			if(ntdll.resolve("wine_nt_to_unix_file_name") != NULL) g_x264_wine.bIsWine = true;
			if(ntdll.resolve("wine_get_version") != NULL) g_x264_wine.bIsWine = true;
			ntdll.unload();
		}
		g_x264_wine.bInitialized = true;
	}

	return g_x264_wine.bIsWine;
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
static bool x264_process_is_elevated(bool *bIsUacEnabled = NULL)
{
	bool bIsProcessElevated = false;
	if(bIsUacEnabled) *bIsUacEnabled = false;
	HANDLE hToken = NULL;
	
	if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		TOKEN_ELEVATION_TYPE tokenElevationType;
		DWORD returnLength;
		if(GetTokenInformation(hToken, TokenElevationType, &tokenElevationType, sizeof(TOKEN_ELEVATION_TYPE), &returnLength))
		{
			if(returnLength == sizeof(TOKEN_ELEVATION_TYPE))
			{
				switch(tokenElevationType)
				{
				case TokenElevationTypeDefault:
					qDebug("Process token elevation type: Default -> UAC is disabled.\n");
					break;
				case TokenElevationTypeFull:
					qWarning("Process token elevation type: Full -> potential security risk!\n");
					bIsProcessElevated = true;
					if(bIsUacEnabled) *bIsUacEnabled = true;
					break;
				case TokenElevationTypeLimited:
					qDebug("Process token elevation type: Limited -> not elevated.\n");
					if(bIsUacEnabled) *bIsUacEnabled = true;
					break;
				default:
					qWarning("Unknown tokenElevationType value: %d", tokenElevationType);
					break;
				}
			}
			else
			{
				qWarning("GetTokenInformation() return an unexpected size!");
			}
		}
		CloseHandle(hToken);
	}
	else
	{
		qWarning("Failed to open process token!");
	}

	return bIsProcessElevated;
}

/*
 * Check if the current user is an administartor (helper function)
 */
static bool x264_user_is_admin_helper(void)
{
	HANDLE hToken = NULL;
	if(!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		return false;
	}

	DWORD dwSize = 0;
	if(!GetTokenInformation(hToken, TokenGroups, NULL, 0, &dwSize))
	{
		if(GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			CloseHandle(hToken);
			return false;
		}
	}

	PTOKEN_GROUPS lpGroups = (PTOKEN_GROUPS) malloc(dwSize);
	if(!lpGroups)
	{
		CloseHandle(hToken);
		return false;
	}

	if(!GetTokenInformation(hToken, TokenGroups, lpGroups, dwSize, &dwSize))
	{
		free(lpGroups);
		CloseHandle(hToken);
		return false;
	}

	PSID lpSid = NULL; SID_IDENTIFIER_AUTHORITY Authority = {SECURITY_NT_AUTHORITY};
	if(!AllocateAndInitializeSid(&Authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &lpSid))
	{
		free(lpGroups);
		CloseHandle(hToken);
		return false;
	}

	bool bResult = false;
	for(DWORD i = 0; i < lpGroups->GroupCount; i++)
	{
		if(EqualSid(lpSid, lpGroups->Groups[i].Sid))
		{
			bResult = true;
			break;
		}
	}

	FreeSid(lpSid);
	free(lpGroups);
	CloseHandle(hToken);
	return bResult;
}

/*
 * Check if the current user is an administartor
 */
bool x264_user_is_admin(void)
{
	bool isAdmin = false;

	//Check for process elevation and UAC support first!
	if(x264_process_is_elevated(&isAdmin))
	{
		qWarning("Process is elevated -> user is admin!");
		return true;
	}
	
	//If not elevated and UAC is not available -> user must be in admin group!
	if(!isAdmin)
	{
		qDebug("UAC is disabled/unavailable -> checking for Administrators group");
		isAdmin = x264_user_is_admin_helper();
	}

	return isAdmin;
}

/*
 * Initialize Qt framework
 */
bool x264_init_qt(int &argc, char **argv)
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
	if(x264_process_is_elevated() && (!x264_detect_wine()))
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
QString x264_path2ansi(const QString &longPath, bool makeLowercase)
{
	QString shortPath = longPath;
	
	const QString longPathNative = QDir::toNativeSeparators(longPath);
	DWORD buffSize = GetShortPathNameW(QWCHAR(longPathNative), NULL, NULL);
	
	if(buffSize > 0)
	{
		wchar_t *buffer = (wchar_t*) _malloca(sizeof(wchar_t) * buffSize);
		DWORD result = GetShortPathNameW(QWCHAR(longPathNative), buffer, buffSize);

		if((result > 0) && (result < buffSize))
		{
			shortPath = QDir::fromNativeSeparators(QString::fromUtf16(reinterpret_cast<const unsigned short*>(buffer), result));

			if(makeLowercase)
			{
				QFileInfo info(shortPath);
				shortPath = QString("%1/%2").arg(info.absolutePath(), info.fileName().toLower());
			}
		}

		_freea(buffer);
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
 * Current process ID
 */
unsigned int x264_process_id(QProcess &process)
{
	if(Q_PID pid = process.pid())
	{
		return pid->dwProcessId;
	}
	return NULL;
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
static bool x264_bring_to_front(const HWND hWin)
{
	if(hWin)
	{
		const bool ret = (SetForegroundWindow(hWin) != FALSE);
		SwitchToThisWindow(hWin, TRUE);
		return ret;
	}
	return false;
}

/*
 * Bring the specifed window to the front
 */
bool x264_bring_to_front(const QWidget *win)
{
	if(win)
	{
		return x264_bring_to_front(win->winId());
	}
	return false;
}

/*
 * Bring window of the specifed process to the front (callback)
 */
static BOOL CALLBACK x264_bring_process_to_front_helper(HWND hwnd, LPARAM lParam)
{
	DWORD processId = *reinterpret_cast<WORD*>(lParam);
	DWORD windowProcessId = NULL;
	GetWindowThreadProcessId(hwnd, &windowProcessId);
	if(windowProcessId == processId)
	{
		x264_bring_to_front(hwnd);
		return FALSE;
	}
	return TRUE;
}

/*
 * Bring window of the specifed process to the front
 */
bool x264_bring_process_to_front(const unsigned long pid)
{
	return EnumWindows(x264_bring_process_to_front_helper, reinterpret_cast<LPARAM>(&pid)) == TRUE;
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
 * Locate known folder on local system
 */
const QString &x264_known_folder(x264_known_folder_t folder_id)
{
	static const int CSIDL_FLAG_CREATE = 0x8000;
	typedef enum { KF_FLAG_CREATE = 0x00008000 } kf_flags_t;
	
	struct
	{
		const int csidl;
		const GUID guid;
	}
	static s_folders[] =
	{
		{ 0x001c, {0xF1B32785,0x6FBA,0x4FCF,{0x9D,0x55,0x7B,0x8E,0x7F,0x15,0x70,0x91}} },  //CSIDL_LOCAL_APPDATA
		{ 0x0026, {0x905e63b6,0xc1bf,0x494e,{0xb2,0x9c,0x65,0xb7,0x32,0xd3,0xd2,0x1a}} },  //CSIDL_PROGRAM_FILES
		{ 0x0024, {0xF38BF404,0x1D43,0x42F2,{0x93,0x05,0x67,0xDE,0x0B,0x28,0xFC,0x23}} },  //CSIDL_WINDOWS_FOLDER
		{ 0x0025, {0x1AC14E77,0x02E7,0x4E5D,{0xB7,0x44,0x2E,0xB1,0xAE,0x51,0x98,0xB7}} },  //CSIDL_SYSTEM_FOLDER
	};

	size_t folderId = size_t(-1);

	switch(folder_id)
	{
		case x264_folder_localappdata: folderId = 0; break;
		case x264_folder_programfiles: folderId = 1; break;
		case x264_folder_systroot_dir: folderId = 2; break;
		case x264_folder_systemfolder: folderId = 3; break;
	}

	if(folderId == size_t(-1))
	{
		qWarning("Invalid 'known' folder was requested!");
		return *reinterpret_cast<QString*>(NULL);
	}

	QReadLocker readLock(&g_x264_known_folder.lock);

	//Already in cache?
	if(g_x264_known_folder.knownFolders)
	{
		if(g_x264_known_folder.knownFolders->contains(folderId))
		{
			return (*g_x264_known_folder.knownFolders)[folderId];
		}
	}

	//Obtain write lock to initialize
	readLock.unlock();
	QWriteLocker writeLock(&g_x264_known_folder.lock);

	//Still not in cache?
	if(g_x264_known_folder.knownFolders)
	{
		if(g_x264_known_folder.knownFolders->contains(folderId))
		{
			return (*g_x264_known_folder.knownFolders)[folderId];
		}
	}

	//Initialize on first call
	if(!g_x264_known_folder.knownFolders)
	{
		QLibrary shell32("shell32.dll");
		if(shell32.load())
		{
			g_x264_known_folder.getFolderPath =      (SHGetFolderPath_t)      shell32.resolve("SHGetFolderPathW");
			g_x264_known_folder.getKnownFolderPath = (SHGetKnownFolderPath_t) shell32.resolve("SHGetKnownFolderPath");
		}
		g_x264_known_folder.knownFolders = new QMap<size_t, QString>();
	}

	QString folderPath;

	//Now try to get the folder path!
	if(g_x264_known_folder.getKnownFolderPath)
	{
		WCHAR *path = NULL;
		if(g_x264_known_folder.getKnownFolderPath(s_folders[folderId].guid, KF_FLAG_CREATE, NULL, &path) == S_OK)
		{
			//MessageBoxW(0, path, L"SHGetKnownFolderPath", MB_TOPMOST);
			QDir folderTemp = QDir(QDir::fromNativeSeparators(QString::fromUtf16(reinterpret_cast<const unsigned short*>(path))));
			if(folderTemp.exists())
			{
				folderPath = folderTemp.canonicalPath();
			}
			CoTaskMemFree(path);
		}
	}
	else if(g_x264_known_folder.getFolderPath)
	{
		WCHAR *path = new WCHAR[4096];
		if(g_x264_known_folder.getFolderPath(NULL, s_folders[folderId].csidl | CSIDL_FLAG_CREATE, NULL, NULL, path) == S_OK)
		{
			//MessageBoxW(0, path, L"SHGetFolderPathW", MB_TOPMOST);
			QDir folderTemp = QDir(QDir::fromNativeSeparators(QString::fromUtf16(reinterpret_cast<const unsigned short*>(path))));
			if(folderTemp.exists())
			{
				folderPath = folderTemp.canonicalPath();
			}
		}
		X264_DELETE_ARRAY(path);
	}

	//Update cache
	g_x264_known_folder.knownFolders->insert(folderId, folderPath);
	return (*g_x264_known_folder.knownFolders)[folderId];
}

/*
 * Try to initialize the folder (with *write* access)
 */
static QString x264_try_init_folder(const QString &folderPath)
{
	static const char *DATA = "Lorem ipsum dolor sit amet, consectetur, adipisci velit!";
	
	bool success = false;

	const QFileInfo folderInfo(folderPath);
	const QDir folderDir(folderInfo.absoluteFilePath());

	//Create folder, if it does *not* exist yet
	for(int i = 0; i < 16; i++)
	{
		if(folderDir.exists()) break;
		folderDir.mkpath(".");
	}

	//Make sure folder exists now *and* is writable
	if(folderDir.exists())
	{
		const QByteArray testData = QByteArray(DATA);
		for(int i = 0; i < 32; i++)
		{
			QFile testFile(folderDir.absoluteFilePath(QString("~%1.tmp").arg(x264_rand_str())));
			if(testFile.open(QIODevice::ReadWrite | QIODevice::Truncate))
			{
				if(testFile.write(testData) >= testData.size())
				{
					success = true;
				}
				testFile.remove();
				testFile.close();
			}
			if(success) break;
		}
	}

	return (success ? folderDir.canonicalPath() : QString());
}

/*
 * Detect the TEMP directory
 */
const QString &x264_temp_directory(void)
{
	QReadLocker readLock(&g_x264_temp_folder.lock);

	if(g_x264_temp_folder.path)
	{
		return *g_x264_temp_folder.path;
	}

	readLock.unlock();
	QWriteLocker writeLock(&g_x264_temp_folder.lock);

	if(!g_x264_temp_folder.path)
	{
		//Try %TEMP% first
		g_x264_temp_folder.path = new QString(x264_try_init_folder(QDir::temp().absolutePath()));

		//Fall back to %LOCALAPPDATA%, if %TEMP% didn't work
		if(g_x264_temp_folder.path->isEmpty())
		{
			qWarning("%%TEMP%% directory not found -> falling back to %%LOCALAPPDATA%%");
			static const x264_known_folder_t folderId[2] = { x264_folder_localappdata, x264_folder_systroot_dir };
			for(size_t id = 0; (g_x264_temp_folder.path->isEmpty() && (id < 2)); id++)
			{
				const QString &localAppData = x264_known_folder(x264_folder_localappdata);
				if(!localAppData.isEmpty())
				{
					*g_x264_temp_folder.path = x264_try_init_folder(QString("%1/Temp").arg(localAppData));
				}
				else
				{
					qWarning("%%LOCALAPPDATA%% directory could not be found!");
				}
			}
		}

		//Failed to init TEMP folder?
		if(g_x264_temp_folder.path->isEmpty())
		{
			qWarning("Temporary directory could not be initialized !!!");
		}
	}

	return *g_x264_temp_folder.path;
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

/*
 * Play beep sound
 */
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
 * Check the network connection status
 */
int x264_network_status(void)
{
	DWORD dwFlags;
	const BOOL ret = IsNetworkAlive(&dwFlags);
	if(GetLastError() == 0)
	{
		return (ret != FALSE) ? x264_network_yes : x264_network_non;
	}
	return x264_network_err;
}

/*
 * Setup QPorcess object
 */
void x264_init_process(QProcess &process, const QString &wokringDir, const bool bReplaceTempDir)
{
	//Environment variable names
	static const char *const s_envvar_names_temp[] =
	{
		"TEMP", "TMP", "TMPDIR", "HOME", "USERPROFILE", "HOMEPATH", NULL
	};
	static const char *const s_envvar_names_remove[] =
	{
		"WGETRC", "SYSTEM_WGETRC", "HTTP_PROXY", "FTP_PROXY", "NO_PROXY", "GNUPGHOME", "LC_ALL", "LC_COLLATE", "LC_CTYPE", "LC_MESSAGES", "LC_MONETARY", "LC_NUMERIC", "LC_TIME", "LANG", NULL
	};

	//Initialize environment
	QProcessEnvironment env = process.processEnvironment();
	if(env.isEmpty()) env = QProcessEnvironment::systemEnvironment();

	//Clean a number of enviroment variables that might affect our tools
	for(size_t i = 0; s_envvar_names_remove[i]; i++)
	{
		env.remove(QString::fromLatin1(s_envvar_names_remove[i]));
		env.remove(QString::fromLatin1(s_envvar_names_remove[i]).toLower());
	}

	const QString tempDir = QDir::toNativeSeparators(x264_temp_directory());

	//Replace TEMP directory in environment
	if(bReplaceTempDir)
	{
		for(size_t i = 0; s_envvar_names_temp[i]; i++)
		{
			env.insert(s_envvar_names_temp[i], tempDir);
		}
	}

	//Setup PATH variable
	const QString path = env.value("PATH", QString()).trimmed();
	env.insert("PATH", path.isEmpty() ? tempDir : QString("%1;%2").arg(tempDir, path));
	
	//Setup QPorcess object
	process.setWorkingDirectory(wokringDir);
	process.setProcessChannelMode(QProcess::MergedChannels);
	process.setReadChannel(QProcess::StandardOutput);
	process.setProcessEnvironment(env);
}

/*
 * Inform the system that it is in use, thereby preventing the system from entering sleep
 */
bool x264_set_thread_execution_state(const bool systemRequired)
{
	EXECUTION_STATE state = NULL;
	if(systemRequired)
	{
		state = SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_AWAYMODE_REQUIRED);
	}
	else
	{
		state = SetThreadExecutionState(ES_CONTINUOUS);
	}
	return (state != NULL);
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
		X264_ZERO_MEMORY(g_x264_known_folder);
		X264_ZERO_MEMORY(g_x264_temp_folder);

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
	
	//Clear CLI args
	X264_DELETE(g_x264_argv.list);

	//Clear folders cache
	X264_DELETE(g_x264_known_folder.knownFolders);
	X264_DELETE(g_x264_temp_folder.path);
}
