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

#pragma once

#include "uic_win_preferences.h"

class PreferencesDialog : public QDialog, private Ui::PreferencesDialog
{
	Q_OBJECT

public:
	enum
	{
		X264_PRIORITY_NORMAL = 0,
		X264_PRIORITY_BELOWNORMAL = 1,
		X264_PRIORITY_IDLE = 2,
	}
	x264_priority_t;
	
	typedef struct
	{
		bool autoRunNextJob;
		unsigned int maxRunningJobCount;
		bool shutdownComputer;
		bool use10BitEncoding;
		bool useAvisyth64Bit;
		bool saveLogFiles;
		bool saveToSourcePath;
		unsigned int processPriority;
	}
	Preferences;

	PreferencesDialog(QWidget *parent, Preferences *preferences, bool x64);
	~PreferencesDialog(void);

	const bool m_x64;

	static void initPreferences(Preferences *preferences);
	static void loadPreferences(Preferences *preferences);
	static void savePreferences(Preferences *preferences);

protected:
	virtual void done(int n);
	virtual void showEvent(QShowEvent *event);
	virtual bool eventFilter(QObject *o, QEvent *e);

	void emulateMouseEvent(QObject *object, QEvent *event, QWidget *source, QWidget *target);

private:
	Preferences *m_preferences;

private slots:
	void resetButtonPressed(void);
	void use10BitEncodingToggled(bool checked);
};
