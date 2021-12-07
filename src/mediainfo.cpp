///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2021 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "mediainfo.h"

#include <QString>
#include <QFile>
#include <QByteArray>
#include <QFileInfo>

static const char *YUV4MPEG2 = "YUV4MPEG2";

int MediaInfo::analyze(const QString &fileName)
{
	const QString suffix = QFileInfo(fileName).suffix();

	//Try to guess Avisynth or VapourSynth from the file extension first!
	if((suffix.compare("avs", Qt::CaseInsensitive) == 0) || (suffix.compare("avsi", Qt::CaseInsensitive) == 0))
	{
		return FILETYPE_AVISYNTH;
	}
	if((suffix.compare("vpy", Qt::CaseInsensitive) == 0) || (suffix.compare("py", Qt::CaseInsensitive) == 0))
	{
		return FILETYPE_VAPOURSYNTH;
	}

	//Check for YUV4MEPG2 format next
	if(isYuv4Mpeg(fileName))
	{
		return FILETYPE_YUV4MPEG2;
	}

	//Unknown file type
	return FILETYPE_UNKNOWN;
}

bool MediaInfo::isYuv4Mpeg(const QString &fileName)
{
	QFile testFile(fileName);

	//Try to open test file
	if(!testFile.open(QIODevice::ReadOnly))
	{
		qWarning("[isYuv4Mpeg] Failed to open input file!");
		return false;
	}

	//Read file header
	const size_t len = strlen(YUV4MPEG2);
	QByteArray header = testFile.read(len);
	testFile.close();

	//Did we read enough header bytes?
	if(len != header.size())
	{
		qWarning("[isYuv4Mpeg] File is too short to be analyzed!");
		return false;
	}

	//Compare YUV4MPEG2 signature
	return (memcmp(header.constData(), YUV4MPEG2, len) == 0);
}
