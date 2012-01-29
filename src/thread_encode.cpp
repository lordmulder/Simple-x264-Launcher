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

#include "thread_encode.h"

#include "global.h"
#include "model_options.h"

#include <QDate>
#include <QTime>

EncodeThread::EncodeThread(const QString &sourceFileName, const QString &outputFileName, const OptionsModel *options)
:
	m_jobId(QUuid::createUuid()),
	m_sourceFileName(sourceFileName),
	m_outputFileName(outputFileName),
	m_options(new OptionsModel(*options))
{
	m_abort = false;
}

EncodeThread::~EncodeThread(void)
{
	X264_DELETE(m_options);
}

///////////////////////////////////////////////////////////////////////////////
// Thread entry point
///////////////////////////////////////////////////////////////////////////////

void EncodeThread::run(void)
{
	try
	{
		encode();
	}
	catch(char *msg)
	{
		emit messageLogged(m_jobId, QString("EXCEPTION ERROR: ").append(QString::fromLatin1(msg)));
	}
	catch(...)
	{
		emit messageLogged(m_jobId, QString("EXCEPTION ERROR !!!"));
	}
}

void EncodeThread::encode(void)
{
	Sleep(1500);

	//Print some basic info
	log(tr("Job started at %1, %2.\n").arg(QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString( Qt::ISODate)));
	log(tr("Source file: %1").arg(m_sourceFileName));
	log(tr("Output file: %1").arg(m_outputFileName));
	log(tr("\n[Encoder Options]"));
	log(tr("RC Mode: %1").arg(OptionsModel::rcMode2String(m_options->rcMode())));
	log(tr("Preset: %1").arg(m_options->preset()));
	log(tr("Tuning: %1").arg(m_options->tune()));
	log(tr("Profile: %1").arg(m_options->profile()));
	log(tr("Custom: %1").arg(m_options->custom().isEmpty() ? tr("(None)") : m_options->custom()));
	log(tr("\n[Input Properties]"));

	for(int i = 0; i <= 100; i += 5)
	{
		emit progressChanged(m_jobId, i);
		emit statusChanged(m_jobId, (i % 2) ? JobStatus_Indexing : JobStatus_Running_Pass1);
		emit messageLogged(m_jobId, QUuid::createUuid().toString());
	
		for(int j = 0; j < 3; j++)
		{
			emit detailsChanged(m_jobId, QUuid::createUuid().toString());
			Sleep(120);
		}

		if(m_abort)
		{
			Sleep(500);
			emit statusChanged(m_jobId, JobStatus_Aborted);
			return;
		}
	}

	Sleep(1500);

	for(int i = 0; i <= 100; i += 5)
	{
		emit progressChanged(m_jobId, i);
		emit statusChanged(m_jobId, (i % 2) ? JobStatus_Indexing : JobStatus_Running_Pass2);
		emit messageLogged(m_jobId, QUuid::createUuid().toString());
	
		for(int j = 0; j < 3; j++)
		{
			emit detailsChanged(m_jobId, QUuid::createUuid().toString());
			Sleep(120);
		}

		if(m_abort)
		{
			Sleep(500);
			emit statusChanged(m_jobId, JobStatus_Aborted);
			return;
		}
	}

	Sleep(250);

	emit statusChanged(m_jobId, JobStatus_Completed);
}
