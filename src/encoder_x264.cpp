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

#include "encoder_x264.h"

#include "model_options.h"

#include <QStringList>
#include <QRegExp>

//x264 version info
static const unsigned int X264_VERSION_X264_MINIMUM_REV = 2380;
static const unsigned int X264_VERSION_X264_CURRENT_API = 142;

X264Encoder::X264Encoder(const QUuid *jobId, JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, volatile bool *abort)
:
	AbstractEncoder(jobId, jobObject, options, sysinfo, preferences, abort)
{
	if(options->encType() != OptionsModel::EncType_X264)
	{
		throw "Invalid encoder type!";
	}

}

X264Encoder::~X264Encoder(void)
{
	/*Nothing to do here*/
}

void X264Encoder::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegExp("\\bx264\\s(\\d)\\.(\\d+)\\.(\\d+)\\s([a-f0-9]{7})", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bx264 (\\d)\\.(\\d+)\\.(\\d+)", Qt::CaseInsensitive);
}

void X264Encoder::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &coreVers, unsigned int &revision, bool &modified)
{
	int offset = -1;
	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		unsigned int temp1 = patterns[0]->cap(2).toUInt(&ok1);
		unsigned int temp2 = patterns[0]->cap(3).toUInt(&ok2);
		if(ok1) coreVers = temp1;
		if(ok2) revision = temp2;
	}
	else if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		unsigned int temp1 = patterns[1]->cap(2).toUInt(&ok1);
		unsigned int temp2 = patterns[1]->cap(3).toUInt(&ok2);
		if(ok1) coreVers = temp1;
		if(ok2) revision = temp2;
		modified = true;
	}
}

void X264Encoder::printVersion(const unsigned int &revision, const bool &modified)
{
	log(tr("\nx264 revision: %1 (core #%2)").arg(QString::number(revision % REV_MULT), QString::number(revision / REV_MULT)).append(modified ? tr(" - with custom patches!") : QString()));
}

bool X264Encoder::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	if((revision % REV_MULT) < X264_VERSION_X264_MINIMUM_REV)
	{
		log(tr("\nERROR: Your revision of x264 is too old! (Minimum required revision is %1)").arg(QString::number(X264_VERSION_X264_MINIMUM_REV)));
		return false;
	}
	
	if((revision / REV_MULT) != X264_VERSION_X264_CURRENT_API)
	{
		log(tr("\nWARNING: Your revision of x264 uses an unsupported core (API) version, take care!"));
		log(tr("This application works best with x264 core (API) version %2.").arg(QString::number(X264_VERSION_X264_CURRENT_API)));
	}

	return true;
}
