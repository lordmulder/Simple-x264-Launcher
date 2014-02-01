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

#include "global.h"
#include <QMainWindow>

class IPC;
class JobListModel;
class OptionsModel;
class QFile;
class QLibrary;
class PreferencesModel;
class RecentlyUsed;
class QModelIndex;
class QLabel;
enum JobStatus;

namespace Ui
{
	class MainWindow;
}

class MainWindow: public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(const x264_cpu_t *const cpuFeatures, IPC *ipc);
	~MainWindow(void);

protected:
	virtual void closeEvent(QCloseEvent *e);
	virtual void showEvent(QShowEvent *e);
	virtual void resizeEvent(QResizeEvent *e);
	virtual bool eventFilter(QObject *o, QEvent *e);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);
	virtual bool winEvent(MSG *message, long *result);

	typedef enum
	{
		STATUS_PRE_INIT = 0,
		STATUS_IDLE     = 1,
		STATUS_AWAITING = 2,
		STATUS_BLOCKED  = 3,
		STATUS_EXITTING = 4
	}
	x264_status_t;

private:
	Ui::MainWindow *const ui;

	bool m_firstShow;
	bool m_skipVersionTest;
	bool m_abortOnTimeout;

	x264_status_t m_status;

	QLabel *m_label;
	IPC *const m_ipc;

	JobListModel *m_jobList;
	OptionsModel *m_options;
	QStringList *m_pendingFiles;
	QList<QFile*> m_toolsList;
	
	PreferencesModel *m_preferences;
	RecentlyUsed *m_recentlyUsed;

	QString m_vapoursynthPath;

	const x264_cpu_t *const m_cpuFeatures;
	const QString m_appDir;
	
	bool createJob(QString &sourceFileName, QString &outputFileName, OptionsModel *options, bool &runImmediately, const bool restart = false, int fileNo = -1, int fileTotal = 0, bool *applyToAll = NULL);
	bool createJobMultiple(const QStringList &filePathIn);

	bool appendJob(const QString &sourceFileName, const QString &outputFileName, OptionsModel *options, const bool runImmediately);
	void updateButtons(JobStatus status);
	void updateTaskbar(JobStatus status, const QIcon &icon);
	unsigned int countPendingJobs(void);
	unsigned int countRunningJobs(void);

	bool parseCommandLineArgs(void);

private slots:
	void addButtonPressed();
	void openActionTriggered();
	void abortButtonPressed(void);
	void browseButtonPressed(void);
	void deleteButtonPressed(void);
	void copyLogToClipboard(bool checked);
	void checkUpdates(void);
	void handlePendingFiles(void);
	void init(void);
	void handleCommand(const int &command, const QStringList &args, const quint32 &flags = 0);
	void jobSelected(const QModelIndex &current, const QModelIndex &previous);
	void jobChangedData(const  QModelIndex &top, const  QModelIndex &bottom);
	void jobLogExtended(const QModelIndex & parent, int start, int end);
	void launchNextJob();
	void pauseButtonPressed(bool checked);
	void restartButtonPressed(void);
	void saveLogFile(const QModelIndex &index);
	void showAbout(void);
	void showPreferences(void);
	void showWebLink(void);
	void shutdownComputer(void);
	void startButtonPressed(void);
	void updateLabelPos(void);
};
