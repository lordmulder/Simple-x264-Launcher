///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
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

//MUtils includes
#include <MUtils/Global.h>

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
}
g_x264_version =
{
	(VER_X264_MAJOR),
	(VER_X264_MINOR),
	(VER_X264_PATCH),
	(VER_X264_BUILD),
	__DATE__,
	__TIME__,
};

//Portable Mode
static struct
{
	bool bInitialized;
	bool bPortableModeEnabled;
	QReadWriteLock lock;
}
g_x264_portable;

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
		static const char *g_x264_version_compiler = "ICL 13." x264_MAKE_STR(__INTEL_COMPILER_BUILD_DATE);
	#elif (__INTEL_COMPILER >= 1200)
		static const char *g_x264_version_compiler = "ICL 12." x264_MAKE_STR(__INTEL_COMPILER_BUILD_DATE);
	#elif (__INTEL_COMPILER >= 1100)
		static const char *g_x264_version_compiler = "ICL 11.x";
	#elif (__INTEL_COMPILER >= 1000)
		static const char *g_x264_version_compiler = "ICL 10.x";
	#else
		#error Compiler is not supported!
	#endif
#elif defined(_MSC_VER)
	#if (_MSC_VER == 1800)
		#if (_MSC_FULL_VER == 180021005)
			static const char *g_x264_version_compiler = "MSVC 2013";
		#elif (_MSC_FULL_VER == 180030501)
			static const char *g_x264_version_compiler = "MSVC 2013.2";
		#elif (_MSC_FULL_VER == 180030723)
			static const char *g_x264_version_compiler = "MSVC 2013.3";
		#elif (_MSC_FULL_VER == 180031101)
			static const char *g_x264_version_compiler = "MSVC 2013.4";
		#else
			#error Compiler version is not supported yet!
		#endif
	#elif (_MSC_VER == 1700)
		#if (_MSC_FULL_VER == 170050727)
			static const char *g_x264_version_compiler = "MSVC 2012";
		#elif (_MSC_FULL_VER == 170051106)
			static const char *g_x264_version_compiler = "MSVC 2012.1";
		#elif (_MSC_FULL_VER == 170060315)
			static const char *g_x264_version_compiler = "MSVC 2012.2";
		#elif (_MSC_FULL_VER == 170060610)
			static const char *g_x264_version_compiler = "MSVC 2012.3";
		#elif (_MSC_FULL_VER == 170061030)
			static const char *g_x264_version_compiler = "MSVC 2012.4";
		#else
			#error Compiler version is not supported yet!
		#endif
	#elif (_MSC_VER == 1600)
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
			x264_COMPILER_WARNING("SSE2 (or higher) instruction set is enabled!")
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
	DWORD buffSize = GetShortPathNameW(MUTILS_WCHR(longPathNative), NULL, NULL);
	
	if(buffSize > 0)
	{
		wchar_t *buffer = (wchar_t*) _malloca(sizeof(wchar_t) * buffSize);
		DWORD result = GetShortPathNameW(MUTILS_WCHR(longPathNative), buffer, buffSize);

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
 * Read value from registry
 */
QString x264_query_reg_string(const bool bUser, const QString &path, const QString &name)
{
	QString result; HKEY hKey = NULL;
	if(RegOpenKey((bUser ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE), MUTILS_WCHR(path), &hKey) == ERROR_SUCCESS)
	{
		const size_t DATA_LEN = 2048; wchar_t data[DATA_LEN];
		DWORD type = REG_NONE, size = sizeof(wchar_t) * DATA_LEN;
		if(RegQueryValueEx(hKey, MUTILS_WCHR(name), NULL, &type, ((BYTE*)&data[0]), &size) == ERROR_SUCCESS)
		{
			if((type == REG_SZ) || (type == REG_EXPAND_SZ))
			{
				result = MUTILS_QSTR(&data[0]);
			}
		}
		RegCloseKey(hKey);
	}
	return result;
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
