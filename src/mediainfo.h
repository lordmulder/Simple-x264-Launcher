///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
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

class QString;

class MediaInfo
{
public:
	typedef enum
	{
		FILETYPE_UNKNOWN      = 0,
		FILETYPE_YUV4MPEG2    = 1,
		FILETYPE_AVISYNTH     = 2,
		FILETYPE_VAPOURSYNTH  = 3
	}
	fileType_t;

	static int analyze(const QString &fileName);

private:
	MediaInfo(void)  {/*NOP*/}
	~MediaInfo(void) {/*NOP*/}

	static bool isYuv4Mpeg(const QString &fileName);
};
