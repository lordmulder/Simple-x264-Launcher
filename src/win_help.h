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

#include "uic_win_help.h"

class QProcess;

class HelpDialog : public QDialog, private Ui::HelpDialog
{
	Q_OBJECT

public:
	HelpDialog(QWidget *parent, bool x64supported);
	~HelpDialog(void);

private slots:
	void readyRead(void);
	void finished(void);

private:
	const QString m_appDir;
	QProcess *const m_process;

	bool m_startAgain;

protected:
	const bool m_x64supported;

	virtual void showEvent(QShowEvent *event);
	virtual void closeEvent(QCloseEvent *e);
};

