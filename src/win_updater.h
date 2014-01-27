///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2014 LoRd_MuldeR <MuldeR2@GMX.de>
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

class QMovie;
class UpdateCheckThread;

namespace Ui
{
	class UpdaterDialog;
}

class UpdaterDialog : public QDialog
{
	Q_OBJECT

public:
	UpdaterDialog(QWidget *parent, const QString &binDir);
	~UpdaterDialog(void);

	static const int READY_TO_INSTALL_UPDATE = 42;

	inline bool getSuccess(void) { return m_success; }

protected:
	virtual bool event(QEvent *e);
	virtual void showEvent(QShowEvent *event);
	virtual void closeEvent(QCloseEvent *e);
	virtual void keyPressEvent(QKeyEvent *event);

private slots:
	void initUpdate(void);
	void checkForUpdates(void);
	void threadStatusChanged(int status);
	void threadMessageLogged(const QString &message);
	void threadFinished(void);
	void updateFinished(void);
	void openUrl(const QString &url);
	void installUpdate(void);

private:
	Ui::UpdaterDialog *const ui;

	bool checkBinaries(QString &wgetBin, QString &gpgvBin);
	bool checkFileHash(const QString &filePath, const char *expectedHash);

	bool m_firstShow;
	bool m_success;
	const QString m_binDir;
	QMovie *m_animator;
	UpdateCheckThread *m_thread;
	unsigned long m_updaterProcess;
	QStringList m_logFile;
	QString m_keysFile;
	QString m_wupdFile;
	int m_status;
};
