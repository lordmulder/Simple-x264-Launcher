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

#include "thread_binaries.h"

#include <QLibrary>
#include <QEventLoop>
#include <QTimer>
#include <QMutexLocker>
#include <QApplication>
#include <QProcess>
#include <QDir>

//Internal
#include "global.h"
#include "model_sysinfo.h"
#include "win_updater.h"
#include "binaries.h"
#include "encoder_factory.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/OSSupport.h>

//Static
QMutex BinariesCheckThread::m_binLock;
QScopedPointer<QFile> BinariesCheckThread::m_binPath[MAX_BINARIES];

//Whatever
#define NEXT(X) ((*reinterpret_cast<int*>(&(X)))++)
#define SHFL(X) ((*reinterpret_cast<int*>(&(X))) <<= 1)

//-------------------------------------
// External API
//-------------------------------------

bool BinariesCheckThread::check(SysinfoModel *sysinfo)
{
	QMutexLocker lock(&m_binLock);

	QEventLoop loop;
	BinariesCheckThread thread(sysinfo);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()), &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(30000, &loop, SLOT(quit()));
	
	qDebug("Binaries checker thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("Binaries checker thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(1000))
	{
		qWarning("Binaries checker thread encountered timeout -> probably deadlock!");
		thread.terminate();
		thread.wait();
		return false;
	}

	if(thread.getException())
	{
		qWarning("Binaries checker thread encountered an exception !!!");
		return false;
	}
	
	return thread.getSuccess();
}

//-------------------------------------
// Thread class
//-------------------------------------

BinariesCheckThread::BinariesCheckThread(const SysinfoModel *const sysinfo)
:
	m_sysinfo(sysinfo)
{
	m_success = m_exception = false;
}

BinariesCheckThread::~BinariesCheckThread(void)
{
}

void BinariesCheckThread::run(void)
{
	m_success = m_exception = false;
	checkBinaries1(m_success, m_sysinfo, &m_exception);
}

void BinariesCheckThread::checkBinaries1(volatile bool &success, const SysinfoModel *const sysinfo, volatile bool *exception)
{
	__try
	{
		checkBinaries2(success, sysinfo, exception);
	}
	__except(1)
	{
		*exception = true;
		qWarning("Unhandled exception error in binaries checker thread !!!");
	}
}

void BinariesCheckThread::checkBinaries2(volatile bool &success, const SysinfoModel *const sysinfo, volatile bool *exception)
{
	try
	{
		return checkBinaries3(success, sysinfo);
	}
	catch(...)
	{
		*exception = true;
		qWarning("Binaries checker initializdation raised an C++ exception!");
	}
}

void BinariesCheckThread::checkBinaries3(volatile bool &success, const SysinfoModel *const sysinfo)
{
	success = true;

	//Create list of all required binary files
	QStringList binFiles;
	for(OptionsModel::EncType encdr = OptionsModel::EncType_X264; encdr <= OptionsModel::EncType_X265; NEXT(encdr))
	{
		const AbstractEncoderInfo &encInfo = EncoderFactory::getEncoderInfo(encdr);
		const QFlags<OptionsModel::EncVariant> variants = encInfo.getVariants();
		for(OptionsModel::EncArch arch = OptionsModel::EncArch_x86_32; arch <= OptionsModel::EncArch_x86_64; NEXT(arch))
		{
			for(OptionsModel::EncVariant varnt = OptionsModel::EncVariant_MIN; varnt <= OptionsModel::EncVariant_MAX; SHFL(varnt))
			{
				if(variants.testFlag(varnt))
				{
					binFiles << encInfo.getBinaryPath(sysinfo, arch, varnt);
				}
			}
		}
	}
	for(OptionsModel::EncArch arch = OptionsModel::EncArch_x86_32; arch <= OptionsModel::EncArch_x86_64; NEXT(arch))
	{
		binFiles << AVS_BINARY(sysinfo, arch == OptionsModel::EncArch_x86_64);
		binFiles << CHK_BINARY(sysinfo, arch == OptionsModel::EncArch_x86_64);
	}
	for(size_t i = 0; UpdaterDialog::BINARIES[i].name; i++)
	{
		if(UpdaterDialog::BINARIES[i].exec)
		{
			binFiles << QString("%1/toolset/common/%2").arg(sysinfo->getAppPath(), QString::fromLatin1(UpdaterDialog::BINARIES[i].name));
		}
	}

	//Actually validate the binaries
	size_t currentFile = 0;
	for(QStringList::ConstIterator iter = binFiles.constBegin(); iter != binFiles.constEnd(); iter++)
	{
		QScopedPointer<QFile> file(new QFile(*iter));
		qDebug("%s", MUTILS_UTF8(file->fileName()));

		if(file->open(QIODevice::ReadOnly))
		{
			if(!MUtils::OS::is_executable_file(file->fileName()))
			{
				success = false;
				qWarning("Required tool does NOT look like a valid Win32/Win64 binary:\n%s\n", MUTILS_UTF8(file->fileName()));
				return;
			}
			if(currentFile < MAX_BINARIES)
			{
				m_binPath[currentFile++].reset(file.take());
				continue;
			}
			qFatal("Current binary file exceeds max. number of binaries!");
		}
		else
		{
			success = false;
			qWarning("Required tool could not be found or access denied:\n%s\n", MUTILS_UTF8(file->fileName()));
			return;
		}
	}
}
