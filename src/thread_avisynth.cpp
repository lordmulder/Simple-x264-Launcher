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

#include "thread_avisynth.h"

#include <QLibrary>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <QApplication>

//Internal
#include "global.h"
#include "3rd_party/avisynth_c.h"

//MUtils
#include <MUtils/Global.h>

QMutex AvisynthCheckThread::m_avsLock;
QScopedPointer<QLibrary> AvisynthCheckThread::m_avsLib;

//-------------------------------------
// External API
//-------------------------------------

int AvisynthCheckThread::detect(volatile double *version)
{
	*version = 0.0;
	QMutexLocker lock(&m_avsLock);

	QEventLoop loop;
	AvisynthCheckThread thread;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(15000, &loop, SLOT(quit()));
	
	qDebug("Avisynth thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("Avisynth thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(1000))
	{
		qWarning("Avisynth thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return -1;
	}

	if(thread.getException())
	{
		qWarning("Avisynth thread encountered an exception !!!");
		return -1;
	}
	
	if(thread.getSuccess())
	{
		*version = thread.getVersion();
		qDebug("Version check completed: %.2f", *version);
		return 1;
	}

	qWarning("Avisynth thread failed to determine the version!");
	return 0;
}

//-------------------------------------
// Thread class
//-------------------------------------

AvisynthCheckThread::AvisynthCheckThread(void)
{
	m_success = false;
	m_exception = false;
	m_version = 0.0;
}

AvisynthCheckThread::~AvisynthCheckThread(void)
{
}

void AvisynthCheckThread::run(void)
{
	m_exception = m_success = false;
	m_success = detectAvisynthVersion1(&m_version, &m_exception);
}

bool AvisynthCheckThread::detectAvisynthVersion1(volatile double *version_number, volatile bool *exception)
{
	__try
	{
		return detectAvisynthVersion2(version_number, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in Avisynth thread !!!");
		return false;
	}
}

bool AvisynthCheckThread::detectAvisynthVersion2(volatile double *version_number, volatile bool *exception)
{
	try
	{
		return detectAvisynthVersion3(version_number);
	}
	catch(...)
	{
		*exception = true;
		qWarning("Avisynth initializdation raised an C++ exception!");
		return false;
	}
}

bool AvisynthCheckThread::detectAvisynthVersion3(volatile double *version_number)
{
	bool success = false;
	*version_number = 0.0;

	m_avsLib.reset(new QLibrary("avisynth.dll"));
	if(m_avsLib->isLoaded() || m_avsLib->load())
	{
		avs_create_script_environment_func avs_create_script_environment_ptr = (avs_create_script_environment_func) m_avsLib->resolve("avs_create_script_environment");
		avs_invoke_func avs_invoke_ptr = (avs_invoke_func) m_avsLib->resolve("avs_invoke");
		avs_function_exists_func avs_function_exists_ptr = (avs_function_exists_func) m_avsLib->resolve("avs_function_exists");
		avs_delete_script_environment_func avs_delete_script_environment_ptr = (avs_delete_script_environment_func) m_avsLib->resolve("avs_delete_script_environment");
		avs_release_value_func avs_release_value_ptr = (avs_release_value_func) m_avsLib->resolve("avs_release_value");
	
		if((avs_create_script_environment_ptr != NULL) && (avs_invoke_ptr != NULL) && (avs_function_exists_ptr != NULL))
		{
			qDebug("avs_create_script_environment_ptr(AVS_INTERFACE_25)");
			AVS_ScriptEnvironment* avs_env = avs_create_script_environment_ptr(AVS_INTERFACE_25);
			if(avs_env != NULL)
			{
				qDebug("avs_function_exists_ptr(avs_env, \"VersionNumber\")");
				if(avs_function_exists_ptr(avs_env, "VersionNumber"))
				{
					qDebug("avs_invoke_ptr(avs_env, \"VersionNumber\", avs_new_value_array(NULL, 0), NULL)");
					AVS_Value avs_version = avs_invoke_ptr(avs_env, "VersionNumber", avs_new_value_array(NULL, 0), NULL);
					if(!avs_is_error(avs_version))
					{
						if(avs_is_float(avs_version))
						{
							qDebug("Avisynth version: v%.2f", avs_as_float(avs_version));
							*version_number = avs_as_float(avs_version);
							if(avs_release_value_ptr) avs_release_value_ptr(avs_version);
							success = true;
						}
						else
						{
							qWarning("Failed to determine version number, Avisynth didn't return a float!");
						}
					}
					else
					{
						qWarning("Failed to determine version number, Avisynth returned an error!");
					}
				}
				else
				{
					qWarning("The 'VersionNumber' function does not exist in your Avisynth DLL, can't determine version!");
				}
				if(avs_delete_script_environment_ptr != NULL)
				{
					avs_delete_script_environment_ptr(avs_env);
					avs_env = NULL;
				}
			}
			else
			{
				qWarning("The Avisynth DLL failed to create the script environment!");
			}
		}
		else
		{
			qWarning("It seems the Avisynth DLL is missing required API functions!");
		}
	}
	else
	{
		qWarning("Failed to load Avisynth.dll library!");
	}

	return success;
}
