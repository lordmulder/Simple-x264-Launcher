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

#include "global.h"
#include "win_main.h"
#include "ipc.h"
#include "taskbar7.h"

//Qt includes
#include <QApplication>
#include <QDate>
#include <QPlastiqueStyle>

//Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

//Forward declaration
void handleMultipleInstances(QStringList args, IPC *ipc);

///////////////////////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////////////////////

static int x264_main(int argc, char* argv[])
{
	//Init console
	x264_init_console(argc, argv);

	//Print version info
	qDebug("Simple x264 Launcher v%u.%02u.%u - use 64-Bit x264 with 32-Bit Avisynth", x264_version_major(), x264_version_minor(), x264_version_build());
	qDebug("Copyright (c) 2004-%04d LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.", qMax(x264_version_date().year(),QDate::currentDate().year()));
	qDebug("Built on %s at %s with %s for Win-%s.\n", x264_version_date().toString(Qt::ISODate).toLatin1().constData(), x264_version_time(), x264_version_compiler(), x264_version_arch());
	
	//print license info
	qDebug("This program is free software: you can redistribute it and/or modify");
	qDebug("it under the terms of the GNU General Public License <http://www.gnu.org/>.");
	qDebug("Note that this program is distributed with ABSOLUTELY NO WARRANTY.\n");

	//Print warning, if this is a "debug" build
	if(X264_DEBUG)
	{
		qWarning("---------------------------------------------------------");
		qWarning("DEBUG BUILD: DO NOT RELEASE THIS BINARY TO THE PUBLIC !!!");
		qWarning("---------------------------------------------------------\n"); 
	}

	//Get CLI arguments
	const QStringList &arguments = x264_arguments();
	
	//Detect CPU capabilities
	const x264_cpu_t cpuFeatures = x264_detect_cpu_features(arguments);
	qDebug("   CPU vendor id  :  %s (Intel: %s)", cpuFeatures.vendor, X264_BOOL(cpuFeatures.intel));
	qDebug("CPU brand string  :  %s", cpuFeatures.brand);
	qDebug("   CPU signature  :  Family: %d, Model: %d, Stepping: %d", cpuFeatures.family, cpuFeatures.model, cpuFeatures.stepping);
	qDebug("CPU capabilities  :  MMX=%s, MMXEXT=%s, SSE=%s, SSE2=%s, SSE3=%s, SSSE3=%s, X64=%s", X264_BOOL(cpuFeatures.mmx), X264_BOOL(cpuFeatures.mmx2), X264_BOOL(cpuFeatures.sse), X264_BOOL(cpuFeatures.sse2), X264_BOOL(cpuFeatures.sse3), X264_BOOL(cpuFeatures.ssse3), X264_BOOL(cpuFeatures.x64));
	qDebug(" Number of CPU's  :  %d\n", cpuFeatures.count);

	//Initialize the IPC handler class
	bool firstInstance = false;
	IPC *ipc = new IPC();
	if(ipc->initialize(firstInstance))
	{
		if(!firstInstance)
		{
			qDebug("This is *not* the fist instance -> sending all CLI commands to first instance!");
			handleMultipleInstances(arguments, ipc);
			X264_DELETE(ipc);
			return 0;
		}
	}
	else
	{
		qWarning("IPC initialization has failed!");
	}

	//Initialize Qt
	if(!x264_init_qt(argc, argv))
	{
		return -1;
	}
	
	//Running in portable mode?
	if(x264_portable())
	{
		qDebug("Application is running in portable mode!\n");
	}

	//Taskbar init
	WinSevenTaskbar::init();

	//Set style
	if(!qApp->arguments().contains("--no-style", Qt::CaseInsensitive))
	{
		qApp->setStyle(new QPlastiqueStyle());
	}

	//Create Main Window
	MainWindow *mainWin = new MainWindow(&cpuFeatures, ipc);
	mainWin->show();

	//Run application
	int ret = qApp->exec();

	//Taskbar uninit
	WinSevenTaskbar::init();
	
	//Clean up
	X264_DELETE(mainWin);
	X264_DELETE(ipc);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Multi-instance handler
///////////////////////////////////////////////////////////////////////////////

void handleMultipleInstances(QStringList args, IPC *ipc)
{
	bool commandSent = false;

	//Skip the program file name
	args.takeFirst();

	//Process all command-line arguments
	while(!args.isEmpty())
	{
		const QString current = args.takeFirst();
		if(X264_STRCMP(current, "--add") || X264_STRCMP(current, "--add-file"))
		{
			commandSent = true;
			if(!args.isEmpty())
			{
				if(!ipc->sendAsync(IPC::IPC_OPCODE_ADD_FILE, QStringList() << args.takeFirst()))
				{
					break;
				}
			}
			else
			{
				qWarning("Argument for '--add-file' is missing!");
			}
		}
		else if(X264_STRCMP(current, "--add-job"))
		{
			commandSent = true;
			if(args.size() >= 3)
			{
				QStringList lst;
				for(int i = 0; i < 3; i++)
				{
					lst << args.takeFirst();
				}
				if(!ipc->sendAsync(IPC::IPC_OPCODE_ADD_JOB, lst))
				{
					break;
				}
			}
			else
			{
				qWarning("Argument(s) for '--add-job' are missing!");
				args.clear();
			}
		}
		else
		{
			if(!current.startsWith("--"))
			{
				qWarning("Unknown argument: %s", current.toUtf8().constData());
				break;
			}
		}
	}

	//If no argument has been sent yet, send a ping!
	if(!commandSent)
	{
		ipc->sendAsync(IPC::IPC_OPCODE_PING, QStringList());
	}
}

///////////////////////////////////////////////////////////////////////////////
// Applicaton entry point
///////////////////////////////////////////////////////////////////////////////

LONG WINAPI x264_exception_handler(__in struct _EXCEPTION_POINTERS *ExceptionInfo);
void x264_invalid_param_handler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t);

static int _main(int argc, char* argv[])
{
	if(X264_DEBUG)
	{
		int iResult = -1;
		qInstallMsgHandler(x264_message_handler);
		X264_MEMORY_CHECK(x264_main, iResult, argc, argv);
		x264_finalization();
		return iResult;
	}
	else
	{
		int iResult = -1;
		try
		{
			qInstallMsgHandler(x264_message_handler);
			iResult = x264_main(argc, argv);
			x264_finalization();
		}
		catch(char *error)
		{
			fflush(stdout);
			fflush(stderr);
			fprintf(stderr, "\nGURU MEDITATION !!!\n\nException error message: %s\n", error);
			x264_fatal_exit(L"Unhandeled C++ exception error, application will exit!");
		}
		catch(int error)
		{
			fflush(stdout);
			fflush(stderr);
			fprintf(stderr, "\nGURU MEDITATION !!!\n\nException error code: 0x%X\n", error);
			x264_fatal_exit(L"Unhandeled C++ exception error, application will exit!");
		}
		catch(...)
		{
			fflush(stdout);
			fflush(stderr);
			fprintf(stderr, "\nGURU MEDITATION !!!\n");
			x264_fatal_exit(L"Unhandeled C++ exception error, application will exit!");
		}
		return iResult;
	}
}

int main(int argc, char* argv[])
{
	if(X264_DEBUG)
	{
		return _main(argc, argv);
	}
	else
	{
		__try
		{
			SetUnhandledExceptionFilter(x264_exception_handler);
			_set_invalid_parameter_handler(x264_invalid_param_handler);
			return _main(argc, argv);
		}
		__except(1)
		{
			fflush(stdout);
			fflush(stderr);
			fprintf(stderr, "\nGURU MEDITATION !!!\n\nUnhandeled structured exception error! [code: 0x%X]\n", GetExceptionCode());
			x264_fatal_exit(L"Unhandeled structured exception error, application will exit!");
		}
	}
}
