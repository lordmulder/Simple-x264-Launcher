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
#include "win_main.h"

//Qt includes
#include <QCoreApplication>
#include <QDate>
#include <QPlastiqueStyle>

///////////////////////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////////////////////

static int x264_main(int argc, char* argv[])
{
	//Init console
	x264_init_console(argc, argv);

	//Print version info
	qDebug("Simple x264 Launcher v%u.%02u - use 64-Bit x264 with 32-Bit Avisynth", x264_version_major(), x264_version_minor());
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

	//Detect CPU capabilities
	const x264_cpu_t cpuFeatures = x264_detect_cpu_features(argc, argv);
	qDebug("   CPU vendor id  :  %s (Intel: %s)", cpuFeatures.vendor, X264_BOOL(cpuFeatures.intel));
	qDebug("CPU brand string  :  %s", cpuFeatures.brand);
	qDebug("   CPU signature  :  Family: %d, Model: %d, Stepping: %d", cpuFeatures.family, cpuFeatures.model, cpuFeatures.stepping);
	qDebug("CPU capabilities  :  MMX=%s, MMXEXT=%s, SSE=%s, SSE2=%s, SSE3=%s, SSSE3=%s, X64=%s", X264_BOOL(cpuFeatures.mmx), X264_BOOL(cpuFeatures.mmx2), X264_BOOL(cpuFeatures.sse), X264_BOOL(cpuFeatures.sse2), X264_BOOL(cpuFeatures.sse3), X264_BOOL(cpuFeatures.ssse3), X264_BOOL(cpuFeatures.x64));
	qDebug(" Number of CPU's  :  %d\n", cpuFeatures.count);

	//Initialize Qt
	if(!x264_init_qt(argc, argv))
	{
		return -1;
	}

	//Set style
	qApp->setStyle(new QPlastiqueStyle());

	//Create Main Window
	MainWindow *mainWin = new MainWindow(&cpuFeatures);
	mainWin->show();

	//Run application
	int ret = qApp->exec();
	
	X264_DELETE(mainWin);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Applicaton entry point
///////////////////////////////////////////////////////////////////////////////

static int _main(int argc, char* argv[])
{
	if(X264_DEBUG)
	{
		int iResult = -1;
		qInstallMsgHandler(x264_message_handler);
		X264_MEMORY_CHECK(iResult = x264_main(argc, argv));
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
			FatalAppExit(0, L"Unhandeled C++ exception error, application will exit!");
			TerminateProcess(GetCurrentProcess(), -1);
		}
		catch(int error)
		{
			fflush(stdout);
			fflush(stderr);
			fprintf(stderr, "\nGURU MEDITATION !!!\n\nException error code: 0x%X\n", error);
			FatalAppExit(0, L"Unhandeled C++ exception error, application will exit!");
			TerminateProcess(GetCurrentProcess(), -1);
		}
		catch(...)
		{
			fflush(stdout);
			fflush(stderr);
			fprintf(stderr, "\nGURU MEDITATION !!!\n");
			FatalAppExit(0, L"Unhandeled C++ exception error, application will exit!");
			TerminateProcess(GetCurrentProcess(), -1);
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
			FatalAppExit(0, L"Unhandeled structured exception error, application will exit!");
			TerminateProcess(GetCurrentProcess(), -1);
		}
	}
}
