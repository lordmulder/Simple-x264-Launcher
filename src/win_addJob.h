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

#pragma once

#include <QDialog>

class OptionsModel;
class RecentlyUsed;
class SysinfoModel;
class PreferencesModel;
class AbstractEncoderInfo;
class QComboBox;

namespace Ui
{
	class AddJobDialog;
}

class AddJobDialog : public QDialog
{
	Q_OBJECT

public:
	AddJobDialog(QWidget *parent, OptionsModel *const options, RecentlyUsed *const recentlyUsed, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences);
	~AddJobDialog(void);

	QString sourceFile(void);
	QString outputFile(void);

	bool runImmediately(void);
	bool applyToAll(void);
	void setRunImmediately(bool run);
	void setSourceFile(const QString &path);
	void setOutputFile(const QString &path);
	void setSourceEditable(const bool editable);
	void setApplyToAllVisible(const bool visible);
	
	static QString generateOutputFileName(const QString &sourceFilePath, const QString &destinationDirectory, const int filterIndex, const bool saveToSourceDir);
	static int getFilterIdx(const QString &fileExt);
	static QString getFilterExt(const int filterIndex);
	static QString AddJobDialog::getFilterStr(const int filterIndex);
	static QString getFilterLst(void);
	static QString getInputFilterLst(void);

protected:
	OptionsModel *const m_options;
	RecentlyUsed *const m_recentlyUsed;

	const SysinfoModel *const m_sysinfo;
	const PreferencesModel *const m_preferences;
	const OptionsModel *m_defaults;

	virtual void showEvent(QShowEvent *event);
	virtual bool eventFilter(QObject *o, QEvent *e);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);

private slots:
	void encoderIndexChanged(int index);
	void variantIndexChanged(int index);
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
	Ui::AddJobDialog *const ui;
	bool m_monitorConfigChanges;
	QString m_lastTemplateName;

	void loadTemplateList(void);
	void restoreOptions(const OptionsModel *options);
	void saveOptions(OptionsModel *options);
	void updateComboBox(QComboBox *const cbox, const QString &text);
	void updateComboBox(QComboBox *const cbox, const int &data);
	void updateComboBox(QComboBox *const cbox, const quint32 &data);

	QString currentSourcePath(const bool bWithName = false);
	QString currentOutputPath(const bool bWithName = false);
	int currentOutputIndx(void);
};
