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

unsigned int AbstractEncoder::checkVersion(bool &modified)
{
	if(m_preferences->getSkipVersionTest())
	{
		log("Warning: Skipping encoder version check this time!");
		return (999 * REV_MULT) + (REV_MULT-1);
	}

	QProcess process;
	QStringList cmdLine = QStringList() << "--version";

	log("Creating process:");
	if(!startProcess(process, ENC_BINARY(m_sysinfo, m_options), cmdLine))
	{
		return false;;
	}

	QRegExp regExpVersion("", Qt::CaseInsensitive);
	QRegExp regExpVersionMod("\\bx264 (\\d)\\.(\\d+)\\.(\\d+)", Qt::CaseInsensitive);
	
	switch(m_options->encType())
	{
		case OptionsModel::EncType_X264: regExpVersion.setPattern("\\bx264\\s(\\d)\\.(\\d+)\\.(\\d+)\\s([a-f0-9]{7})");
		case OptionsModel::EncType_X265: regExpVersion.setPattern("\\bHEVC\\s+encoder\\s+version\\s+0\\.(\\d+)\\+(\\d+)-[a-f0-9]+\\b");
		default: throw "Invalid encoder type!";
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
				QString text = QString::fromUtf8(lines.takeFirst().constData()).simplified();
				int offset = -1;
				if((offset = regExpVersion.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false;
					unsigned int temp1 = regExpVersion.cap(2).toUInt(&ok1);
					unsigned int temp2 = regExpVersion.cap(3).toUInt(&ok2);
					if(ok1) coreVers = temp1;
					if(ok2) revision = temp2;
				}
				else if((offset = regExpVersionMod.lastIndexIn(text)) >= 0)
				{
					bool ok1 = false, ok2 = false;
					unsigned int temp1 = regExpVersionMod.cap(2).toUInt(&ok1);
					unsigned int temp2 = regExpVersionMod.cap(3).toUInt(&ok2);
					if(ok1) coreVers = temp1;
					if(ok2) revision = temp2;
					modified = true;
				}
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
