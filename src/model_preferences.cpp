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

#include "model_preferences.h"

#include "global.h"

#include <cstring>

#include <QSettings>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QMessageBox>

PreferencesModel::PreferencesModel(void)
{
	initPreferences(this);
}

void PreferencesModel::initPreferences(PreferencesModel *preferences)
{
	memset(preferences, 0, sizeof(PreferencesModel));

	preferences->m_autoRunNextJob = true;
	preferences->m_maxRunningJobCount = 1;
	preferences->m_shutdownComputer = false;
	preferences->m_use10BitEncoding = false;
	preferences->m_useAvisyth64Bit = false;
	preferences->m_saveLogFiles = false;
	preferences->m_saveToSourcePath = false;
	preferences->m_processPriority = -1;
	preferences->m_enableSounds = false;
	preferences->m_disableWarnings = false;
	preferences->m_noUpdateReminder = false;
}

void PreferencesModel::loadPreferences(PreferencesModel *preferences)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	PreferencesModel defaults;

	settings.beginGroup("preferences");
	preferences->m_autoRunNextJob = settings.value("auto_run_next_job", QVariant(defaults.m_autoRunNextJob)).toBool();
	preferences->m_maxRunningJobCount = qBound(1U, settings.value("max_running_job_count", QVariant(defaults.m_maxRunningJobCount)).toUInt(), 16U);
	preferences->m_shutdownComputer = settings.value("shutdown_computer_on_completion", QVariant(defaults.m_shutdownComputer)).toBool();
	preferences->m_use10BitEncoding = settings.value("use_10bit_encoding", QVariant(defaults.m_use10BitEncoding)).toBool();
	preferences->m_useAvisyth64Bit = settings.value("use_64bit_avisynth", QVariant(defaults.m_useAvisyth64Bit)).toBool();
	preferences->m_saveLogFiles = settings.value("save_log_files", QVariant(defaults.m_saveLogFiles)).toBool();
	preferences->m_saveToSourcePath = settings.value("save_to_source_path", QVariant(defaults.m_saveToSourcePath)).toBool();
	preferences->m_processPriority = settings.value("process_priority", QVariant(defaults.m_processPriority)).toInt();
	preferences->m_enableSounds = settings.value("enable_sounds", QVariant(defaults.m_enableSounds)).toBool();
	preferences->m_disableWarnings = settings.value("disable_warnings", QVariant(defaults.m_disableWarnings)).toBool();
	preferences->m_noUpdateReminder = settings.value("disable_update_reminder", QVariant(defaults.m_disableWarnings)).toBool();
}

void PreferencesModel::savePreferences(PreferencesModel *preferences)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	settings.beginGroup("preferences");
	settings.setValue("auto_run_next_job", preferences->m_autoRunNextJob);
	settings.setValue("shutdown_computer_on_completion", preferences->m_shutdownComputer);
	settings.setValue("max_running_job_count", preferences->m_maxRunningJobCount);
	settings.setValue("use_10bit_encoding", preferences->m_use10BitEncoding);
	settings.setValue("use_64bit_avisynth", preferences->m_useAvisyth64Bit);
	settings.setValue("save_log_files", preferences->m_saveLogFiles);
	settings.setValue("save_to_source_path", preferences->m_saveToSourcePath);
	settings.setValue("process_priority", preferences->m_processPriority);
	settings.setValue("enable_sounds", preferences->m_enableSounds);
	settings.setValue("disable_warnings", preferences->m_disableWarnings);
	settings.setValue("disable_update_reminder", preferences->m_noUpdateReminder);
	settings.sync();
}
