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

#pragma once

#include <QString>

static const struct
{
	const char *pcExt;
	const char *pcStr;
}
X264_FILE_TYPE_FILTERS[] =
{
	{ "mkv", "Matroska Files" },
	{ "mp4", "MPEG-4 Part 14 Container" },
	{ "264", "H.264 Elementary Stream"},
};

class RecentlyUsed
{
public:
	RecentlyUsed(void);
	~RecentlyUsed(void);

	static void initRecentlyUsed(RecentlyUsed *recentlyUsed);
	static void loadRecentlyUsed(RecentlyUsed *recentlyUsed);
	static void saveRecentlyUsed(RecentlyUsed *recentlyUsed);

	//Getter
	QString sourceDirectory(void) { return m_sourceDirectory; }
	QString outputDirectory(void) { return m_outputDirectory; }
	int filterIndex(void)         { return m_filterIndex; }
	int lastUpdateCheck(void)     { return m_lastUpdateCheck; }

	//Setter
	void setSourceDirectory(const QString &sourceDirectory) { m_sourceDirectory = sourceDirectory; }
	void setOutputDirectory(const QString &outputDirectory) { m_outputDirectory = outputDirectory; }
	void setFilterIndex(const int filterIndex)              { m_filterIndex = filterIndex; }
	void setLastUpdateCheck(const int updateCheck)          { m_lastUpdateCheck = updateCheck; }

protected:
	QString m_sourceDirectory;
	QString m_outputDirectory;
	int m_filterIndex;
	int m_lastUpdateCheck;
};
