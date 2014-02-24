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

#include "encoder_abstract.h"

#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "binaries.h"

#include <QProcess>

AbstractEncoder::AbstractEncoder(const QUuid *jobId, JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, volatile bool *abort)
:
	AbstractTool(jobId, jobObject, options, sysinfo, preferences, abort)
{
	/*Nothing to do here*/
}

AbstractEncoder::~AbstractEncoder(void)
{
	/*Nothing to do here*/
}

unsigned int AbstractEncoder::checkVersion(bool &modified)
{
	if(m_preferences->getSkipVersionTest())
	{
		log("Warning: Skipping encoder version check this time!");
		return (999 * REV_MULT) + (REV_MULT-1);
	}

	QProcess process;
	QList<QRegExp*> patterns;
	QStringList cmdLine;

	//Init encoder-specific values
	checkVersion_init(patterns, cmdLine);

	log("Creating process:");
	if(!startProcess(process, ENC_BINARY(m_sysinfo, m_options), cmdLine))
	{
		return false;
	}

	bool bTimeout = false;
	bool bAborted = false;

	unsigned int revision = UINT_MAX;
	unsigned int coreVers = UINT_MAX;
	modified = false;

	while(process.state() != QProcess::NotRunning)
	{
		if(m_abort)
		{
			process.kill();
			bAborted = true;
			break;
		}
		if(!process.waitForReadyRead())
		{
			if(process.state() == QProcess::Running)
			{
				process.kill();
				qWarning("encoder process timed out <-- killing!");
				log("\nPROCESS TIMEOUT !!!");
				bTimeout = true;
				break;
			}
		}
		while(process.bytesAvailable() > 0)
		{
			QList<QByteArray> lines = process.readLine().split('\r');
			while(!lines.isEmpty())
			{
				const QString text = QString::fromUtf8(lines.takeFirst().constData()).simplified();
				checkVersion_parseLine(text, patterns, coreVers, revision, modified);
				if(!text.isEmpty())
				{
					log(text);
				}
			}
		}
	}

	process.waitForFinished();
	if(process.state() != QProcess::NotRunning)
	{
		process.kill();
		process.waitForFinished(-1);
	}

	while(!patterns.isEmpty())
	{
		QRegExp *pattern = patterns.takeFirst();
		X264_DELETE(pattern);
	}

	if(bTimeout || bAborted || process.exitCode() != EXIT_SUCCESS)
	{
		if(!(bTimeout || bAborted))
		{
			log(tr("\nPROCESS EXITED WITH ERROR CODE: %1").arg(QString::number(process.exitCode())));
		}
		return UINT_MAX;
	}

	if((revision == UINT_MAX) || (coreVers == UINT_MAX))
	{
		log(tr("\nFAILED TO DETERMINE ENCODER VERSION !!!"));
		return UINT_MAX;
	}
	
	return (coreVers * REV_MULT) + (revision % REV_MULT);
}
