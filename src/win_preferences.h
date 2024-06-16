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

class PreferencesModel;
class SysinfoModel;

namespace Ui
{
	class PreferencesDialog;
}

class PreferencesDialog : public QDialog
{
	Q_OBJECT

public:
	PreferencesDialog(QWidget *parent, PreferencesModel *preferences, const SysinfoModel *sysinfo);
	~PreferencesDialog(void);

protected:
	virtual void done(int n);
	virtual void showEvent(QShowEvent *event);
	virtual bool eventFilter(QObject *o, QEvent *e);

	inline static void emulateMouseEvent(QObject *object, QEvent *event, QWidget *source, QWidget *target);

private:
	Ui::PreferencesDialog *const ui;
	const SysinfoModel *const m_sysinfo;
	PreferencesModel *m_preferences;

private slots:
	void resetButtonPressed(void);
	void disableWarningsToggled(bool checked);
};
