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

#include "encoder_x265.h"

#include "model_options.h"

#include <QStringList>
#include <QRegExp>

//x265 version info
static const unsigned int X265_VERSION_X264_MINIMUM_VER = 7;
static const unsigned int X265_VERSION_X264_MINIMUM_REV = 167;

X265Encoder::X265Encoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile)
:
	AbstractEncoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile)
{
	if(options->encType() != OptionsModel::EncType_X265)
	{
		throw "Invalid encoder type!";
	}
}

X265Encoder::~X265Encoder(void)
{
	/*Nothing to do here*/
}

void X265Encoder::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegExp("\\bHEVC\\s+encoder\\s+version\\s+0\\.(\\d+)\\+(\\d+)-[a-f0-9]+\\b", Qt::CaseInsensitive);
}

void X265Encoder::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &coreVers, unsigned int &revision, bool &modified)
{
	int offset = -1;
	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		unsigned int temp1 = patterns[0]->cap(1).toUInt(&ok1);
		unsigned int temp2 = patterns[0]->cap(2).toUInt(&ok2);
		if(ok1) coreVers = temp1;
		if(ok2) revision = temp2;
	}
}

void X265Encoder::printVersion(const unsigned int &revision, const bool &modified)
{
	log(tr("\nx265 version: 0.%1+%2").arg(QString::number(revision / REV_MULT), QString::number(revision % REV_MULT)));
}

bool X265Encoder::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	const unsigned int ver = (revision / REV_MULT);
	const unsigned int rev = (revision % REV_MULT);

	if((ver < X265_VERSION_X264_MINIMUM_VER) || (rev < X265_VERSION_X264_MINIMUM_REV))
	{
		log(tr("\nERROR: Your version of x265 is too old! (Minimum required revision is 0.%1+%2)").arg(QString::number(X265_VERSION_X264_MINIMUM_VER), QString::number(X265_VERSION_X264_MINIMUM_REV)));
		return false;
	}
	
	return true;
}
