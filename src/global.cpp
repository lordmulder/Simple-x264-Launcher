///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
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
}
g_x264_version =
{
	(VER_X264_MAJOR),
	(VER_X264_MINOR),
	(VER_X264_PATCH),
	(VER_X264_BUILD),
};

//Portable mode
static QReadWriteLock g_portableModeLock;
static bool           g_portableModeData = false;
static bool           g_portableModeInit = false;

//Data path
static QString        g_dataPathData;
static QReadWriteLock g_dataPathLock;

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

/*
 * Check for portable mode
 */
bool x264_is_portable(void)
{
	QReadLocker readLock(&g_portableModeLock);

	if(g_portableModeInit)
	{
		return g_portableModeData;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_portableModeLock);

	if(!g_portableModeInit)
	{
		if(VER_X264_PORTABLE_EDITION)
		{
			qWarning("Simple x264 Launcher portable edition!\n");
			g_portableModeData = true;
		}
		else
		{
			QString baseName = QFileInfo(QApplication::applicationFilePath()).completeBaseName();
			int idx1 = baseName.indexOf("x264", 0, Qt::CaseInsensitive);
			int idx2 = baseName.lastIndexOf("portable", -1, Qt::CaseInsensitive);
			g_portableModeData = (idx1 >= 0) && (idx2 >= 0) && (idx1 < idx2);
		}
		g_portableModeInit = true;
	}
	
	return g_portableModeData;
}

/*
 * Get data path (i.e. path to store config files)
 */
const QString &x264_data_path(void)
{
	QReadLocker readLock(&g_dataPathLock);

	if(!g_dataPathData.isEmpty())
	{
		return g_dataPathData;
	}
	
	readLock.unlock();
	QWriteLocker writeLock(&g_dataPathLock);
	
	if(g_dataPathData.isEmpty())
	{
		g_dataPathData = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
		if(g_dataPathData.isEmpty() || x264_is_portable())
		{
			g_dataPathData = QApplication::applicationDirPath();
		}
		if(!QDir(g_dataPathData).mkpath("."))
		{
			qWarning("Data directory could not be created:\n%s\n", g_dataPathData.toUtf8().constData());
			g_dataPathData = QDir::currentPath();
		}
	}
	
	return g_dataPathData;
}

/*
 * Is pre-release version?
 */
bool x264_is_prerelease(void)
{
	return (VER_X264_PRE_RELEASE);
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
