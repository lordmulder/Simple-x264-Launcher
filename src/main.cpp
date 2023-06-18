///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2023 LoRd_MuldeR <MuldeR2@GMX.de>
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

//Internal
#include "global.h"
#include "win_main.h"
#include "cli.h"
#include "ipc.h"
#include "thread_ipc_send.h"

//MUtils
#include <MUtils/Startup.h>
#include <MUtils/OSSupport.h>
#include <MUtils/CPUFeatures.h>
#include <MUtils/IPCChannel.h>
#include <MUtils/Version.h>

//Qt includes
#include <QApplication>
#include <QDate>
#include <QPlastiqueStyle>
#include <QFile>
#include <QTextStream>

//Windows includes
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////

static void x264_print_logo(void)
{
	//Print version info
	qDebug("Simple x264 Launcher v%u.%02u.%u - use 64-Bit x264 with 32-Bit Avisynth", x264_version_major(), x264_version_minor(), x264_version_build());
	qDebug("Copyright (c) 2004-%04d LoRd_MuldeR <mulder2@gmx.de>. Some rights reserved.", qMax(MUtils::Version::app_build_date().year(), MUtils::OS::current_date().year()));
	qDebug("Built on %s at %s with %s for Win-%s.\n", MUTILS_UTF8(MUtils::Version::app_build_date().toString(Qt::ISODate)), MUTILS_UTF8(MUtils::Version::app_build_time().toString(Qt::ISODate)), MUtils::Version::compiler_version(), MUtils::Version::compiler_arch());

	//print license info
	qDebug("This program is free software: you can redistribute it and/or modify");
	qDebug("it under the terms of the GNU General Public License <http://www.gnu.org/>.");
	qDebug("Note that this program is distributed with ABSOLUTELY NO WARRANTY.\n");

	//Print library version
	qDebug("This application is powerd by MUtils library v%u.%02u (%s, %s).\n", MUtils::Version::lib_version_major(), MUtils::Version::lib_version_minor(), MUTILS_UTF8(MUtils::Version::lib_build_date().toString(Qt::ISODate)), MUTILS_UTF8(MUtils::Version::lib_build_time().toString(Qt::ISODate)));

	//Print warning, if this is a "debug" build
	if(MUTILS_DEBUG)
	{
		qWarning("---------------------------------------------------------");
		qWarning("DEBUG BUILD: DO NOT RELEASE THIS BINARY TO THE PUBLIC !!!");
		qWarning("---------------------------------------------------------\n"); 
	}
}

static int x264_initialize_ipc(MUtils::IPCChannel *const ipcChannel)
{
		int iResult = 0;

	if((iResult = ipcChannel->initialize()) != MUtils::IPCChannel::RET_SUCCESS_MASTER)
	{
		if(iResult == MUtils::IPCChannel::RET_SUCCESS_SLAVE)
		{
			qDebug("Simple x264 Launcher is already running, connecting to running instance...");
			QScopedPointer<IPCThread_Send> messageProducerThread(new IPCThread_Send(ipcChannel));
			messageProducerThread->start();
			if(!messageProducerThread->wait(30000))
			{
				qWarning("MessageProducer thread has encountered timeout -> going to kill!");
				messageProducerThread->terminate();
				messageProducerThread->wait();
				MUtils::OS::system_message_err(L"Simple x264 Launcher", L"Simple x264 Launcher is already running, but the running instance doesn't respond!");
				return -1;
			}
			return 0;
		}
		else
		{
			qFatal("The IPC initialization has failed!");
			return -1;
		}
	}

	return 1;
}

///////////////////////////////////////////////////////////////////////////////
// Main function
///////////////////////////////////////////////////////////////////////////////

static int simple_x264_main(int &argc, char **argv)
{
	int iResult = -1;

	//Print logo
	x264_print_logo();

	//Get CLI arguments
	const MUtils::OS::ArgumentMap &arguments = MUtils::OS::arguments();

	//Enumerate CLI arguments
	if(!arguments.isEmpty())
	{
		qDebug("Command-Line Arguments:");
		foreach(const QString &key, arguments.uniqueKeys())
		{
			foreach(const QString &val, arguments.values(key))
			{
				if(!val.isEmpty())
				{
					qDebug("--%s = \"%s\"", MUTILS_UTF8(key), MUTILS_UTF8(val));
					continue;
				}
				qDebug("--%s", MUTILS_UTF8(key));
			}
		}
		qDebug(" ");
	}

	//Detect CPU capabilities
	const MUtils::CPUFetaures::cpu_info_t cpuFeatures = MUtils::CPUFetaures::detect();
	qDebug("   CPU vendor id  :  %s (Intel=%s)", cpuFeatures.idstr, MUTILS_BOOL2STR(cpuFeatures.vendor & MUtils::CPUFetaures::VENDOR_INTEL));
	qDebug("CPU brand string  :  %s", cpuFeatures.brand);
	qDebug("   CPU signature  :  Family=%d, Model=%d, Stepping=%d", cpuFeatures.family, cpuFeatures.model, cpuFeatures.stepping);
	qDebug("CPU architecture  :  %s", cpuFeatures.x64 ? "x64 (64-Bit)" : "x86 (32-Bit)");
	qDebug("CPU capabilities  :  CMOV=%s, MMX=%s, SSE=%s, SSE2=%s, SSE3=%s, SSSE3=%s", MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_CMOV), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_MMX), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE2), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE3), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSSE3));
	qDebug("CPU capabilities  :  SSE4.1=%s, SSE4.2=%s, AVX=%s, AVX2=%s, FMA3=%s, LZCNT=%s", MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE41), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE42), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_AVX), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_AVX2), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_FMA3), MUTILS_BOOL2STR(cpuFeatures.features & MUtils::CPUFetaures::FLAG_LZCNT));
	qDebug(" Number of CPU's  :  %d\n", cpuFeatures.count);

	//Initialize Qt
	QScopedPointer<QApplication> application(MUtils::Startup::create_qt(argc, argv, QLatin1String("Simple x264 Launcher"), QLatin1String("LoRd_MuldeR"), QLatin1String("muldersoft.com"), false));
	if(application.isNull())
	{
		return EXIT_FAILURE;
	}

	//Initialize application
	application->setWindowIcon(QIcon(":/icons/movie.ico"));
	application->setApplicationVersion(QString().sprintf("%d.%02d.%04d", x264_version_major(), x264_version_minor(), x264_version_build())); 

	//Initialize the IPC handler class
	QScopedPointer<MUtils::IPCChannel> ipcChannel(new MUtils::IPCChannel("simple-x264-launcher", x264_version_build(), "instance"));
	if((iResult = x264_initialize_ipc(ipcChannel.data())) < 1)
	{
		return (iResult == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
	}

	//Running in portable mode?
	if(x264_is_portable())
	{
		qDebug("Application is running in portable mode!\n");
	}

	//Set style
	if(!arguments.contains(CLI_PARAM_NO_GUI_STYLE))
	{
		qApp->setStyle(new QPlastiqueStyle());
	}
	if (arguments.contains(CLI_PARAM_DARK_GUI_MODE))
	{
		QFile qss(":qdarkstyle/style.qss");
		if (qss.open(QFile::ReadOnly | QFile::Text))
		{
			QTextStream textStream(&qss);
			application->setStyleSheet(textStream.readAll());
		}
	}

	//Create Main Window
	QScopedPointer<MainWindow> mainWindow(new MainWindow(cpuFeatures, ipcChannel.data()));
	mainWindow->show();

	//Run application
	int ret = qApp->exec();
	
	//Exit program
	return ret;
}

///////////////////////////////////////////////////////////////////////////////
// Applicaton entry point
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
	return MUtils::Startup::startup(argc, argv, simple_x264_main, "Simple x264 Launcher", x264_is_prerelease());
}
