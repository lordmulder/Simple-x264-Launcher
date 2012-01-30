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

#pragma once

#include "uic_win_addJob.h"

class OptionsModel;

class AddJobDialog : public QDialog, private Ui::AddJobDialog
{
	Q_OBJECT

public:
	AddJobDialog(QWidget *parent, OptionsModel *options);
	~AddJobDialog(void);

	QString sourceFile(void);
	QString outputFile(void);
	QString preset(void) { return cbxPreset->itemText(cbxPreset->currentIndex()); }
	QString tuning(void) { return cbxTuning->itemText(cbxTuning->currentIndex()); }
	QString profile(void) { return cbxProfile->itemText(cbxProfile->currentIndex()); }
	QString params(void) { return cbxCustomParams->currentText().simplified(); }
	bool runImmediately(void) { return checkBoxRun->isChecked(); }
	void setRunImmediately(bool run) { checkBoxRun->setChecked(run); }

protected:
	OptionsModel *m_options;
	
	virtual void showEvent(QShowEvent *event);
	virtual bool eventFilter(QObject *o, QEvent *e);

private slots:
	void modeIndexChanged(int index);
	void browseButtonClicked(void);
	
	virtual void accept(void);

private:
	void restoreOptions(OptionsModel *options);
	void saveOptions(OptionsModel *options);
	void updateComboBox(QComboBox *cbox, const QString &text);
	QString makeFileFilter(void);
};
