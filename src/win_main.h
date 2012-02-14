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

#include "uic_win_main.h"
#include "thread_encode.h"
#include "win_preferences.h"
#include "global.h"

class JobListModel;
class OptionsModel;
class QFile;
class QLibrary;

class MainWindow: public QMainWindow, private Ui::MainWindow
{
	Q_OBJECT

public:
	MainWindow(const x264_cpu_t *const cpuFeatures);
	~MainWindow(void);

protected:
	virtual void closeEvent(QCloseEvent *e);
	virtual void showEvent(QShowEvent *e);
	virtual void resizeEvent(QResizeEvent *e);
	virtual bool eventFilter(QObject *o, QEvent *e);
	virtual void dragEnterEvent(QDragEnterEvent *event);
	virtual void dropEvent(QDropEvent *event);
	virtual bool winEvent(MSG *message, long *result);

private:
	bool m_firstShow;
	QLabel *m_label;

	JobListModel *m_jobList;
	OptionsModel *m_options;
	QStringList *m_droppedFiles;
	QList<QFile*> m_toolsList;
	
	PreferencesDialog::Preferences m_preferences;

	const x264_cpu_t *const m_cpuFeatures;
	const QString m_appDir;
	
	void updateButtons(EncodeThread::JobStatus status);
	void updateTaskbar(EncodeThread::JobStatus status, const QIcon &icon);
	unsigned int countPendingJobs(void);
	unsigned int countRunningJobs(void);
	double detectAvisynthVersion(QLibrary *avsLib);

private slots:
	void addButtonPressed(const QString &filePathIn = QString(), const QString &filePathOut = QString(), OptionsModel *options = NULL, int fileNo = -1, int fileTotal = 0, bool *ok = NULL);
	void abortButtonPressed(void);
	void browseButtonPressed(void);
	void deleteButtonPressed(void);
	void copyLogToClipboard(bool checked);
	void handleDroppedFiles(void);
	void init(void);
	void jobSelected(const QModelIndex & current, const QModelIndex & previous);
	void jobChangedData(const  QModelIndex &top, const  QModelIndex &bottom);
	void jobLogExtended(const QModelIndex & parent, int start, int end);
	void launchNextJob();
	void pauseButtonPressed(bool checked);
	void restartButtonPressed(void);
	void showAbout(void);
	void showPreferences(void);
	void showWebLink(void);
	void shutdownComputer(void);
	void startButtonPressed(void);
	void updateLabelPos(void);
};
