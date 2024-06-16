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

#include "win_preferences.h"
#include "UIC_win_preferences.h"

//Internal
#include "global.h"
#include "model_preferences.h"
#include "model_sysinfo.h"

//MUtils
#include <MUtils/GUI.h>

//Qt
#include <QSettings>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QMessageBox>

static inline void UPDATE_CHECKBOX(QCheckBox *const chkbox, const bool value, const bool block = false)
{
	if(block) { chkbox->blockSignals(true); }
	if(chkbox->isChecked() != value) chkbox->click();
	if(chkbox->isChecked() != value) chkbox->setChecked(value);
	if(block) { chkbox->blockSignals(false); }
}

static inline void UPDATE_COMBOBOX(QComboBox *const cobox, const int value, const int defVal)
{
	const int count = cobox->count();
	for(int i = 0; i < count; i++)
	{
		const int current = cobox->itemData(i).toInt();
		if((current == value) || (current == defVal))
		{
			cobox->setCurrentIndex(i);
			if((current == value)) break;
		}
	}
}

PreferencesDialog::PreferencesDialog(QWidget *parent, PreferencesModel *preferences, const SysinfoModel *sysinfo)
:
	QDialog(parent),
	m_sysinfo(sysinfo),
	ui(new Ui::PreferencesDialog())
{
	ui->setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
	setFixedSize(minimumSize());
	MUtils::GUI::enable_close_button(this, false);
	
	ui->comboBoxPriority->setItemData(0, QVariant::fromValue( 1)); //Above Normal
	ui->comboBoxPriority->setItemData(1, QVariant::fromValue( 0)); //Normal
	ui->comboBoxPriority->setItemData(2, QVariant::fromValue(-1)); //Below Normal
	ui->comboBoxPriority->setItemData(3, QVariant::fromValue(-2)); //Idle

	ui->labelRunNextJob        ->installEventFilter(this);
	ui->labelUse64BitAvs2YUV   ->installEventFilter(this);
	ui->labelSaveLogFiles      ->installEventFilter(this);
	ui->labelSaveToSourceFolder->installEventFilter(this);
	ui->labelEnableSounds      ->installEventFilter(this);
	ui->labelDisableWarnings   ->installEventFilter(this);
	ui->labelNoUpdateReminder  ->installEventFilter(this);
	ui->labelSaveQueueNoConfirm->installEventFilter(this);

	ui->checkBoxDummy1->installEventFilter(this);
	ui->checkBoxDummy2->installEventFilter(this);

	connect(ui->resetButton, SIGNAL(clicked()), this, SLOT(resetButtonPressed()));
	connect(ui->checkDisableWarnings, SIGNAL(toggled(bool)), this, SLOT(disableWarningsToggled(bool)));
	
	m_preferences = preferences;
}

PreferencesDialog::~PreferencesDialog(void)
{
	delete ui;
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
	if(event) QDialog::showEvent(event);
	
	UPDATE_CHECKBOX(ui->checkRunNextJob,         m_preferences->getAutoRunNextJob());
	UPDATE_CHECKBOX(ui->checkUse64BitAvs2YUV,    m_preferences->getPrefer64BitSource() && m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64));
	UPDATE_CHECKBOX(ui->checkSaveLogFiles,       m_preferences->getSaveLogFiles());
	UPDATE_CHECKBOX(ui->checkSaveToSourceFolder, m_preferences->getSaveToSourcePath());
	UPDATE_CHECKBOX(ui->checkEnableSounds,       m_preferences->getEnableSounds());
	UPDATE_CHECKBOX(ui->checkNoUpdateReminder,   m_preferences->getNoUpdateReminder());
	UPDATE_CHECKBOX(ui->checkDisableWarnings,    m_preferences->getDisableWarnings(), true);
	UPDATE_CHECKBOX(ui->checkSaveQueueNoConfirm, m_preferences->getSaveQueueNoConfirm());
	
	ui->spinBoxJobCount->setValue(m_preferences->getMaxRunningJobCount());
	UPDATE_COMBOBOX(ui->comboBoxPriority, qBound(-2, m_preferences->getProcessPriority(), 1), 0);
	
	const bool hasX64 = m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64);
	ui->checkUse64BitAvs2YUV->setEnabled(hasX64);
	ui->labelUse64BitAvs2YUV->setEnabled(hasX64);
}

bool PreferencesDialog::eventFilter(QObject *o, QEvent *e)
{
	if(e->type() == QEvent::Paint)
	{
		if(o == ui->checkBoxDummy1) return true;
		if(o == ui->checkBoxDummy2) return true;
	}
	else if((e->type() == QEvent::MouseButtonPress) || (e->type() == QEvent::MouseButtonRelease))
	{
		emulateMouseEvent(o, e, ui->labelRunNextJob,         ui->checkRunNextJob);
		emulateMouseEvent(o, e, ui->labelUse64BitAvs2YUV,    ui->checkUse64BitAvs2YUV);
		emulateMouseEvent(o, e, ui->labelSaveLogFiles,       ui->checkSaveLogFiles);
		emulateMouseEvent(o, e, ui->labelSaveToSourceFolder, ui->checkSaveToSourceFolder);
		emulateMouseEvent(o, e, ui->labelEnableSounds,       ui->checkEnableSounds);
		emulateMouseEvent(o, e, ui->labelDisableWarnings,    ui->checkDisableWarnings);
		emulateMouseEvent(o, e, ui->labelNoUpdateReminder,   ui->checkNoUpdateReminder);
		emulateMouseEvent(o, e, ui->labelSaveQueueNoConfirm, ui->checkSaveQueueNoConfirm);
	}
	return false;
}

void PreferencesDialog::emulateMouseEvent(QObject *object, QEvent *event, QWidget *source, QWidget *target)
{
	if(object == source)
	{
		if(QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event))
		{
			qApp->postEvent(target, new QMouseEvent
			(
				event->type(),
				(qApp->widgetAt(mouseEvent->globalPos()) == source) ? QPoint(1, 1) : QPoint(INT_MAX, INT_MAX),
				Qt::LeftButton,
				0, 0
			));
		}
	}
}

void PreferencesDialog::done(int n)
{
	m_preferences->setAutoRunNextJob    (ui->checkRunNextJob->isChecked());
	m_preferences->setPrefer64BitSource (ui->checkUse64BitAvs2YUV->isChecked());
	m_preferences->setSaveLogFiles      (ui->checkSaveLogFiles->isChecked());
	m_preferences->setSaveToSourcePath  (ui->checkSaveToSourceFolder->isChecked());
	m_preferences->setMaxRunningJobCount(ui->spinBoxJobCount->value());
	m_preferences->setProcessPriority   (ui->comboBoxPriority->itemData(ui->comboBoxPriority->currentIndex()).toInt());
	m_preferences->setEnableSounds      (ui->checkEnableSounds->isChecked());
	m_preferences->setDisableWarnings   (ui->checkDisableWarnings->isChecked());
	m_preferences->setNoUpdateReminder  (ui->checkNoUpdateReminder->isChecked());
	m_preferences->setSaveQueueNoConfirm(ui->checkSaveQueueNoConfirm->isChecked());

	PreferencesModel::savePreferences(m_preferences);
	QDialog::done(n);
}

void PreferencesDialog::resetButtonPressed(void)
{
	PreferencesModel::initPreferences(m_preferences);
	showEvent(NULL);
}

void PreferencesDialog::disableWarningsToggled(bool checked)
{
	if(checked)
	{
		QString text;
		text += QString("<nobr>%1</nobr><br>").arg(tr("Please note that Avisynth and/or VapourSynth support might be unavailable <b>without</b> any notice!"));
		text += QString("<nobr>%1</nobr><br>").arg(tr("Also note that the CLI option <tt>--console</tt> may be used to get more diagnostic infomation."));

		if(QMessageBox::warning(this, tr("Avisynth/VapourSynth Warnings"), text.replace("-", "&minus;"), tr("Continue"), tr("Revert"), QString(), 1) != 0)
		{
			UPDATE_CHECKBOX(ui->checkDisableWarnings, false, true);
		}
	}
}