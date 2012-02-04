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
#include <QMouseEvent>

PreferencesDialog::PreferencesDialog(QWidget *parent, Preferences *preferences)
:
	QDialog(parent)
{
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
	setFixedSize(minimumSize());

	labelRunNextJob->installEventFilter(this);
	labelShutdownComputer->installEventFilter(this);

	m_preferences = preferences;
}

PreferencesDialog::~PreferencesDialog(void)
{
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	
	while(checkRunNextJob->isChecked() != m_preferences->autoRunNextJob)
	{
		checkRunNextJob->click();
	}
	while(checkShutdownComputer->isChecked() != m_preferences->shutdownComputer)
	{
		checkShutdownComputer->click();
	}
	spinBoxJobCount->setValue(m_preferences->maxRunningJobCount);
}

bool PreferencesDialog::eventFilter(QObject *o, QEvent *e)
{
	if(o == labelRunNextJob && e->type() == QEvent::MouseButtonRelease)
	{
		QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(e);
		if(mouseEvent)
		{
			if(qApp->widgetAt(mouseEvent->globalPos()) == labelRunNextJob)
			{
				checkRunNextJob->click();
			}
		}
	}
	if(o == labelShutdownComputer && e->type() == QEvent::MouseButtonRelease)
	{
		QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(e);
		if(mouseEvent)
		{
			if(qApp->widgetAt(mouseEvent->globalPos()) == labelShutdownComputer)
			{
				checkShutdownComputer->click();
			}
		}
	}
	return false;
}

void PreferencesDialog::accept(void)
{
	m_preferences->autoRunNextJob = checkRunNextJob->isChecked();
	m_preferences->shutdownComputer = checkShutdownComputer->isChecked();
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
	preferences->maxRunningJobCount = qBound(1U, settings.value("max_running_job_count", QVariant(1U)).toUInt(), 16U);
	preferences->shutdownComputer = settings.value("shutdown_computer_on_completion", QVariant(false)).toBool();
}

void PreferencesDialog::savePreferences(Preferences *preferences)
{
	const QString appDir = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	settings.beginGroup("preferences");
	settings.setValue("auto_run_next_job", preferences->autoRunNextJob);
	settings.setValue("shutdown_computer_on_completion", preferences->shutdownComputer);
	settings.setValue("max_running_job_count", preferences->maxRunningJobCount);
	settings.sync();
}

