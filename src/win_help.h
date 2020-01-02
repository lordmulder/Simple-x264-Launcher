///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2020 LoRd_MuldeR <MuldeR2@GMX.de>
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

class QProcess;
class SysinfoModel;
class PreferencesModel;
class OptionsModel;

namespace Ui
{
	class HelpDialog;
}

class HelpDialog : public QDialog
{
	Q_OBJECT

public:
	HelpDialog(QWidget *parent, bool avs2yuv, const SysinfoModel *const sysinfo, const OptionsModel *const options, const PreferencesModel *const preferences);
	~HelpDialog(void);

private slots:
	void readyRead(void);
	void finished(void);

private:
	Ui::HelpDialog *const ui;

	const bool m_avs2yuv;

	const SysinfoModel *const m_sysinfo;
	const PreferencesModel *const m_preferences;
	const OptionsModel *const m_options;

	QProcess *const m_process;
	bool m_startAgain;

protected:
	virtual void showEvent(QShowEvent *event);
	virtual void closeEvent(QCloseEvent *e);
};

