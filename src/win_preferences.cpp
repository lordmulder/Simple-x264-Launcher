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

#include "win_preferences.h"

#include "global.h"
#include "model_preferences.h"

#include <QSettings>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QMessageBox>

#define UPDATE_CHECKBOX(CHKBOX, VALUE, BLOCK) \
{ \
	if((BLOCK)) { (CHKBOX)->blockSignals(true); } \
	if((CHKBOX)->isChecked() != (VALUE)) (CHKBOX)->click(); \
	if((CHKBOX)->isChecked() != (VALUE)) (CHKBOX)->setChecked(VALUE); \
	if((BLOCK)) { (CHKBOX)->blockSignals(false); } \
}

PreferencesDialog::PreferencesDialog(QWidget *parent, PreferencesModel *preferences, bool x64)
:
	QDialog(parent),
	m_x64(x64)
{
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
	setFixedSize(minimumSize());

	if(WId hWindow = this->winId())
	{
		HMENU hMenu = GetSystemMenu(hWindow, 0);
		EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
	}
	
	labelRunNextJob->installEventFilter(this);
	labelUse10BitEncoding->installEventFilter(this);
	labelUse64BitAvs2YUV->installEventFilter(this);
	labelShutdownComputer->installEventFilter(this);
	labelSaveLogFiles->installEventFilter(this);
	labelSaveToSourceFolder->installEventFilter(this);
	labelEnableSounds->installEventFilter(this);
	labelDisableWarnings->installEventFilter(this);

	connect(resetButton, SIGNAL(clicked()), this, SLOT(resetButtonPressed()));
	connect(checkUse10BitEncoding, SIGNAL(toggled(bool)), this, SLOT(use10BitEncodingToggled(bool)));
	connect(checkDisableWarnings, SIGNAL(toggled(bool)), this, SLOT(disableWarningsToggled(bool)));
	
	m_preferences = preferences;
}

PreferencesDialog::~PreferencesDialog(void)
{
}

void PreferencesDialog::showEvent(QShowEvent *event)
{
	if(event) QDialog::showEvent(event);
	
	UPDATE_CHECKBOX(checkRunNextJob, m_preferences->autoRunNextJob(), false);
	UPDATE_CHECKBOX(checkShutdownComputer, m_preferences->shutdownComputer(), false);
	UPDATE_CHECKBOX(checkUse64BitAvs2YUV, m_preferences->useAvisyth64Bit(), false);
	UPDATE_CHECKBOX(checkSaveLogFiles, m_preferences->saveLogFiles(), false);
	UPDATE_CHECKBOX(checkSaveToSourceFolder, m_preferences->saveToSourcePath(), false);
	UPDATE_CHECKBOX(checkEnableSounds, m_preferences->enableSounds(), false);
	UPDATE_CHECKBOX(checkDisableWarnings, m_preferences->disableWarnings(), true);
	UPDATE_CHECKBOX(checkUse10BitEncoding, m_preferences->use10BitEncoding(), true);

	spinBoxJobCount->setValue(m_preferences->maxRunningJobCount());
	comboBoxPriority->setCurrentIndex(qBound(0, m_preferences->processPriority(), comboBoxPriority->count()-1));

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
	emulateMouseEvent(o, e, labelSaveToSourceFolder, checkSaveToSourceFolder);
	emulateMouseEvent(o, e, labelEnableSounds, checkEnableSounds);
	emulateMouseEvent(o, e, labelDisableWarnings, checkDisableWarnings);
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
	m_preferences->setAutoRunNextJob(checkRunNextJob->isChecked());
	m_preferences->setShutdownComputer(checkShutdownComputer->isChecked());
	m_preferences->setUse10BitEncoding(checkUse10BitEncoding->isChecked());
	m_preferences->setUseAvisyth64Bit(checkUse64BitAvs2YUV->isChecked());
	m_preferences->setSaveLogFiles(checkSaveLogFiles->isChecked());
	m_preferences->setSaveToSourcePath(checkSaveToSourceFolder->isChecked());
	m_preferences->setMaxRunningJobCount(spinBoxJobCount->value());
	m_preferences->setProcessPriority(comboBoxPriority->currentIndex());
	m_preferences->setEnableSounds(checkEnableSounds->isChecked());
	m_preferences->setDisableWarnings(checkDisableWarnings->isChecked());

	PreferencesModel::savePreferences(m_preferences);
	QDialog::done(n);
}

void PreferencesDialog::resetButtonPressed(void)
{
	PreferencesModel::initPreferences(m_preferences);
	showEvent(NULL);
}

void PreferencesDialog::use10BitEncodingToggled(bool checked)
{
	if(checked)
	{
		QString text;
		text += QString("<nobr>%1</nobr><br>").arg(tr("Please note that 10&minus;Bit H.264 streams are <b>not</b> currently supported by hardware (standalone) players!"));
		text += QString("<nobr>%1</nobr><br>").arg(tr("To play such streams, you will need an <i>up&minus;to&minus;date</i> ffdshow&minus;tryouts, CoreAVC 3.x or another supported s/w decoder."));
		text += QString("<nobr>%1</nobr><br>").arg(tr("Also be aware that hardware&minus;acceleration (CUDA, DXVA, etc) usually will <b>not</b> work with 10&minus;Bit H.264 streams."));
		
		if(QMessageBox::warning(this, tr("10-Bit Encoding"), text.replace("-", "&minus;"), tr("Continue"), tr("Revert"), QString(), 1) != 0)
		{
			UPDATE_CHECKBOX(checkUse10BitEncoding, false, true);
		}
	}
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
			UPDATE_CHECKBOX(checkDisableWarnings, false, true);
		}
	}
}