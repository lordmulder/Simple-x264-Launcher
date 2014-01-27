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

#include "thread_vapoursynth.h"

#include <QLibrary>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <QApplication>
#include <QDir>

#include "global.h"

QMutex VapourSynthCheckThread::m_vpsLock;
QFile *VapourSynthCheckThread::m_vpsExePath = NULL;
QFile *VapourSynthCheckThread::m_vpsDllPath = NULL;
QLibrary *VapourSynthCheckThread::m_vpsLib = NULL;

#define VALID_DIR(STR) ((!(STR).isEmpty()) && QDir((STR)).exists())

static inline QString &cleanDir(QString &path)
{
	if(!path.isEmpty())
	{
		path = QDir::fromNativeSeparators(path);
		while(path.endsWith('/'))
		{
			path.chop(1);
		}
	}
	return path;
}

//-------------------------------------
// External API
//-------------------------------------

int VapourSynthCheckThread::detect(QString &path)
{
	path.clear();
	QMutexLocker lock(&m_vpsLock);

	QEventLoop loop;
	VapourSynthCheckThread thread;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(15000, &loop, SLOT(quit()));
	
	qDebug("VapourSynth thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("VapourSynth thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(1000))
	{
		qWarning("VapourSynth thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return -1;
	}

	if(thread.getException())
	{
		qWarning("VapourSynth thread encountered an exception !!!");
		return -1;
	}
	
	if(thread.getSuccess())
	{
		path = thread.getPath();
		qDebug("VapourSynth check completed successfully.");
		return 1;
	}

	qWarning("VapourSynth thread failed to detect installation!");
	return 0;
}

void VapourSynthCheckThread::unload(void)
{
	QMutexLocker lock(&m_vpsLock);

	if(m_vpsLib)
	{
		if(m_vpsLib->isLoaded())
		{
			m_vpsLib->unload();
		}
	}

	if(m_vpsExePath)
	{
		if (m_vpsExePath->isOpen())
		{
			m_vpsExePath->close();
		}
	}

	if(m_vpsDllPath)
	{
		if(m_vpsDllPath->isOpen())
		{
			m_vpsDllPath->close();
		}
	}

	X264_DELETE(m_vpsExePath);
	X264_DELETE(m_vpsDllPath);
	X264_DELETE(m_vpsLib);
}

//-------------------------------------
// Thread class
//-------------------------------------

VapourSynthCheckThread::VapourSynthCheckThread(void)
{
	m_success = false;
	m_exception = false;
	m_vpsPath.clear();
}

VapourSynthCheckThread::~VapourSynthCheckThread(void)
{
}

void VapourSynthCheckThread::run(void)
{
	m_exception = m_success = false;
	m_success = detectVapourSynthPath1(m_vpsPath, &m_exception);
}

bool VapourSynthCheckThread::detectVapourSynthPath1(QString &path, volatile bool *exception)
{
	__try
	{
		return detectVapourSynthPath2(path, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in VapourSynth thread !!!");
		return false;
	}
}

bool VapourSynthCheckThread::detectVapourSynthPath2(QString &path, volatile bool *exception)
{
	try
	{
		return detectVapourSynthPath3(path);
	}
	catch(...)
	{
		*exception = true;
		qWarning("VapourSynth initializdation raised an C++ exception!");
		return false;
	}
}

bool VapourSynthCheckThread::detectVapourSynthPath3(QString &path)
{
	bool success = false;
	X264_DELETE(m_vpsExePath);
	X264_DELETE(m_vpsDllPath);
	path.clear();

	static const char *VPS_REG_KEYS[] = 
	{
		"SOFTWARE\\VapourSynth",
		"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VapourSynth_is1",
		NULL
	};
	static const char *VPS_REG_NAME[] =
	{
		"Path",
		"InstallLocation",
		"Inno Setup: App Path",
		NULL
	};

	//Read VapourSynth path from registry
	QString vapoursynthPath;
	for(size_t i = 0; VPS_REG_KEYS[i]; i++)
	{
		for(size_t j = 0; VPS_REG_NAME[j]; j++)
		{
			vapoursynthPath = cleanDir(x264_query_reg_string(false, VPS_REG_KEYS[i], VPS_REG_NAME[j]));
			if(VALID_DIR(vapoursynthPath)) break;
		}
		if(VALID_DIR(vapoursynthPath)) break;
	}

	//Make sure VapourSynth does exist
	if(!VALID_DIR(vapoursynthPath))
	{
		qWarning("VapourSynth install path not found -> disable Vapousynth support!");
		vapoursynthPath.clear();
	}

	//Make sure that 'vapoursynth.dll' and 'vspipe.exe' are available
	bool vapoursynthComplete = false;
	if(!vapoursynthPath.isEmpty())
	{
		static const char *CORE_PATH[2] = { "core32", "core" };
		qDebug("VapourSynth Dir: %s", vapoursynthPath.toUtf8().constData());
		for(int i = 0; (i < 2) && (!vapoursynthComplete); i++)
		{
			QFileInfo vpsExeInfo(QString("%1/%2/vspipe.exe"     ).arg(vapoursynthPath, CORE_PATH[i]));
			QFileInfo vpsDllInfo(QString("%1/%2/vapoursynth.dll").arg(vapoursynthPath, CORE_PATH[i]));
			qDebug("VapourSynth EXE: %s", vpsExeInfo.absoluteFilePath().toUtf8().constData());
			qDebug("VapourSynth DLL: %s", vpsDllInfo.absoluteFilePath().toUtf8().constData());
			if(vpsExeInfo.exists() && vpsDllInfo.exists())
			{
				m_vpsExePath = new QFile(vpsExeInfo.canonicalFilePath());
				m_vpsDllPath = new QFile(vpsDllInfo.canonicalFilePath());
				if(m_vpsExePath->open(QIODevice::ReadOnly) && m_vpsDllPath->open(QIODevice::ReadOnly))
				{
					if(vapoursynthComplete = x264_is_executable(m_vpsExePath->fileName()))
					{
						vapoursynthPath.append("/").append(CORE_PATH[i]);
					}
					break;
				}
				X264_DELETE(m_vpsExePath);
				X264_DELETE(m_vpsDllPath);
			}
		}
		if(!vapoursynthComplete)
		{
			qWarning("VapourSynth installation incomplete -> disable Vapousynth support!");
		}
	}

	//Make sure 'vsscript.dll' can be loaded successfully
	if(vapoursynthComplete)
	{
		if(!m_vpsLib)
		{
			m_vpsLib = new QLibrary("vsscript.dll");
		}
		if(m_vpsLib->isLoaded() || m_vpsLib->load())
		{
			static const char *VSSCRIPT_ENTRY = "_vsscript_init@0";
			qDebug("VapourSynth scripting library loaded.");
			if(!(success = (m_vpsLib->resolve(VSSCRIPT_ENTRY) != NULL)))
			{
				qWarning("Entrypoint '%s' not found in VSSCRIPT.DLL !!!", VSSCRIPT_ENTRY);
			}
		}
		else
		{
			qWarning("Failed to load VSSCRIPT.DLL !!!");
		}
	}

	//Return VapourSynth path
	if(success)
	{
		path = vapoursynthPath;
	}

	return success;
}
