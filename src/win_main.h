///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2016 LoRd_MuldeR <MuldeR2@GMX.de>
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

//Internal
#include "global.h"

//Qt
#include <QMainWindow>

//Forward declarations
class JobListModel;
class OptionsModel;
class SysinfoModel;
class QFile;
class QLibrary;
class PreferencesModel;
class RecentlyUsed;
class InputEventFilter;
class QModelIndex;
class QLabel;
class QSystemTrayIcon;
class IPCThread_Recv;
enum JobStatus;

namespace Ui
{
	class MainWindow;
}

namespace MUtils
{
	class IPCChannel;
	class Taskbar7;
	namespace CPUFetaures
	{
		typedef struct _cpu_info_t cpu_info_t;
	}
}

class MainWindow: public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(const MUtils::CPUFetaures::cpu_info_t &cpuFeatures, MUtils::IPCChannel *const ipcChannel);
	~MainWindow(void);

protected:
	virtual void closeEvent(QCloseEvent *e);
	virtual void showEvent(QShowEvent *e);
	virtual void resizeEvent(QResizeEvent *e);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);

private:
	Ui::MainWindow *const ui;
	MUtils::IPCChannel *const m_ipcChannel;

	bool m_initialized;
	QScopedPointer<QLabel> m_label[2];
	QScopedPointer<QMovie> m_animation;
	QScopedPointer<QTimer> m_fileTimer;

	QScopedPointer<IPCThread_Recv>   m_ipcThread;
	QScopedPointer<MUtils::Taskbar7> m_taskbar;
	QScopedPointer<QSystemTrayIcon>  m_sysTray;

	QScopedPointer<InputEventFilter> m_inputFilter_jobList;
	QScopedPointer<InputEventFilter> m_inputFilter_version;
	QScopedPointer<InputEventFilter> m_inputFilter_checkUp;

	QScopedPointer<JobListModel> m_jobList;
	QScopedPointer<OptionsModel> m_options;
	QScopedPointer<QStringList> m_pendingFiles;
	
	QScopedPointer<SysinfoModel> m_sysinfo;
	QScopedPointer<PreferencesModel> m_preferences;
	QScopedPointer<RecentlyUsed> m_recentlyUsed;
	
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
	void cleanupActionTriggered(void);
	void abortButtonPressed(void);
	void browseButtonPressed(void);
	void deleteButtonPressed(void);
	void copyLogToClipboard(bool checked);
	void saveLogToLocalFile(bool checked);
	void toggleLineWrapping(bool checked);
	void checkUpdates(void);
	void handlePendingFiles(void);
	void init(void);
	void handleCommand(const int &command, const QStringList &args, const quint32 &flags = 0);
	void jobSelected(const QModelIndex &current, const QModelIndex &previous);
	void jobChangedData(const  QModelIndex &top, const  QModelIndex &bottom);
	void jobLogExtended(const QModelIndex & parent, int start, int end);
	void jobListKeyPressed(const int &tag);
	void launchNextJob();
	void moveButtonPressed(void);
	void pauseButtonPressed(bool checked);
	void restartButtonPressed(void);
	void saveLogFile(const QModelIndex &index);
	void showAbout(void);
	void showPreferences(void);
	void showWebLink(void);
	void shutdownComputer(void);
	void startButtonPressed(void);
	void sysTrayActived(void);
	void updateLabelPos(void);
	void versionLabelMouseClicked(const int &tag);
};
