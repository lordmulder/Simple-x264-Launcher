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

#include "global.h"
#include "version.h"

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

//CRT includes
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include <intrin.h>

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

//Compiler detection
//The following code was borrowed from MPC-HC project: http://mpc-hc.sf.net/
#if defined(__INTEL_COMPILER)
	#if (__INTEL_COMPILER >= 1200)
		static const char *g_x264_version_compiler = "ICL 12.x";
	#elif (__INTEL_COMPILER >= 1100)
		static const char *g_x264_version_compiler = = "ICL 11.x";
	#elif (__INTEL_COMPILER >= 1000)
		static const char *g_x264_version_compiler = = "ICL 10.x";
	#else
		#error Compiler is not supported!
	#endif
#elif defined(_MSC_VER)
	#if (_MSC_VER == 1600)
		#if (_MSC_FULL_VER >= 160040219)
			static const char *g_x264_version_compiler = "MSVC 2010-SP1";
		#else
			static const char *g_x264_version_compiler = "MSVC 2010";
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
			x264_COMPILER_WARNING("SSE instruction set is enabled!")
		#elif (_M_IX86_FP == 2)
			x264_COMPILER_WARNING("SSE2 instruction set is enabled!")
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

	FatalAppExit(0, L"Unhandeled exception handler invoked, application will exit!");
	TerminateProcess(GetCurrentProcess(), -1);
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

	FatalAppExit(0, L"Invalid parameter handler invoked, application will exit!");
	TerminateProcess(GetCurrentProcess(), -1);
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
 * Qt message handler
 */
void x264_message_handler(QtMsgType type, const char *msg)
{
	static const char *GURU_MEDITATION = "\n\nGURU MEDITATION !!!\n\n";
	
	QMutexLocker lock(&g_x264_message_mutex);

	if(g_x264_log_file)
	{
		static char prefix[] = "DWCF";
		int index = qBound(0, static_cast<int>(type), 3);
		unsigned int timestamp = static_cast<unsigned int>(_time64(NULL) % 3600I64);
		QString str = QString::fromUtf8(msg).trimmed().replace('\n', '\t');
		fprintf(g_x264_log_file, "[%c][%04u] %s\r\n", prefix[index], timestamp, str.toUtf8().constData());
		fflush(g_x264_log_file);
	}

	if(g_x264_console_attached)
	{
		UINT oldOutputCP = GetConsoleOutputCP();
		if(oldOutputCP != CP_UTF8) SetConsoleOutputCP(CP_UTF8);

		switch(type)
		{
		case QtCriticalMsg:
		case QtFatalMsg:
			fflush(stdout);
			fflush(stderr);
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
	else
	{
		QString temp("[x264][%1] %2");
		
		switch(type)
		{
		case QtCriticalMsg:
		case QtFatalMsg:
			temp = temp.arg("C", QString::fromUtf8(msg));
			break;
		case QtWarningMsg:
			temp = temp.arg("W", QString::fromUtf8(msg));
			break;
		default:
			temp = temp.arg("I", QString::fromUtf8(msg));
			break;
		}

		temp.replace("\n", "\t").append("\n");
		OutputDebugStringA(temp.toLatin1().constData());
	}

	if(type == QtCriticalMsg || type == QtFatalMsg)
	{
		lock.unlock();
		MessageBoxW(NULL, QWCHAR(QString::fromUtf8(msg)), L"Simple x264 Launcher - GURU MEDITATION", MB_ICONERROR | MB_TOPMOST | MB_TASKMODAL);
		FatalAppExit(0, L"The application has encountered a critical error and will exit now!");
		TerminateProcess(GetCurrentProcess(), -1);
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
 * Check for portable mode
 */
bool x264_portable(void)
{
	static bool detected = false;
	static bool portable = false;

	if(!detected)
	{
		portable = portable || QFileInfo(QApplication::applicationFilePath()).baseName().contains(QRegExp("^portable[^A-Za-z0-9]", Qt::CaseInsensitive));
		portable = portable || QFileInfo(QApplication::applicationFilePath()).baseName().contains(QRegExp("[^A-Za-z0-9]portable[^A-Za-z0-9]", Qt::CaseInsensitive));
		portable = portable || QFileInfo(QApplication::applicationFilePath()).baseName().contains(QRegExp("[^A-Za-z0-9]portable$", Qt::CaseInsensitive));
		detected = true;
	}

	return portable;
}

/*
 * Get data path (i.e. path to store config files)
 */
const QString &x264_data_path(void)
{
	static QString *pathCache = NULL;
	
	if(!pathCache)
	{
		pathCache = new QString();
		if(!x264_portable())
		{
			*pathCache = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
		}
		if(pathCache->isEmpty() || x264_portable())
		{
			*pathCache = QApplication::applicationDirPath();
		}
		if(!QDir(*pathCache).mkpath("."))
		{
			qWarning("Data directory could not be created:\n%s\n", pathCache->toUtf8().constData());
			*pathCache = QDir::currentPath();
		}
	}
	
	return *pathCache;
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
const x264_cpu_t x264_detect_cpu_features(int argc, char **argv)
{
	typedef BOOL (WINAPI *IsWow64ProcessFun)(__in HANDLE hProcess, __out PBOOL Wow64Process);
	typedef VOID (WINAPI *GetNativeSystemInfoFun)(__out LPSYSTEM_INFO lpSystemInfo);
	
	static IsWow64ProcessFun IsWow64ProcessPtr = NULL;
	static GetNativeSystemInfoFun GetNativeSystemInfoPtr = NULL;

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
	memcpy(CPUIdentificationString, &CPUInfo[1], 4);
	memcpy(CPUIdentificationString + 4, &CPUInfo[3], 4);
	memcpy(CPUIdentificationString + 8, &CPUInfo[2], 4);
	features.intel = (_stricmp(CPUIdentificationString, "GenuineIntel") == 0);
	strncpy_s(features.vendor, 0x40, CPUIdentificationString, _TRUNCATE);

	if(CPUInfo[0] >= 1)
	{
		x264_cpu_cpuid(1, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
		features.mmx = (CPUInfo[3] & 0x800000U) || false;
		features.sse = (CPUInfo[3] & 0x2000000U) || false;
		features.sse2 = (CPUInfo[3] & 0x4000000U) || false;
		features.ssse3 = (CPUInfo[2] & 0x200U) || false;
		features.sse3 = (CPUInfo[2] & 0x1U) || false;
		features.ssse3 = (CPUInfo[2] & 0x200U) || false;
		features.stepping = CPUInfo[0] & 0xf;
		features.model = ((CPUInfo[0] >> 4) & 0xf) + (((CPUInfo[0] >> 16) & 0xf) << 4);
		features.family = ((CPUInfo[0] >> 8) & 0xf) + ((CPUInfo[0] >> 20) & 0xff);
		if(features.sse) features.mmx2 = true; //MMXEXT is a subset of SSE!
	}

	x264_cpu_cpuid(0x80000000U, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
	unsigned int nExIds = qBound(0x80000000U, CPUInfo[0], 0x80000004U);

	if((_stricmp(CPUIdentificationString, "AuthenticAMD") == 0) && (nExIds >= 0x80000001U))
	{
		x264_cpu_cpuid(0x80000001U, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
		features.mmx2 = features.mmx2 || (CPUInfo[3] & 0x00400000U);
	}

	for(unsigned int i = 0x80000002U; i <= nExIds; ++i)
	{
		x264_cpu_cpuid(i, &CPUInfo[0], &CPUInfo[1], &CPUInfo[2], &CPUInfo[3]);
		switch(i)
		{
		case 0x80000002U:
			memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
			break;
		case 0x80000003U:
			memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
			break;
		case 0x80000004U:
			memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
			break;
		}
	}

	strncpy_s(features.brand, 0x40, CPUBrandString, _TRUNCATE);

	if(strlen(features.brand) < 1) strncpy_s(features.brand, 0x40, "Unknown", _TRUNCATE);
	if(strlen(features.vendor) < 1) strncpy_s(features.vendor, 0x40, "Unknown", _TRUNCATE);

#if !defined(_M_X64 ) && !defined(_M_IA64)
	if(!IsWow64ProcessPtr || !GetNativeSystemInfoPtr)
	{
		QLibrary Kernel32Lib("kernel32.dll");
		IsWow64ProcessPtr = (IsWow64ProcessFun) Kernel32Lib.resolve("IsWow64Process");
		GetNativeSystemInfoPtr = (GetNativeSystemInfoFun) Kernel32Lib.resolve("GetNativeSystemInfo");
	}
	if(IsWow64ProcessPtr)
	{
		BOOL x64 = FALSE;
		if(IsWow64ProcessPtr(GetCurrentProcess(), &x64))
		{
			features.x64 = x64;
		}
	}
	if(GetNativeSystemInfoPtr)
	{
		GetNativeSystemInfoPtr(&systemInfo);
	}
	else
	{
		GetSystemInfo(&systemInfo);
	}
	features.count = qBound(1UL, systemInfo.dwNumberOfProcessors, 64UL);
#else
	GetNativeSystemInfo(&systemInfo);
	features.count = systemInfo.dwNumberOfProcessors;
	features.x64 = true;
#endif

	if((argv != NULL) && (argc > 0))
	{
		bool flag = false;
		for(int i = 0; i < argc; i++)
		{
			if(!_stricmp("--force-cpu-no-64bit", argv[i])) { flag = true; features.x64 = false; }
			if(!_stricmp("--force-cpu-no-mmx", argv[i])) { flag = true; features.mmx = false; }
			if(!_stricmp("--force-cpu-no-mmx2", argv[i])) { flag = true; features.mmx2 = false; }
			if(!_stricmp("--force-cpu-no-sse", argv[i])) { flag = true; features.sse = features.sse2 = features.sse3 = features.ssse3 = false; }
			if(!_stricmp("--force-cpu-no-intel", argv[i])) { flag = true; features.intel = false; }
			
			if(!_stricmp("--force-cpu-have-64bit", argv[i])) { flag = true; features.x64 = true; }
			if(!_stricmp("--force-cpu-have-mmx", argv[i])) { flag = true; features.mmx = true; }
			if(!_stricmp("--force-cpu-have-mmx2", argv[i])) { flag = true; features.mmx2 = true; }
			if(!_stricmp("--force-cpu-have-sse", argv[i])) { flag = true; features.sse = features.sse2 = features.sse3 = features.ssse3 = true; }
			if(!_stricmp("--force-cpu-have-intel", argv[i])) { flag = true; features.intel = true; }
		}
		if(flag) qWarning("CPU flags overwritten by user-defined parameters. Take care!\n");
	}

	return features;
}

/*
 * Get the native operating system version
 */
DWORD x264_get_os_version(void)
{
	OSVERSIONINFO osVerInfo;
	memset(&osVerInfo, 0, sizeof(OSVERSIONINFO));
	osVerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	DWORD version = 0;
	
	if(GetVersionEx(&osVerInfo) == TRUE)
	{
		if(osVerInfo.dwPlatformId != VER_PLATFORM_WIN32_NT)
		{
			throw "Ouuups: Not running under Windows NT. This is not supposed to happen!";
		}
		version = (DWORD)((osVerInfo.dwMajorVersion << 16) | (osVerInfo.dwMinorVersion & 0xffff));
	}

	return version;
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
	bool isWine = false;
	typedef BOOL (WINAPI *SetDllDirectoryProc)(WCHAR *lpPathName);

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
		kernel32.unload();
	}

	//Extract executable name from argv[] array
	char *executableName = argv[0];
	while(char *temp = strpbrk(executableName, "\\/:?"))
	{
		executableName = temp + 1;
	}

	//Check Qt version
	qDebug("Using Qt v%s [%s], %s, %s", qVersion(), QLibraryInfo::buildDate().toString(Qt::ISODate).toLatin1().constData(), (qSharedBuild() ? "DLL" : "Static"), QLibraryInfo::buildKey().toLatin1().constData());
	qDebug("Compiled with Qt v%s [%s], %s\n", QT_VERSION_STR, QT_PACKAGEDATE_STR, QT_BUILD_KEY);
	if(_stricmp(qVersion(), QT_VERSION_STR))
	{
		qFatal("%s", QApplication::tr("Executable '%1' requires Qt v%2, but found Qt v%3.").arg(QString::fromLatin1(executableName), QString::fromLatin1(QT_VERSION_STR), QString::fromLatin1(qVersion())).toLatin1().constData());
		return false;
	}
	if(QLibraryInfo::buildKey().compare(QString::fromLatin1(QT_BUILD_KEY), Qt::CaseInsensitive))
	{
		qFatal("%s", QApplication::tr("Executable '%1' was built for Qt '%2', but found Qt '%3'.").arg(QString::fromLatin1(executableName), QString::fromLatin1(QT_BUILD_KEY), QLibraryInfo::buildKey()).toLatin1().constData());
		return false;
	}

	//Check the Windows version
	switch(QSysInfo::windowsVersion() & QSysInfo::WV_NT_based)
	{
	case 0:
	case QSysInfo::WV_NT:
	case QSysInfo::WV_2000:
		qFatal("%s", QApplication::tr("Executable '%1' requires Windows XP or later.").arg(QString::fromLatin1(executableName)).toLatin1().constData());
		break;
		//qDebug("Running on Windows 2000 (not officially supported!).\n");
		//x264_check_compatibility_mode("GetNativeSystemInfo", executableName);
	case QSysInfo::WV_XP:
		qDebug("Running on Windows XP.\n");
		x264_check_compatibility_mode("GetLargePageMinimum", executableName);
		break;
	case QSysInfo::WV_2003:
		qDebug("Running on Windows Server 2003 or Windows XP x64-Edition.\n");
		x264_check_compatibility_mode("GetLocaleInfoEx", executableName);
		break;
	case QSysInfo::WV_VISTA:
		qDebug("Running on Windows Vista or Windows Server 2008.\n");
		x264_check_compatibility_mode("CreateRemoteThreadEx", executableName);
		break;
	case QSysInfo::WV_WINDOWS7:
		qDebug("Running on Windows 7 or Windows Server 2008 R2.\n");
		x264_check_compatibility_mode(NULL, executableName);
		break;
	default:
		{
			DWORD osVersionNo = x264_get_os_version();
			qWarning("Running on an unknown/untested WinNT-based OS (v%u.%u).\n", HIWORD(osVersionNo), LOWORD(osVersionNo));
		}
		break;
	}

	//Check for Wine
	QLibrary ntdll("ntdll.dll");
	if(ntdll.load())
	{
		if(ntdll.resolve("wine_nt_to_unix_file_name") != NULL) isWine = true;
		if(ntdll.resolve("wine_get_version") != NULL) isWine = true;
		if(isWine) qWarning("It appears we are running under Wine, unexpected things might happen!\n");
		ntdll.unload();
	}

	//Create Qt application instance and setup version info
	QApplication *application = new QApplication(argc, argv);
	application->setApplicationName("Simple x264 Launcher");
	application->setApplicationVersion(QString().sprintf("%d.%02d", x264_version_major(), x264_version_minor())); 
	application->setOrganizationName("LoRd_MuldeR");
	application->setOrganizationDomain("mulder.at.gg");
	application->setWindowIcon(QIcon(":/icons/movie.ico"));
	
	//application->setEventFilter(x264_event_filter);

	//Set text Codec for locale
	QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));

	//Load plugins from application directory
	QCoreApplication::setLibraryPaths(QStringList() << QApplication::applicationDirPath());
	qDebug("Library Path:\n%s\n", QApplication::libraryPaths().first().toUtf8().constData());

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
	// g_x264_translation.files.insert(x264_DEFAULT_LANGID, "");
	// g_x264_translation.names.insert(x264_DEFAULT_LANGID, "English");

	//Check for process elevation
	if(!x264_check_elevation())
	{
		if(QMessageBox::warning(NULL, "Simple x264 Launcher", "<nobr>Program was started with elevated rights. This is a potential security risk!</nobr>", "Quit Program (Recommended)", "Ignore") == 0)
		{
			return false;
		}
	}

	//Update console icon, if a console is attached
	if(g_x264_console_attached && !isWine)
	{
		typedef DWORD (__stdcall *SetConsoleIconFun)(HICON);
		QLibrary kernel32("kernel32.dll");
		if(kernel32.load())
		{
			SetConsoleIconFun SetConsoleIconPtr = (SetConsoleIconFun) kernel32.resolve("SetConsoleIcon");
			if(SetConsoleIconPtr != NULL) SetConsoleIconPtr(QIcon(":/icons/movie.ico").pixmap(16, 16).toWinHICON());
			kernel32.unload();
		}
	}

	//Done
	qt_initialized = true;
	return true;
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
static bool x264_check_for_debugger(void)
{
	__try 
	{
		DebugBreak();
	}
	__except(GetExceptionCode() == EXCEPTION_BREAKPOINT ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) 
	{
		return false;
	}
	return true;
}

/*
 * Check for debugger (thread proc)
 */
static void WINAPI x264_debug_thread_proc(__in LPVOID lpParameter)
{
	while(!(IsDebuggerPresent() || x264_check_for_debugger()))
	{
		Sleep(333);
	}
	TerminateProcess(GetCurrentProcess(), -1);
}

/*
 * Check for debugger (startup routine)
 */
static HANDLE x264_debug_thread_init(void)
{
	if(IsDebuggerPresent() || x264_check_for_debugger())
	{
		FatalAppExit(0, L"Not a debug build. Please unload debugger and try again!");
		TerminateProcess(GetCurrentProcess(), -1);
	}

	return CreateThread(NULL, NULL, reinterpret_cast<LPTHREAD_START_ROUTINE>(&x264_debug_thread_proc), NULL, NULL, NULL);
}

/*
 * Initialize debug thread
 */
static const HANDLE g_debug_thread = X264_DEBUG ? NULL : x264_debug_thread_init();

/*
 * Get number private bytes [debug only]
 */
SIZE_T x264_dbg_private_bytes(void)
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
	/* NOP */
}
