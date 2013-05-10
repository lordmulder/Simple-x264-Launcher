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

#include <QDir>

class OptionsModel;

class AddJobDialog : public QDialog, private Ui::AddJobDialog
{
	Q_OBJECT

public:
	typedef struct
	{
		QString sourceDirectory;
		QString outputDirectory;
		int filterIndex;
	}
	RecentlyUsed;

	AddJobDialog(QWidget *parent, OptionsModel *options, RecentlyUsed *recentlyUsed, bool x64supported, bool use10BitEncoding, bool saveToSourceFolder);
	~AddJobDialog(void);

	QString sourceFile(void);
	QString outputFile(void);
	QString preset(void) { return cbxPreset->itemText(cbxPreset->currentIndex()); }
	QString tuning(void) { return cbxTuning->itemText(cbxTuning->currentIndex()); }
	QString profile(void) { return cbxProfile->itemText(cbxProfile->currentIndex()); }
	QString params(void) { return editCustomX264Params->text().simplified(); }
	bool runImmediately(void) { return checkBoxRun->isChecked(); }
	bool applyToAll(void) { return checkBoxApplyToAll->isChecked(); }
	void setRunImmediately(bool run) { checkBoxRun->setChecked(run); }
	void setSourceFile(const QString &path) { editSource->setText(QDir::toNativeSeparators(path)); }
	void setOutputFile(const QString &path) { editOutput->setText(QDir::toNativeSeparators(path)); }
	void setSourceEditable(const bool editable) { buttonBrowseSource->setEnabled(editable); }
	void setApplyToAllVisible(const bool visible) { checkBoxApplyToAll->setVisible(visible); }

	static void initRecentlyUsed(RecentlyUsed *recentlyUsed);
	static void loadRecentlyUsed(RecentlyUsed *recentlyUsed);
	static void saveRecentlyUsed(RecentlyUsed *recentlyUsed);
	
	static QString generateOutputFileName(const QString &sourceFilePath, const QString &destinationDirectory, const int filterIndex, const bool saveToSourceDir);
	static int getFilterIdx(const QString &fileExt);
	static QString getFilterExt(const int filterIndex);
	static QString AddJobDialog::getFilterStr(const int filterIndex);
	static QString getFilterLst(void);
	static QString getInputFilterLst(void);

protected:
	OptionsModel *m_options;
	OptionsModel *m_defaults;
	RecentlyUsed *m_recentlyUsed;

	const bool m_x64supported;
	const bool m_use10BitEncoding;
	const bool m_saveToSourceFolder;

	virtual void showEvent(QShowEvent *event);
	virtual bool eventFilter(QObject *o, QEvent *e);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);

private slots:
	void modeIndexChanged(int index);
	void browseButtonClicked(void);
	void configurationChanged(void);
	void templateSelected(void);
	void saveTemplateButtonClicked(void);
	void deleteTemplateButtonClicked(void);
	void editorActionTriggered(void);
	void copyActionTriggered(void);
	void pasteActionTriggered(void);
	
	virtual void accept(void);

private:
	void loadTemplateList(void);
	void restoreOptions(OptionsModel *options);
	void saveOptions(OptionsModel *options);
	void updateComboBox(QComboBox *cbox, const QString &text);

	QString currentSourcePath(const bool bWithName = false);
	QString currentOutputPath(const bool bWithName = false);
	int currentOutputIndx(void);
};
