///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2012 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "win_preferences.h"

#include "global.h"

#include <QSettings>
#include <QDesktopServices>

PreferencesDialog::PreferencesDialog(QWidget *parent, Preferences *preferences)
:
	QDialog(parent)
{
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
	setFixedSize(size());

	HMENU hMenu = GetSystemMenu((HWND) winId(), FALSE);
	EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

	m_preferences = preferences;
}

PreferencesDialog::~PreferencesDialog(void)
{
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
	while(checkRunNextJob->isChecked() != m_preferences->autoRunNextJob)
	{
		checkRunNextJob->click();
	}

	spinBoxJobCount->setValue(m_preferences->maxRunningJobCount);
}

void PreferencesDialog::accept(void)
{
	m_preferences->autoRunNextJob = checkRunNextJob->isChecked();
	m_preferences->maxRunningJobCount = spinBoxJobCount->value();

	savePreferences(m_preferences);
	QDialog::accept();
}

void PreferencesDialog::loadPreferences(Preferences *preferences)
{
	const QString appDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	settings.beginGroup("preferences");
	preferences->autoRunNextJob = settings.value("auto_run_next_job", QVariant(true)).toBool();
	preferences->maxRunningJobCount = settings.value("max_running_job_count", QVariant(1U)).toUInt();
}

void PreferencesDialog::savePreferences(Preferences *preferences)
{
	const QString appDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	settings.beginGroup("preferences");
	settings.setValue("auto_run_next_job", preferences->autoRunNextJob);
	settings.setValue("max_running_job_count", preferences->maxRunningJobCount);
	settings.sync();
}

