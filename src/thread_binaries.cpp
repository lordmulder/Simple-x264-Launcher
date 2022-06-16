///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2022 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <QSet>
#include <QMutexLocker>
#include <QApplication>
#include <QProcess>
#include <QDir>

//Internal
#include "global.h"
#include "model_sysinfo.h"
#include "win_updater.h"
#include "encoder_factory.h"
#include "source_factory.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/OSSupport.h>

//Static
QMutex BinariesCheckThread::m_binLock;
QScopedPointer<QFile> BinariesCheckThread::m_binPath[MAX_BINARIES];

//Whatever
#define NEXT(X) ((*reinterpret_cast<int*>(&(X)))++)
#define SHFL(X) ((*reinterpret_cast<int*>(&(X))) <<= 1)
#define BOOLIFY(X) (!!(X))

//External
QString AVS_CHECK_BINARY(const SysinfoModel *sysinfo, const bool& x64);

//-------------------------------------
// External API
//-------------------------------------

bool BinariesCheckThread::check(const SysinfoModel *const sysinfo, QString *const failedPath)
{
	QMutexLocker lock(&m_binLock);

	QEventLoop loop;
	BinariesCheckThread thread(sysinfo);

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	connect(&thread, SIGNAL(finished()),   &loop, SLOT(quit()));
	connect(&thread, SIGNAL(terminated()), &loop, SLOT(quit()));
	
	thread.start();
	QTimer::singleShot(30000, &loop, SLOT(quit()));
	
	qDebug("Binaries checker thread has been created, please wait...");
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	qDebug("Binaries checker thread finished.");

	QApplication::restoreOverrideCursor();

	if(!thread.wait(5000))
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
	
	const bool success = BOOLIFY(thread.getSuccess());
	if ((!success) && failedPath)
	{
		*failedPath = thread.getFailedPath();
	}

	return success;
}

//-------------------------------------
// Thread class
//-------------------------------------

BinariesCheckThread::BinariesCheckThread(const SysinfoModel *const sysinfo)
:
	m_sysinfo(sysinfo)
{
	m_failedPath.clear();
}

BinariesCheckThread::~BinariesCheckThread(void)
{
}

void BinariesCheckThread::run(void)
{
	m_failedPath.clear();
	StarupThread::run();
}

int BinariesCheckThread::threadMain(void)
{
	//Create list of all required binary files
	typedef QPair<QString, bool> FileEntry;
	QList<FileEntry> binFiles;
	for(OptionsModel::EncType encdr = OptionsModel::EncType_MIN; encdr <= OptionsModel::EncType_MAX; NEXT(encdr))
	{
		const AbstractEncoderInfo &encInfo = EncoderFactory::getEncoderInfo(encdr);
		const quint32 archCount = encInfo.getArchitectures().count();
		QSet<QString> filesSet;
		for (quint32 archIdx = 0; archIdx < archCount; ++archIdx)
		{
			const QStringList variants = encInfo.getVariants();
			for (quint32 varntIdx = 0; varntIdx < quint32(variants.count()); ++varntIdx)
			{
				const QStringList dependencies = encInfo.getDependencies(m_sysinfo, archIdx, varntIdx);
				for (QStringList::ConstIterator iter = dependencies.constBegin(); iter != dependencies.constEnd(); iter++)
				{
					if (!filesSet.contains(*iter))
					{
						filesSet << (*iter);
						binFiles << qMakePair(*iter, true);
					}
				}
				const QString binary = encInfo.getBinaryPath(m_sysinfo, archIdx, varntIdx);
				if (!filesSet.contains(binary))
				{
					filesSet << binary;
					binFiles << qMakePair(binary, false);
				}
			}
		}
	}
	for(int i = 0; i < 2; i++)
	{
		binFiles << qMakePair(SourceFactory::getSourceInfo(SourceFactory::SourceType_AVS).getBinaryPath(m_sysinfo, bool(i)), false);
		binFiles << qMakePair(AVS_CHECK_BINARY(m_sysinfo, bool(i)), false);
	}
	for(size_t i = 0; UpdaterDialog::BINARIES[i].name; i++)
	{
		if(UpdaterDialog::BINARIES[i].exec)
		{
			binFiles << qMakePair(QString("%1/toolset/common/%2").arg(m_sysinfo->getAppPath(), QString::fromLatin1(UpdaterDialog::BINARIES[i].name)), false);
		}
	}

	//Actually validate the binaries
	size_t currentFile = 0;
	for(QList<FileEntry>::ConstIterator iter = binFiles.constBegin(); iter != binFiles.constEnd(); iter++)
	{
		QScopedPointer<QFile> file(new QFile(iter->first));
		qDebug("%s", MUTILS_UTF8(file->fileName()));

		if(file->open(QIODevice::ReadOnly))
		{
			if(!iter->second)
			{
				if (!MUtils::OS::is_executable_file(file->fileName()))
				{
					m_failedPath = file->fileName();
					qWarning("Required tool does NOT look like a valid Win32/Win64 binary:\n%s\n", MUTILS_UTF8(file->fileName()));
					return 0;
				}
			}
			else
			{
				if (!MUtils::OS::is_library_file(file->fileName()))
				{
					m_failedPath = file->fileName();
					qWarning("Required tool does NOT look like a valid Win32/Win64 library:\n%s\n", MUTILS_UTF8(file->fileName()));
					return 0;
				}
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
			m_failedPath = file->fileName();
			qWarning("Required tool could not be found or access denied:\n%s\n", MUTILS_UTF8(file->fileName()));
			return 0;
		}
	}

	return 1;
}
