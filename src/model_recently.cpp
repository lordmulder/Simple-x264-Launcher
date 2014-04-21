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

#include "model_recently.h"

#include "global.h"

#include <QDesktopServices>
#include <QDir>
#include <QSettings>
#include <QDate>

#define ARRAY_SIZE(ARRAY) (sizeof((ARRAY))/sizeof((ARRAY[0])))
#define VALID_DIR(PATH) ((!(PATH).isEmpty()) && QFileInfo(PATH).exists() && QFileInfo(PATH).isDir())

static const char *KEY_FILTER_IDX = "path/filterIndex";
static const char *KEY_SOURCE_DIR = "path/directory_openFrom";
static const char *KEY_OUTPUT_DIR = "path/directory_saveTo";
static const char *KEY_UPDATE_CHK = "auto_update/last_successfull_check";

static void READ_INT(QSettings &settings, const QString &key, int default, int *value)
{
	bool ok = false;
	const int temp = settings.value(key, default).toInt(&ok);
	*value = (ok) ? temp : default;
}

static QString moviesLocation(void)
{
	static const QDesktopServices::StandardLocation locations[3] =
	{
		QDesktopServices::MoviesLocation, QDesktopServices::DesktopLocation, QDesktopServices::HomeLocation
	};

	for(size_t i = 0; i < 3; i++)
	{
		const QString path = QDir::fromNativeSeparators(QDesktopServices::storageLocation(locations[i]));
		if(!path.isEmpty())
		{
			QDir directory(path);
			if(!directory.exists())
			{
				if(!directory.mkpath("."))
				{
					continue;
				}
			}
			return directory.absolutePath();
		}
	}

	return QDir::rootPath();
}

RecentlyUsed::RecentlyUsed(void)
{
	initRecentlyUsed(this);
}

RecentlyUsed::~RecentlyUsed(void)
{
	/*nothing to do*/
}

void RecentlyUsed::initRecentlyUsed(RecentlyUsed *recentlyUsed)
{
	recentlyUsed->m_sourceDirectory = moviesLocation();
	recentlyUsed->m_outputDirectory = moviesLocation();
	recentlyUsed->m_filterIndex = 0;
	recentlyUsed->m_lastUpdateCheck = QDate(1969, 8, 15).toJulianDay();
}

void RecentlyUsed::loadRecentlyUsed(RecentlyUsed *recentlyUsed)
{
	RecentlyUsed defaults;
	QSettings settings(QString("%1/last.ini").arg(x264_data_path()), QSettings::IniFormat);
	int temp = 0;

	recentlyUsed->m_sourceDirectory = settings.value(KEY_SOURCE_DIR, defaults.m_sourceDirectory).toString();
	recentlyUsed->m_outputDirectory = settings.value(KEY_OUTPUT_DIR, defaults.m_outputDirectory).toString();
	READ_INT(settings, KEY_FILTER_IDX, defaults.m_filterIndex,     &recentlyUsed->m_filterIndex);
	READ_INT(settings, KEY_UPDATE_CHK, defaults.m_lastUpdateCheck, &recentlyUsed->m_lastUpdateCheck);

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
		settings.setValue(KEY_UPDATE_CHK, recentlyUsed->m_lastUpdateCheck);
		settings.sync();
	}
	else
	{
		qWarning("Settings are not writable!");
	}
}
