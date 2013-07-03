///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2013 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "model_recently.h"

#include "global.h"

#include <QDesktopServices>
#include <QDir>
#include <QSettings>

#define ARRAY_SIZE(ARRAY) (sizeof((ARRAY))/sizeof((ARRAY[0])))
#define VALID_DIR(PATH) ((!(PATH).isEmpty()) && QFileInfo(PATH).exists() && QFileInfo(PATH).isDir())

static const char *KEY_FILTER_IDX = "path/filterIndex";
static const char *KEY_SOURCE_DIR = "path/directory_openFrom";
static const char *KEY_OUTPUT_DIR = "path/directory_saveTo";

RecentlyUsed::RecentlyUsed(void)
{
	initRecentlyUsed(this);
}

void RecentlyUsed::initRecentlyUsed(RecentlyUsed *recentlyUsed)
{
	recentlyUsed->m_sourceDirectory = QDir::fromNativeSeparators(QDesktopServices::storageLocation(QDesktopServices::MoviesLocation));
	recentlyUsed->m_outputDirectory = QDir::fromNativeSeparators(QDesktopServices::storageLocation(QDesktopServices::MoviesLocation));
	recentlyUsed->m_filterIndex = 0;
}

void RecentlyUsed::loadRecentlyUsed(RecentlyUsed *recentlyUsed)
{
	RecentlyUsed defaults;
	
	QSettings settings(QString("%1/last.ini").arg(x264_data_path()), QSettings::IniFormat);
	recentlyUsed->m_sourceDirectory = settings.value(KEY_SOURCE_DIR, defaults.m_sourceDirectory).toString();
	recentlyUsed->m_outputDirectory = settings.value(KEY_OUTPUT_DIR, defaults.m_outputDirectory).toString();
	recentlyUsed->m_filterIndex = settings.value(KEY_FILTER_IDX, defaults.m_filterIndex).toInt();

	if(!VALID_DIR(recentlyUsed->m_sourceDirectory)) recentlyUsed->m_sourceDirectory = defaults.m_sourceDirectory;
	if(!VALID_DIR(recentlyUsed->m_outputDirectory)) recentlyUsed->m_outputDirectory = defaults.m_outputDirectory;
	recentlyUsed->m_filterIndex = qBound(0, recentlyUsed->m_filterIndex, int(ARRAY_SIZE(X264_FILE_TYPE_FILTERS)-1));
}

void RecentlyUsed::saveRecentlyUsed(RecentlyUsed *recentlyUsed)
{
	QSettings settings(QString("%1/last.ini").arg(x264_data_path()), QSettings::IniFormat);
	if(settings.isWritable())
	{
		settings.setValue(KEY_SOURCE_DIR, recentlyUsed->m_sourceDirectory);
		settings.setValue(KEY_OUTPUT_DIR, recentlyUsed->m_outputDirectory);
		settings.setValue(KEY_FILTER_IDX, recentlyUsed->m_filterIndex);
		settings.sync();
	}
}
