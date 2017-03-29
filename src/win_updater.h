///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2017 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <QMap>

class QMovie;
class QElapsedTimer;
class SysinfoModel;

namespace Ui
{
	class UpdaterDialog;
}

namespace MUtils
{
	class UpdateChecker;
}

class UpdaterDialog : public QDialog
{
	Q_OBJECT

public:
	UpdaterDialog(QWidget *parent, const SysinfoModel *sysinfo, const char *const updateUrl);
	~UpdaterDialog(void);

	typedef struct
	{
		const char* name;
		const char* hash;
		const bool  exec;
	}
	binary_t;

	static const int READY_TO_INSTALL_UPDATE = 42;
	static const binary_t BINARIES[];

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

	bool checkBinaries();
	bool checkFileHash(const QString &filePath, const char *expectedHash);
	void cleanFiles(void);

	const SysinfoModel *const m_sysinfo;
	const char *const m_updateUrl;

	bool m_firstShow;
	bool m_success;

	QScopedPointer<QMovie> m_animator;
	QScopedPointer<MUtils::UpdateChecker> m_thread;
	QScopedPointer<QElapsedTimer> m_elapsed;

	unsigned long m_updaterProcess;
	QStringList m_logFile;
	QMap<QString,QString> m_binaries;
	int m_status;
};
