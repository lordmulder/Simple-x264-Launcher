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
#include <QMessageBox>

#define UPDATE_CHECKBOX(CHKBOX, VALUE) \
{ \
	if((CHKBOX)->isChecked() != (VALUE)) (CHKBOX)->click(); \
	if((CHKBOX)->isChecked() != (VALUE)) (CHKBOX)->setChecked(VALUE); \
}

PreferencesDialog::PreferencesDialog(QWidget *parent, Preferences *preferences, bool x64)
:
	QDialog(parent),
	m_x64(x64)
{
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
	setFixedSize(minimumSize());

	labelRunNextJob->installEventFilter(this);
	labelUse10BitEncoding->installEventFilter(this);
	labelUse64BitAvs2YUV->installEventFilter(this);
	labelShutdownComputer->installEventFilter(this);
	labelSaveLogFiles->installEventFilter(this);

	connect(resetButton, SIGNAL(clicked()), this, SLOT(resetButtonPressed()));
	connect(checkUse10BitEncoding, SIGNAL(toggled(bool)), this, SLOT(use10BitEncodingToggled(bool)));

	m_preferences = preferences;
}

PreferencesDialog::~PreferencesDialog(void)
{
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
	if(event) QDialog::showEvent(event);
	
	UPDATE_CHECKBOX(checkRunNextJob, m_preferences->autoRunNextJob);
	UPDATE_CHECKBOX(checkShutdownComputer, m_preferences->shutdownComputer);
	UPDATE_CHECKBOX(checkUse64BitAvs2YUV, m_preferences->useAvisyth64Bit);
	UPDATE_CHECKBOX(checkSaveLogFiles, m_preferences->saveLogFiles);

	checkUse10BitEncoding->blockSignals(true);
	UPDATE_CHECKBOX(checkUse10BitEncoding, m_preferences->use10BitEncoding);
	checkUse10BitEncoding->blockSignals(false);

	spinBoxJobCount->setValue(m_preferences->maxRunningJobCount);
	
	checkUse64BitAvs2YUV->setEnabled(m_x64);
	labelUse64BitAvs2YUV->setEnabled(m_x64);
}

bool PreferencesDialog::eventFilter(QObject *o, QEvent *e)
{
	emulateMouseEvent(o, e, labelRunNextJob, checkRunNextJob);
	emulateMouseEvent(o, e, labelShutdownComputer, checkShutdownComputer);
	emulateMouseEvent(o, e, labelUse10BitEncoding, checkUse10BitEncoding);
	emulateMouseEvent(o, e, labelUse64BitAvs2YUV, checkUse64BitAvs2YUV);
	emulateMouseEvent(o, e, labelSaveLogFiles, checkSaveLogFiles);
	return false;
}

void PreferencesDialog::emulateMouseEvent(QObject *object, QEvent *event, QWidget *source, QWidget *target)
{
	if(object == source)
	{
		if((event->type() == QEvent::MouseButtonPress) || (event->type() == QEvent::MouseButtonRelease))
		{
			if(QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event))
			{
				qApp->postEvent(target, new QMouseEvent
				(
					event->type(),
					qApp->widgetAt(mouseEvent->globalPos()) == source ? QPoint(1, 1) : QPoint(INT_MAX, INT_MAX),
					Qt::LeftButton,
					0, 0
				));
			}
		}
	}
}

void PreferencesDialog::done(int n)
{
	m_preferences->autoRunNextJob = checkRunNextJob->isChecked();
	m_preferences->shutdownComputer = checkShutdownComputer->isChecked();
	m_preferences->use10BitEncoding = checkUse10BitEncoding->isChecked();
	m_preferences->useAvisyth64Bit = checkUse64BitAvs2YUV->isChecked();
	m_preferences->saveLogFiles = checkSaveLogFiles->isChecked();
	m_preferences->maxRunningJobCount = spinBoxJobCount->value();

	savePreferences(m_preferences);
	QDialog::done(n);
}

void PreferencesDialog::resetButtonPressed(void)
{
	initPreferences(m_preferences);
	showEvent(NULL);
}

void PreferencesDialog::use10BitEncodingToggled(bool checked)
{
	if(checked)
	{
		QString text;
		text += tr("<nobr>Please note that 10&minus;Bit H.264 streams are <b>not</b> currently supported by hardware (standalone) players!</nobr><br>");
		text += tr("<nobr>To play such streams, you will need an <i>up&minus;to&minus;date</i> ffdshow&minus;tryouts, CoreAVC 3.x or another supported s/w decoder.</nobr><br>");
		text += tr("<nobr>Also be aware that hardware&minus;acceleration (CUDA, DXVA, etc) usually will <b>not</b> work with 10&minus;Bit H.264 streams.</nobr><br>");
		
		if(QMessageBox::warning(this, tr("10-Bit Encoding"), text, tr("Continue"), tr("Revert"), QString(), 1) != 0)
		{
			UPDATE_CHECKBOX(checkUse10BitEncoding, false);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Static Functions
///////////////////////////////////////////////////////////////////////////////

void PreferencesDialog::initPreferences(Preferences *preferences)
{
	memset(preferences, 0, sizeof(Preferences));

	preferences->autoRunNextJob = true;
	preferences->maxRunningJobCount = 1;
	preferences->shutdownComputer = false;
	preferences->use10BitEncoding = false;
	preferences->useAvisyth64Bit = false;
	preferences->saveLogFiles = false;
}

void PreferencesDialog::loadPreferences(Preferences *preferences)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	Preferences defaults;
	initPreferences(&defaults);

	settings.beginGroup("preferences");
	preferences->autoRunNextJob = settings.value("auto_run_next_job", QVariant(defaults.autoRunNextJob)).toBool();
	preferences->maxRunningJobCount = qBound(1U, settings.value("max_running_job_count", QVariant(defaults.maxRunningJobCount)).toUInt(), 16U);
	preferences->shutdownComputer = settings.value("shutdown_computer_on_completion", QVariant(defaults.shutdownComputer)).toBool();
	preferences->use10BitEncoding = settings.value("use_10bit_encoding", QVariant(defaults.use10BitEncoding)).toBool();
	preferences->useAvisyth64Bit = settings.value("use_64bit_avisynth", QVariant(defaults.useAvisyth64Bit)).toBool();
	preferences->saveLogFiles = settings.value("save_log_files", QVariant(defaults.saveLogFiles)).toBool();
}

void PreferencesDialog::savePreferences(Preferences *preferences)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);

	settings.beginGroup("preferences");
	settings.setValue("auto_run_next_job", preferences->autoRunNextJob);
	settings.setValue("shutdown_computer_on_completion", preferences->shutdownComputer);
	settings.setValue("max_running_job_count", preferences->maxRunningJobCount);
	settings.setValue("use_10bit_encoding", preferences->use10BitEncoding);
	settings.setValue("use_64bit_avisynth", preferences->useAvisyth64Bit);
	settings.setValue("save_log_files", preferences->saveLogFiles);
	settings.sync();
}

