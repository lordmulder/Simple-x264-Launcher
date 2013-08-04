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

#include "win_main.h"

#include "model_jobList.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_recently.h"
#include "thread_avisynth.h"
#include "thread_ipc.h"
#include "thread_encode.h"
#include "taskbar7.h"
#include "win_addJob.h"
#include "win_preferences.h"
#include "resource.h"

#include <QDate>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QLibrary>
#include <QProcess>
#include <QProgressDialog>
#include <QScrollBar>
#include <QTextStream>
#include <QSettings>
#include <QFileDialog>

#include <Mmsystem.h>

const char *home_url = "http://muldersoft.com/";
const char *update_url = "http://code.google.com/p/mulder/downloads/list";
const char *tpl_last = "<LAST_USED>";

#define SET_FONT_BOLD(WIDGET,BOLD) { QFont _font = WIDGET->font(); _font.setBold(BOLD); WIDGET->setFont(_font); }
#define SET_TEXT_COLOR(WIDGET,COLOR) { QPalette _palette = WIDGET->palette(); _palette.setColor(QPalette::WindowText, (COLOR)); _palette.setColor(QPalette::Text, (COLOR)); WIDGET->setPalette(_palette); }

static int exceptionFilter(_EXCEPTION_RECORD *dst, _EXCEPTION_POINTERS *src) { memcpy(dst, src->ExceptionRecord, sizeof(_EXCEPTION_RECORD)); return EXCEPTION_EXECUTE_HANDLER; }

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

/*
 * Constructor
 */
MainWindow::MainWindow(const x264_cpu_t *const cpuFeatures)
:
	m_cpuFeatures(cpuFeatures),
	m_appDir(QApplication::applicationDirPath()),
	m_options(NULL),
	m_jobList(NULL),
	m_droppedFiles(NULL),
	m_preferences(NULL),
	m_recentlyUsed(NULL),
	m_skipVersionTest(false),
	m_abortOnTimeout(true),
	m_firstShow(true)
{
	//Init the dialog, from the .ui file
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowMaximizeButtonHint));

	//Register meta types
	qRegisterMetaType<QUuid>("QUuid");
	qRegisterMetaType<QUuid>("DWORD");
	qRegisterMetaType<JobStatus>("JobStatus");

	//Load preferences
	m_preferences = new PreferencesModel();
	PreferencesModel::loadPreferences(m_preferences);

	//Load recently used
	m_recentlyUsed = new RecentlyUsed();
	RecentlyUsed::loadRecentlyUsed(m_recentlyUsed);

	//Create options object
	m_options = new OptionsModel();
	OptionsModel::loadTemplate(m_options, QString::fromLatin1(tpl_last));

	//Create IPC thread object
	m_ipcThread = new IPCThread();
	connect(m_ipcThread, SIGNAL(instanceCreated(DWORD)), this, SLOT(instanceCreated(DWORD)), Qt::QueuedConnection);

	//Freeze minimum size
	setMinimumSize(size());
	splitter->setSizes(QList<int>() << 16 << 196);

	//Update title
	labelBuildDate->setText(tr("Built on %1 at %2").arg(x264_version_date().toString(Qt::ISODate), QString::fromLatin1(x264_version_time())));
	labelBuildDate->installEventFilter(this);
	setWindowTitle(QString("%1 (%2 Mode)").arg(windowTitle(), m_cpuFeatures->x64 ? "64-Bit" : "32-Bit"));
	if(X264_DEBUG)
	{
		setWindowTitle(QString("%1 | !!! DEBUG VERSION !!!").arg(windowTitle()));
		setStyleSheet("QMenuBar, QMainWindow { background-color: yellow }");
	}
	else if(x264_is_prerelease())
	{
		setWindowTitle(QString("%1 | PRE-RELEASE VERSION").arg(windowTitle()));
	}
	
	//Create model
	m_jobList = new JobListModel(m_preferences);
	connect(m_jobList, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(jobChangedData(QModelIndex, QModelIndex)));
	jobsView->setModel(m_jobList);
	
	//Setup view
	jobsView->horizontalHeader()->setSectionHidden(3, true);
	jobsView->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
	jobsView->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
	jobsView->horizontalHeader()->setResizeMode(2, QHeaderView::Fixed);
	jobsView->horizontalHeader()->resizeSection(1, 150);
	jobsView->horizontalHeader()->resizeSection(2, 90);
	jobsView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	connect(jobsView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(jobSelected(QModelIndex, QModelIndex)));

	//Create context menu
	QAction *actionClipboard = new QAction(QIcon(":/buttons/page_paste.png"), tr("Copy to Clipboard"), logView);
	actionClipboard->setEnabled(false);
	logView->addAction(actionClipboard);
	connect(actionClipboard, SIGNAL(triggered(bool)), this, SLOT(copyLogToClipboard(bool)));
	jobsView->addActions(menuJob->actions());

	//Enable buttons
	connect(buttonAddJob, SIGNAL(clicked()), this, SLOT(addButtonPressed()));
	connect(buttonStartJob, SIGNAL(clicked()), this, SLOT(startButtonPressed()));
	connect(buttonAbortJob, SIGNAL(clicked()), this, SLOT(abortButtonPressed()));
	connect(buttonPauseJob, SIGNAL(toggled(bool)), this, SLOT(pauseButtonPressed(bool)));
	connect(actionJob_Delete, SIGNAL(triggered()), this, SLOT(deleteButtonPressed()));
	connect(actionJob_Restart, SIGNAL(triggered()), this, SLOT(restartButtonPressed()));
	connect(actionJob_Browse, SIGNAL(triggered()), this, SLOT(browseButtonPressed()));

	//Enable menu
	connect(actionOpen, SIGNAL(triggered()), this, SLOT(openActionTriggered()));
	connect(actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
	connect(actionWebMulder, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebX264, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebKomisar, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebJarod, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebJEEB, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebAvisynth32, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebAvisynth64, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebWiki, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebBluRay, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebAvsWiki, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebSecret, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebSupport, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionPreferences, SIGNAL(triggered()), this, SLOT(showPreferences()));

	//Create floating label
	m_label = new QLabel(jobsView->viewport());
	m_label->setText(tr("No job created yet. Please click the 'Add New Job' button!"));
	m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	SET_TEXT_COLOR(m_label, Qt::darkGray);
	SET_FONT_BOLD(m_label, true);
	m_label->setVisible(true);
	m_label->setContextMenuPolicy(Qt::ActionsContextMenu);
	m_label->addActions(jobsView->actions());
	connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(updateLabelPos()));
	updateLabelPos();
}

/*
 * Destructor
 */
MainWindow::~MainWindow(void)
{
	OptionsModel::saveTemplate(m_options, QString::fromLatin1(tpl_last));
	
	X264_DELETE(m_jobList);
	X264_DELETE(m_options);
	X264_DELETE(m_droppedFiles);
	X264_DELETE(m_label);

	while(!m_toolsList.isEmpty())
	{
		QFile *temp = m_toolsList.takeFirst();
		X264_DELETE(temp);
	}

	if(m_ipcThread->isRunning())
	{
		m_ipcThread->setAbort();
		if(!m_ipcThread->wait(5000))
		{
			m_ipcThread->terminate();
			m_ipcThread->wait();
		}
	}

	X264_DELETE(m_ipcThread);
	AvisynthCheckThread::unload();
	X264_DELETE(m_preferences);
	X264_DELETE(m_recentlyUsed);
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

/*
 * The "add" button was clicked
 */
void MainWindow::addButtonPressed()
{
	qDebug("MainWindow::addButtonPressed");
	bool runImmediately = (countRunningJobs() < (m_preferences->autoRunNextJob() ? m_preferences->maxRunningJobCount() : 1));
	QString sourceFileName, outputFileName;
	
	if(createJob(sourceFileName, outputFileName, m_options, runImmediately))
	{
		appendJob(sourceFileName, outputFileName, m_options, runImmediately);
	}
}

/*
 * The "open" action was triggered
 */
void MainWindow::openActionTriggered()
{
	QStringList fileList = QFileDialog::getOpenFileNames(this, tr("Open Source File(s)"), m_recentlyUsed->sourceDirectory(), AddJobDialog::getInputFilterLst(), NULL, QFileDialog::DontUseNativeDialog);
	if(!fileList.empty())
	{
		m_recentlyUsed->setSourceDirectory(QFileInfo(fileList.last()).absolutePath());
		if(fileList.count() > 1)
		{
			createJobMultiple(fileList);
		}
		else
		{
			bool runImmediately = (countRunningJobs() < (m_preferences->autoRunNextJob() ? m_preferences->maxRunningJobCount() : 1));
			QString sourceFileName(fileList.first()), outputFileName;
			if(createJob(sourceFileName, outputFileName, m_options, runImmediately))
			{
				appendJob(sourceFileName, outputFileName, m_options, runImmediately);
			}
		}
	}
}

/*
 * The "start" button was clicked
 */
void MainWindow::startButtonPressed(void)
{
	m_jobList->startJob(jobsView->currentIndex());
}

/*
 * The "abort" button was clicked
 */
void MainWindow::abortButtonPressed(void)
{
	m_jobList->abortJob(jobsView->currentIndex());
}

/*
 * The "delete" button was clicked
 */
void MainWindow::deleteButtonPressed(void)
{
	m_jobList->deleteJob(jobsView->currentIndex());
	m_label->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
}

/*
 * The "browse" button was clicked
 */
void MainWindow::browseButtonPressed(void)
{
	QString outputFile = m_jobList->getJobOutputFile(jobsView->currentIndex());
	if((!outputFile.isEmpty()) && QFileInfo(outputFile).exists() && QFileInfo(outputFile).isFile())
	{
		QProcess::startDetached(QString::fromLatin1("explorer.exe"), QStringList() << QString::fromLatin1("/select,") << QDir::toNativeSeparators(outputFile), QFileInfo(outputFile).path());
	}
	else
	{
		QMessageBox::warning(this, tr("Not Found"), tr("Sorry, the output file could not be found!"));
	}
}

/*
 * The "pause" button was clicked
 */
void MainWindow::pauseButtonPressed(bool checked)
{
	if(checked)
	{
		m_jobList->pauseJob(jobsView->currentIndex());
	}
	else
	{
		m_jobList->resumeJob(jobsView->currentIndex());
	}
}

/*
 * The "restart" button was clicked
 */
void MainWindow::restartButtonPressed(void)
{
	const QModelIndex index = jobsView->currentIndex();
	const OptionsModel *options = m_jobList->getJobOptions(index);
	QString sourceFileName = m_jobList->getJobSourceFile(index);
	QString outputFileName = m_jobList->getJobOutputFile(index);

	if((options) && (!sourceFileName.isEmpty()) && (!outputFileName.isEmpty()))
	{
		bool runImmediately = true;
		OptionsModel *tempOptions = new OptionsModel(*options);
		if(createJob(sourceFileName, outputFileName, tempOptions, runImmediately, true))
		{
			appendJob(sourceFileName, outputFileName, tempOptions, runImmediately);
		}
		X264_DELETE(tempOptions);
	}
}

/*
 * Job item selected by user
 */
void MainWindow::jobSelected(const QModelIndex & current, const QModelIndex & previous)
{
	qDebug("Job selected: %d", current.row());
	
	if(logView->model())
	{
		disconnect(logView->model(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(jobLogExtended(QModelIndex, int, int)));
	}
	
	if(current.isValid())
	{
		logView->setModel(m_jobList->getLogFile(current));
		connect(logView->model(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(jobLogExtended(QModelIndex, int, int)));
		logView->actions().first()->setEnabled(true);
		QTimer::singleShot(0, logView, SLOT(scrollToBottom()));

		progressBar->setValue(m_jobList->getJobProgress(current));
		editDetails->setText(m_jobList->data(m_jobList->index(current.row(), 3, QModelIndex()), Qt::DisplayRole).toString());
		updateButtons(m_jobList->getJobStatus(current));
		updateTaskbar(m_jobList->getJobStatus(current), m_jobList->data(m_jobList->index(current.row(), 0, QModelIndex()), Qt::DecorationRole).value<QIcon>());
	}
	else
	{
		logView->setModel(NULL);
		logView->actions().first()->setEnabled(false);
		progressBar->setValue(0);
		editDetails->clear();
		updateButtons(JobStatus_Undefined);
		updateTaskbar(JobStatus_Undefined, QIcon());
	}

	progressBar->repaint();
}

/*
 * Handle update of job info (status, progress, details, etc)
 */
void MainWindow::jobChangedData(const QModelIndex &topLeft, const  QModelIndex &bottomRight)
{
	int selected = jobsView->currentIndex().row();
	
	if(topLeft.column() <= 1 && bottomRight.column() >= 1) /*STATUS*/
	{
		for(int i = topLeft.row(); i <= bottomRight.row(); i++)
		{
			JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
			if(i == selected)
			{
				qDebug("Current job changed status!");
				updateButtons(status);
				updateTaskbar(status, m_jobList->data(m_jobList->index(i, 0, QModelIndex()), Qt::DecorationRole).value<QIcon>());
			}
			if((status == JobStatus_Completed) || (status == JobStatus_Failed))
			{
				if(m_preferences->autoRunNextJob()) QTimer::singleShot(0, this, SLOT(launchNextJob()));
				if(m_preferences->shutdownComputer()) QTimer::singleShot(0, this, SLOT(shutdownComputer()));
				if(m_preferences->saveLogFiles()) saveLogFile(m_jobList->index(i, 1, QModelIndex()));
			}
		}
	}
	if(topLeft.column() <= 2 && bottomRight.column() >= 2) /*PROGRESS*/
	{
		for(int i = topLeft.row(); i <= bottomRight.row(); i++)
		{
			if(i == selected)
			{
				progressBar->setValue(m_jobList->getJobProgress(m_jobList->index(i, 0, QModelIndex())));
				WinSevenTaskbar::setTaskbarProgress(this, progressBar->value(), progressBar->maximum());
				break;
			}
		}
	}
	if(topLeft.column() <= 3 && bottomRight.column() >= 3) /*DETAILS*/
	{
		for(int i = topLeft.row(); i <= bottomRight.row(); i++)
		{
			if(i == selected)
			{
				editDetails->setText(m_jobList->data(m_jobList->index(i, 3, QModelIndex()), Qt::DisplayRole).toString());
				break;
			}
		}
	}
}

/*
 * Handle new log file content
 */
void MainWindow::jobLogExtended(const QModelIndex & parent, int start, int end)
{
	QTimer::singleShot(0, logView, SLOT(scrollToBottom()));
}

/*
 * About screen
 */
void MainWindow::showAbout(void)
{
	QString text;

	text += QString().sprintf("<nobr><tt>Simple x264 Launcher v%u.%02u.%u - use 64-Bit x264 with 32-Bit Avisynth<br>", x264_version_major(), x264_version_minor(), x264_version_build());
	text += QString().sprintf("Copyright (c) 2004-%04d LoRd_MuldeR &lt;mulder2@gmx.de&gt;. Some rights reserved.<br>", qMax(x264_version_date().year(),QDate::currentDate().year()));
	text += QString().sprintf("Built on %s at %s with %s for Win-%s.<br><br>", x264_version_date().toString(Qt::ISODate).toLatin1().constData(), x264_version_time(), x264_version_compiler(), x264_version_arch());
	text += QString().sprintf("This program is free software: you can redistribute it and/or modify<br>");
	text += QString().sprintf("it under the terms of the GNU General Public License &lt;http://www.gnu.org/&gt;.<br>");
	text += QString().sprintf("Note that this program is distributed with ABSOLUTELY NO WARRANTY.<br><br>");
	text += QString().sprintf("Please check the web-site at <a href=\"%s\">%s</a> for updates !!!<br></tt></nobr>", home_url, home_url);

	QMessageBox aboutBox(this);
	aboutBox.setIconPixmap(QIcon(":/images/movie.png").pixmap(64,64));
	aboutBox.setWindowTitle(tr("About..."));
	aboutBox.setText(text.replace("-", "&minus;"));
	aboutBox.addButton(tr("About x264"), QMessageBox::NoRole);
	aboutBox.addButton(tr("About AVS"), QMessageBox::NoRole);
	aboutBox.addButton(tr("About Qt"), QMessageBox::NoRole);
	aboutBox.setEscapeButton(aboutBox.addButton(tr("Close"), QMessageBox::NoRole));
		
	forever
	{
		MessageBeep(MB_ICONINFORMATION);
		switch(aboutBox.exec())
		{
		case 0:
			{
				QString text2;
				text2 += tr("<nobr><tt>x264 - the best H.264/AVC encoder. Copyright (c) 2003-2013 x264 project.<br>");
				text2 += tr("Free software library for encoding video streams into the H.264/MPEG-4 AVC format.<br>");
				text2 += tr("Released under the terms of the GNU General Public License.<br><br>");
				text2 += tr("Please visit <a href=\"%1\">%1</a> for obtaining a commercial x264 license.<br>").arg("http://x264licensing.com/");
				text2 += tr("Read the <a href=\"%1\">user's manual</a> to get started and use the <a href=\"%2\">support forum</a> for help!<br></tt></nobr>").arg("http://mewiki.project357.com/wiki/X264_Settings", "http://forum.doom9.org/forumdisplay.php?f=77");

				QMessageBox x264Box(this);
				x264Box.setIconPixmap(QIcon(":/images/x264.png").pixmap(48,48));
				x264Box.setWindowTitle(tr("About x264"));
				x264Box.setText(text2.replace("-", "&minus;"));
				x264Box.setEscapeButton(x264Box.addButton(tr("Close"), QMessageBox::NoRole));
				MessageBeep(MB_ICONINFORMATION);
				x264Box.exec();
			}
			break;
		case 1:
			{
				QString text2;
				text2 += tr("<nobr><tt>Avisynth - powerful video processing scripting language.<br>");
				text2 += tr("Copyright (c) 2000 Ben Rudiak-Gould and all subsequent developers.<br>");
				text2 += tr("Released under the terms of the GNU General Public License.<br><br>");
				text2 += tr("Please visit the web-site <a href=\"%1\">%1</a> for more information.<br>").arg("http://avisynth.org/");
				text2 += tr("Read the <a href=\"%1\">guide</a> to get started and use the <a href=\"%2\">support forum</a> for help!<br></tt></nobr>").arg("http://avisynth.org/mediawiki/First_script", "http://forum.doom9.org/forumdisplay.php?f=33");

				QMessageBox x264Box(this);
				x264Box.setIconPixmap(QIcon(":/images/avisynth.png").pixmap(48,67));
				x264Box.setWindowTitle(tr("About Avisynth"));
				x264Box.setText(text2.replace("-", "&minus;"));
				x264Box.setEscapeButton(x264Box.addButton(tr("Close"), QMessageBox::NoRole));
				MessageBeep(MB_ICONINFORMATION);
				x264Box.exec();
			}
			break;
		case 2:
			QMessageBox::aboutQt(this);
			break;
		default:
			return;
		}
	}
}

/*
 * Open web-link
 */
void MainWindow::showWebLink(void)
{
	if(QObject::sender() == actionWebMulder)     QDesktopServices::openUrl(QUrl(home_url));
	if(QObject::sender() == actionWebX264)       QDesktopServices::openUrl(QUrl("http://www.x264.com/"));
	if(QObject::sender() == actionWebKomisar)    QDesktopServices::openUrl(QUrl("http://komisar.gin.by/"));
	if(QObject::sender() == actionWebJarod)      QDesktopServices::openUrl(QUrl("http://www.x264.nl/"));
	if(QObject::sender() == actionWebJEEB)       QDesktopServices::openUrl(QUrl("http://x264.fushizen.eu/"));
	if(QObject::sender() == actionWebAvisynth32) QDesktopServices::openUrl(QUrl("http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/"));
	if(QObject::sender() == actionWebAvisynth64) QDesktopServices::openUrl(QUrl("http://code.google.com/p/avisynth64/downloads/list"));
	if(QObject::sender() == actionWebWiki)       QDesktopServices::openUrl(QUrl("http://mewiki.project357.com/wiki/X264_Settings"));
	if(QObject::sender() == actionWebBluRay)     QDesktopServices::openUrl(QUrl("http://www.x264bluray.com/"));
	if(QObject::sender() == actionWebAvsWiki)    QDesktopServices::openUrl(QUrl("http://avisynth.org/mediawiki/Main_Page#Usage"));
	if(QObject::sender() == actionWebSupport)    QDesktopServices::openUrl(QUrl("http://forum.doom9.org/showthread.php?t=144140"));
	if(QObject::sender() == actionWebSecret)     QDesktopServices::openUrl(QUrl("http://www.youtube.com/watch_popup?v=AXIeHY-OYNI"));
}

/*
 * Pereferences dialog
 */
void MainWindow::showPreferences(void)
{
	PreferencesDialog *preferences = new PreferencesDialog(this, m_preferences, m_cpuFeatures->x64);
	preferences->exec();
	X264_DELETE(preferences);
}

/*
 * Launch next job, after running job has finished
 */
void MainWindow::launchNextJob(void)
{
	qDebug("launchNextJob(void)");
	
	const int rows = m_jobList->rowCount(QModelIndex());

	if(countRunningJobs() >= m_preferences->maxRunningJobCount())
	{
		qDebug("Still have too many jobs running, won't launch next one yet!");
		return;
	}

	int startIdx= jobsView->currentIndex().isValid() ? qBound(0, jobsView->currentIndex().row(), rows-1) : 0;

	for(int i = 0; i < rows; i++)
	{
		int currentIdx = (i + startIdx) % rows;
		JobStatus status = m_jobList->getJobStatus(m_jobList->index(currentIdx, 0, QModelIndex()));
		if(status == JobStatus_Enqueued)
		{
			if(m_jobList->startJob(m_jobList->index(currentIdx, 0, QModelIndex())))
			{
				jobsView->selectRow(currentIdx);
				return;
			}
		}
	}
		
	qWarning("No enqueued jobs left!");
}

/*
 * Save log to text file
 */
void MainWindow::saveLogFile(const QModelIndex &index)
{
	if(index.isValid())
	{
		if(LogFileModel *log = m_jobList->getLogFile(index))
		{
			QDir(QString("%1/logs").arg(x264_data_path())).mkpath(".");
			QString logFilePath = QString("%1/logs/LOG.%2.%3.txt").arg(x264_data_path(), QDate::currentDate().toString(Qt::ISODate), QTime::currentTime().toString(Qt::ISODate).replace(':', "-"));
			QFile outFile(logFilePath);
			if(outFile.open(QIODevice::WriteOnly))
			{
				QTextStream outStream(&outFile);
				outStream.setCodec("UTF-8");
				outStream.setGenerateByteOrderMark(true);
				
				const int rows = log->rowCount(QModelIndex());
				for(int i = 0; i < rows; i++)
				{
					outStream << log->data(log->index(i, 0, QModelIndex()), Qt::DisplayRole).toString() << QLatin1String("\r\n");
				}
				outFile.close();
			}
			else
			{
				qWarning("Failed to open log file for writing:\n%s", logFilePath.toUtf8().constData());
			}
		}
	}
}

/*
 * Shut down the computer (with countdown)
 */
void MainWindow::shutdownComputer(void)
{
	qDebug("shutdownComputer(void)");
	
	if(countPendingJobs() > 0)
	{
		qDebug("Still have pending jobs, won't shutdown yet!");
		return;
	}
	
	const int iTimeout = 30;
	const Qt::WindowFlags flags = Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowSystemMenuHint;
	const QString text = QString("%1%2%1").arg(QString().fill(' ', 18), tr("Warning: Computer will shutdown in %1 seconds..."));
	
	qWarning("Initiating shutdown sequence!");
	
	QProgressDialog progressDialog(text.arg(iTimeout), tr("Cancel Shutdown"), 0, iTimeout + 1, this, flags);
	QPushButton *cancelButton = new QPushButton(tr("Cancel Shutdown"), &progressDialog);
	cancelButton->setIcon(QIcon(":/buttons/power_on.png"));
	progressDialog.setModal(true);
	progressDialog.setAutoClose(false);
	progressDialog.setAutoReset(false);
	progressDialog.setWindowIcon(QIcon(":/buttons/power_off.png"));
	progressDialog.setWindowTitle(windowTitle());
	progressDialog.setCancelButton(cancelButton);
	progressDialog.show();
	
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	QApplication::setOverrideCursor(Qt::WaitCursor);
	PlaySound(MAKEINTRESOURCE(IDR_WAVE1), GetModuleHandle(NULL), SND_RESOURCE | SND_SYNC);
	QApplication::restoreOverrideCursor();
	
	QTimer timer;
	timer.setInterval(1000);
	timer.start();

	QEventLoop eventLoop(this);
	connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
	connect(&progressDialog, SIGNAL(canceled()), &eventLoop, SLOT(quit()));

	for(int i = 1; i <= iTimeout; i++)
	{
		eventLoop.exec();
		if(progressDialog.wasCanceled())
		{
			progressDialog.close();
			return;
		}
		progressDialog.setValue(i+1);
		progressDialog.setLabelText(text.arg(iTimeout-i));
		if(iTimeout-i == 3) progressDialog.setCancelButton(NULL);
		QApplication::processEvents();
		PlaySound(MAKEINTRESOURCE((i < iTimeout) ? IDR_WAVE2 : IDR_WAVE3), GetModuleHandle(NULL), SND_RESOURCE | SND_SYNC);
	}
	
	qWarning("Shutting down !!!");

	if(x264_shutdown_computer("Simple x264 Launcher: All jobs completed, shutting down!", 10, true))
	{
		qApp->closeAllWindows();
	}
}

/*
 * Main initialization function (called only once!)
 */
void MainWindow::init(void)
{
	static const char *binFiles = "x86/x264_8bit_x86.exe:x64/x264_8bit_x64.exe:x86/x264_10bit_x86.exe:x64/x264_10bit_x64.exe:x86/avs2yuv_x86.exe:x64/avs2yuv_x64.exe";
	QStringList binaries = QString::fromLatin1(binFiles).split(":", QString::SkipEmptyParts);

	updateLabelPos();

	//Check for a running instance
	bool firstInstance = false;
	if(m_ipcThread->initialize(&firstInstance))
	{
		m_ipcThread->start();
		if(!firstInstance)
		{
			if(!m_ipcThread->wait(5000))
			{
				QMessageBox::warning(this, tr("Not Responding"), tr("<nobr>Another instance of this application is already running, but did not respond in time.<br>If the problem persists, please kill the running instance from the task manager!</nobr>"), tr("Quit"));
				m_ipcThread->terminate();
				m_ipcThread->wait();
			}
			close(); qApp->exit(-1); return;
		}
	}

	//Check all binaries
	while(!binaries.isEmpty())
	{
		qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
		QString current = binaries.takeFirst();
		QFile *file = new QFile(QString("%1/toolset/%2").arg(m_appDir, current));
		if(file->open(QIODevice::ReadOnly))
		{
			bool binaryTypeOkay = false;
			DWORD binaryType;
			if(GetBinaryType(QWCHAR(QDir::toNativeSeparators(file->fileName())), &binaryType))
			{
				binaryTypeOkay = (binaryType == SCS_32BIT_BINARY || binaryType == SCS_64BIT_BINARY);
			}
			if(!binaryTypeOkay)
			{
				QMessageBox::critical(this, tr("Invalid File!"), tr("<nobr>At least on required tool is not a valid Win32 or Win64 binary:<br><tt style=\"whitespace:nowrap\">%1</tt><br><br>Please re-install the program in order to fix the problem!</nobr>").arg(QDir::toNativeSeparators(QString("%1/toolset/%2").arg(m_appDir, current))).replace("-", "&minus;"));
				qFatal(QString("Binary is invalid: %1/toolset/%2").arg(m_appDir, current).toLatin1().constData());
				close(); qApp->exit(-1); return;
			}
			m_toolsList << file;
		}
		else
		{
			X264_DELETE(file);
			QMessageBox::critical(this, tr("File Not Found!"), tr("<nobr>At least on required tool could not be found:<br><tt style=\"whitespace:nowrap\">%1</tt><br><br>Please re-install the program in order to fix the problem!</nobr>").arg(QDir::toNativeSeparators(QString("%1/toolset/%2").arg(m_appDir, current))).replace("-", "&minus;"));
			qFatal(QString("Binary not found: %1/toolset/%2").arg(m_appDir, current).toLatin1().constData());
			close(); qApp->exit(-1); return;
		}
	}

	//Check for portable mode
	if(x264_portable())
	{
		bool ok = false;
		static const char *data = "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";
		QFile writeTest(QString("%1/%2").arg(x264_data_path(), QUuid::createUuid().toString()));
		if(writeTest.open(QIODevice::WriteOnly))
		{
			ok = (writeTest.write(data) == strlen(data));
			writeTest.remove();
		}
		if(!ok)
		{
			int val = QMessageBox::warning(this, tr("Write Test Failed"), tr("<nobr>The application was launched in portable mode, but the program path is <b>not</b> writable!</nobr>"), tr("Quit"), tr("Ignore"));
			if(val != 1) { close(); qApp->exit(-1); return; }
		}
	}

	//Pre-release popup
	if(x264_is_prerelease())
	{
		qsrand(time(NULL)); int rnd = qrand() % 3;
		int val = QMessageBox::information(this, tr("Pre-Release Version"), tr("Note: This is a pre-release version. Please do NOT use for production!<br>Click the button #%1 in order to continue...<br><br>(There will be no such message box in the final version of this application)").arg(QString::number(rnd + 1)), tr("(1)"), tr("(2)"), tr("(3)"), qrand() % 3);
		if(rnd != val) { close(); qApp->exit(-1); return; }
	}

	//Make sure this CPU can run x264 (requires MMX + MMXEXT/iSSE to run x264 with ASM enabled, additionally requires SSE1 for most x264 builds)
	if(!(m_cpuFeatures->mmx && m_cpuFeatures->mmx2))
	{
		QMessageBox::critical(this, tr("Unsupported CPU"), tr("<nobr>Sorry, but this machine is <b>not</b> physically capable of running x264 (with assembly).<br>Please get a CPU that supports at least the MMX and MMXEXT instruction sets!</nobr>"), tr("Quit"));
		qFatal("System does not support MMX and MMXEXT, x264 will not work !!!");
		close(); qApp->exit(-1); return;
	}
	else if(!(m_cpuFeatures->mmx && m_cpuFeatures->sse))
	{
		qWarning("WARNING: System does not support SSE1, most x264 builds will not work !!!\n");
		int val = QMessageBox::warning(this, tr("Unsupported CPU"), tr("<nobr>It appears that this machine does <b>not</b> support the SSE1 instruction set.<br>Thus most builds of x264 will <b>not</b> run on this computer at all.<br><br>Please get a CPU that supports the MMX and SSE1 instruction sets!</nobr>"), tr("Quit"), tr("Ignore"));
		if(val != 1) { close(); qApp->exit(-1); return; }
	}

	//Skip version check (not recommended!)
	if(qApp->arguments().contains("--skip-x264-version-check", Qt::CaseInsensitive))
	{
		qWarning("x264 version check disabled, you have been warned!\n");
		m_skipVersionTest = true;
	}
	
	//Don't abort encoding process on timeout (not recommended!)
	if(qApp->arguments().contains("--no-deadlock-detection", Qt::CaseInsensitive))
	{
		qWarning("Deadlock detection disabled, you have been warned!\n");
		m_abortOnTimeout = false;
	}

	//Check for Avisynth support
	if(!qApp->arguments().contains("--skip-avisynth-check", Qt::CaseInsensitive))
	{
		qDebug("[Check for Avisynth support]");
		volatile double avisynthVersion = 0.0;
		const int result = AvisynthCheckThread::detect(&avisynthVersion);
		if(result < 0)
		{
			QString text = tr("A critical error was encountered while checking your Avisynth version.").append("<br>");
			text += tr("This is most likely caused by an erroneous Avisynth Plugin, please try to clean your Plugins folder!").append("<br>");
			text += tr("We suggest to move all .dll and .avsi files out of your Avisynth Plugins folder and try again.");
			int val = QMessageBox::critical(this, tr("Avisynth Error"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
			if(val != 1) { close(); qApp->exit(-1); return; }
		}
		if((!result) || (avisynthVersion < 2.5))
		{
			int val = QMessageBox::warning(this, tr("Avisynth Missing"), tr("<nobr>It appears that Avisynth is <b>not</b> currently installed on your computer.<br>Therefore Avisynth (.avs) input will <b>not</b> be working at all!<br><br>Please download and install Avisynth:<br><a href=\"http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/\">http://sourceforge.net/projects/avisynth2/files/AviSynth 2.5/</a></nobr>").replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
			if(val != 1) { close(); qApp->exit(-1); return; }
		}
		qDebug("");
	}

	//Check for Vapoursynth support
	if(!qApp->arguments().contains("--skip-vapoursynth-check", Qt::CaseInsensitive))
	{
		qDebug("[Check for VapourSynth support]");
		const QString vapursynthPath = getVapoursynthLocation();
		if(!vapursynthPath.isEmpty())
		{
			QFile *vpsExePath = new QFile(QString("%1/core/vspipe.exe").arg(vapursynthPath));
			QFile *vpsDllPath = new QFile(QString("%1/core/vapoursynth.dll").arg(vapursynthPath));
			qDebug("VapourSynth EXE: %s", vpsExePath->fileName().toUtf8().constData());
			qDebug("VapourSynth DLL: %s", vpsDllPath->fileName().toUtf8().constData());
			if(checkVapourSynth(vpsExePath, vpsDllPath))
			{
				qDebug("VapourSynth support enabled.");
				m_vapoursynthPath = QFileInfo(vpsExePath->fileName()).canonicalPath();
				m_toolsList << vpsExePath;
				m_toolsList << vpsDllPath;
			}
			else
			{
				qDebug("VapourSynth not avilable -> disable Vapousynth support!");
				X264_DELETE(vpsExePath);
				X264_DELETE(vpsDllPath);
				int val = QMessageBox::warning(this, tr("VapourSynth Missing"), tr("<nobr>It appears that VapourSynth is <b>not</b> currently installed on your computer.<br>Therefore VapourSynth (.vpy) input will <b>not</b> be working at all!<br><br>Please download and install VapourSynth (r19 or later) here:<br><a href=\"http://www.vapoursynth.com/\">http://www.vapoursynth.com/</a></nobr>").replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
				if(val != 1) { close(); qApp->exit(-1); return; }
			}
		}
		qDebug("");
	}

	//Check for expiration
	if(x264_version_date().addMonths(6) < QDate::currentDate())
	{
		QMessageBox msgBox(this);
		msgBox.setIconPixmap(QIcon(":/images/update.png").pixmap(56,56));
		msgBox.setWindowTitle(tr("Update Notification"));
		msgBox.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
		msgBox.setText(tr("<nobr><tt>Your version of 'Simple x264 Launcher' is more than 6 months old!<br><br>Please download the most recent version from the official web-site at:<br><a href=\"%1\">%1</a><br></tt></nobr>").replace("-", "&minus;").arg(update_url));
		QPushButton *btn1 = msgBox.addButton(tr("Discard"), QMessageBox::NoRole);
		QPushButton *btn2 = msgBox.addButton(tr("Discard"), QMessageBox::AcceptRole);
		btn1->setEnabled(false);
		btn2->setVisible(false);
		QTimer::singleShot(5000, btn1, SLOT(hide()));
		QTimer::singleShot(5000, btn2, SLOT(show()));
		msgBox.exec();
	}

	//Add files from command-line
	bool bAddFile = false;
	QStringList files, args = qApp->arguments();
	while(!args.isEmpty())
	{
		QString current = args.takeFirst();
		if(!bAddFile)
		{
			bAddFile = (current.compare("--add", Qt::CaseInsensitive) == 0);
			continue;
		}
		if((!current.startsWith("--")) && QFileInfo(current).exists() && QFileInfo(current).isFile())
		{
			files << QFileInfo(current).canonicalFilePath();
		}
	}
	if(files.count() > 0)
	{
		createJobMultiple(files);
	}

	//Enable drag&drop support for this window, required for Qt v4.8.4+
	setAcceptDrops(true);
}

/*
 * Update the label position
 */
void MainWindow::updateLabelPos(void)
{
	const QWidget *const viewPort = jobsView->viewport();
	m_label->setGeometry(0, 0, viewPort->width(), viewPort->height());
}

/*
 * Copy the complete log to the clipboard
 */
void MainWindow::copyLogToClipboard(bool checked)
{
	qDebug("copyLogToClipboard");
	
	if(LogFileModel *log = dynamic_cast<LogFileModel*>(logView->model()))
	{
		log->copyToClipboard();
		MessageBeep(MB_ICONINFORMATION);
	}
}

/*
 * Process the dropped files
 */
void MainWindow::handleDroppedFiles(void)
{
	qDebug("MainWindow::handleDroppedFiles");
	if(m_droppedFiles)
	{
		QStringList droppedFiles(*m_droppedFiles);
		m_droppedFiles->clear();
		createJobMultiple(droppedFiles);
	}
	qDebug("Leave from MainWindow::handleDroppedFiles!");
}

void MainWindow::instanceCreated(DWORD pid)
{
	qDebug("Notification from other instance (PID=0x%X) received!", pid);
	
	FLASHWINFO flashWinInfo;
	memset(&flashWinInfo, 0, sizeof(FLASHWINFO));
	flashWinInfo.cbSize = sizeof(FLASHWINFO);
	flashWinInfo.hwnd = this->winId();
	flashWinInfo.dwFlags = FLASHW_ALL;
	flashWinInfo.dwTimeout = 125;
	flashWinInfo.uCount = 5;

	SwitchToThisWindow(this->winId(), TRUE);
	SetForegroundWindow(this->winId());
	qApp->processEvents();
	FlashWindowEx(&flashWinInfo);
}

///////////////////////////////////////////////////////////////////////////////
// Event functions
///////////////////////////////////////////////////////////////////////////////

/*
 * Window shown event
 */
void MainWindow::showEvent(QShowEvent *e)
{
	QMainWindow::showEvent(e);

	if(m_firstShow)
	{
		m_firstShow = false;
		QTimer::singleShot(0, this, SLOT(init()));
	}
}

/*
 * Window close event
 */
void MainWindow::closeEvent(QCloseEvent *e)
{
	if(countRunningJobs() > 0)
	{
		e->ignore();
		QMessageBox::warning(this, tr("Jobs Are Running"), tr("Sorry, can not exit while there still are running jobs!"));
		return;
	}
	
	if(countPendingJobs() > 0)
	{
		int ret = QMessageBox::question(this, tr("Jobs Are Pending"), tr("Do you really want to quit and discard the pending jobs?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if(ret != QMessageBox::Yes)
		{
			e->ignore();
			return;
		}
	}

	while(m_jobList->rowCount(QModelIndex()) > 0)
	{
		qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
		if(!m_jobList->deleteJob(m_jobList->index(0, 0, QModelIndex())))
		{
			e->ignore();
			QMessageBox::warning(this, tr("Failed To Exit"), tr("Sorry, at least one job could not be deleted!"));
			return;
		}
	}

	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
	QMainWindow::closeEvent(e);
}

/*
 * Window resize event
 */
void MainWindow::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	updateLabelPos();
}

/*
 * Event filter
 */
bool MainWindow::eventFilter(QObject *o, QEvent *e)
{
	if((o == labelBuildDate) && (e->type() == QEvent::MouseButtonPress))
	{
		QTimer::singleShot(0, this, SLOT(showAbout()));
		return true;
	}
	return false;
}

/*
 * Win32 message filter
 */
bool MainWindow::winEvent(MSG *message, long *result)
{
	return WinSevenTaskbar::handleWinEvent(message, result);
}

/*
 * File dragged over window
 */
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	bool accept[2] = {false, false};

	foreach(const QString &fmt, event->mimeData()->formats())
	{
		accept[0] = accept[0] || fmt.contains("text/uri-list", Qt::CaseInsensitive);
		accept[1] = accept[1] || fmt.contains("FileNameW", Qt::CaseInsensitive);
	}

	if(accept[0] && accept[1])
	{
		event->acceptProposedAction();
	}
}

/*
 * File dropped onto window
 */
void MainWindow::dropEvent(QDropEvent *event)
{
	QStringList droppedFiles;
	QList<QUrl> urls = event->mimeData()->urls();

	while(!urls.isEmpty())
	{
		QUrl currentUrl = urls.takeFirst();
		QFileInfo file(currentUrl.toLocalFile());
		if(file.exists() && file.isFile())
		{
			qDebug("MainWindow::dropEvent: %s", file.canonicalFilePath().toUtf8().constData());
			droppedFiles << file.canonicalFilePath();
		}
	}
	
	if(droppedFiles.count() > 0)
	{
		if(!m_droppedFiles)
		{
			m_droppedFiles = new QStringList();
		}
		m_droppedFiles->append(droppedFiles);
		m_droppedFiles->sort();
		QTimer::singleShot(0, this, SLOT(handleDroppedFiles()));
	}
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

/*
 * Creates a new job
 */
bool MainWindow::createJob(QString &sourceFileName, QString &outputFileName, OptionsModel *options, bool &runImmediately, const bool restart, int fileNo, int fileTotal, bool *applyToAll)
{
	bool okay = false;
	AddJobDialog *addDialog = new AddJobDialog(this, options, m_recentlyUsed, m_cpuFeatures->x64, m_preferences->use10BitEncoding(), m_preferences->saveToSourcePath());

	addDialog->setRunImmediately(runImmediately);
	if(!sourceFileName.isEmpty()) addDialog->setSourceFile(sourceFileName);
	if(!outputFileName.isEmpty()) addDialog->setOutputFile(outputFileName);
	if(restart) addDialog->setWindowTitle(tr("Restart Job"));

	const bool multiFile = (fileNo >= 0) && (fileTotal > 1);
	if(multiFile)
	{
		addDialog->setSourceEditable(false);
		addDialog->setWindowTitle(addDialog->windowTitle().append(tr(" (File %1 of %2)").arg(QString::number(fileNo+1), QString::number(fileTotal))));
		addDialog->setApplyToAllVisible(applyToAll);
	}

	if(addDialog->exec() == QDialog::Accepted)
	{
		sourceFileName = addDialog->sourceFile();
		outputFileName = addDialog->outputFile();
		runImmediately = addDialog->runImmediately();
		if(applyToAll)
		{
			*applyToAll = addDialog->applyToAll();
		}
		okay = true;
	}

	X264_DELETE(addDialog);
	return okay;
}

/*
 * Creates a new job from *multiple* files
 */
bool MainWindow::createJobMultiple(const QStringList &filePathIn)
{
	QStringList::ConstIterator iter;
	bool applyToAll = false, runImmediately = false;
	int counter = 0;

	//Add files individually
	for(iter = filePathIn.constBegin(); (iter != filePathIn.constEnd()) && (!applyToAll); iter++)
	{
		runImmediately = (countRunningJobs() < (m_preferences->autoRunNextJob() ? m_preferences->maxRunningJobCount() : 1));
		QString sourceFileName(*iter), outputFileName;
		if(createJob(sourceFileName, outputFileName, m_options, runImmediately, false, counter++, filePathIn.count(), &applyToAll))
		{
			if(appendJob(sourceFileName, outputFileName, m_options, runImmediately))
			{
				continue;
			}
		}
		return false;
	}

	//Add remaining files
	while(applyToAll && (iter != filePathIn.constEnd()))
	{
		const bool runImmediatelyTmp = runImmediately && (countRunningJobs() < (m_preferences->autoRunNextJob() ? m_preferences->maxRunningJobCount() : 1));
		const QString sourceFileName = *iter;
		const QString outputFileName = AddJobDialog::generateOutputFileName(sourceFileName, m_recentlyUsed->outputDirectory(), m_recentlyUsed->filterIndex(), m_preferences->saveToSourcePath());
		if(!appendJob(sourceFileName, outputFileName, m_options, runImmediatelyTmp))
		{
			return false;
		}
		iter++;
	}

	return true;
}


/*
 * Append a new job
 */
bool MainWindow::appendJob(const QString &sourceFileName, const QString &outputFileName, OptionsModel *options, const bool runImmediately)
{
	bool okay = false;
	
	EncodeThread *thrd = new EncodeThread
	(
		sourceFileName,
		outputFileName,
		options,
		QString("%1/toolset").arg(m_appDir),
		m_vapoursynthPath,
		m_cpuFeatures->x64,
		m_preferences->use10BitEncoding(),
		m_cpuFeatures->x64 && m_preferences->useAvisyth64Bit(),
		m_skipVersionTest,
		m_preferences->processPriority(),
		m_abortOnTimeout
	);

	QModelIndex newIndex = m_jobList->insertJob(thrd);

	if(newIndex.isValid())
	{
		if(runImmediately)
		{
			jobsView->selectRow(newIndex.row());
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			m_jobList->startJob(newIndex);
		}

		okay = true;
	}

	m_label->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
	return okay;
}

/*
 * Jobs that are not completed (or failed, or aborted) yet
 */
unsigned int MainWindow::countPendingJobs(void)
{
	unsigned int count = 0;
	const int rows = m_jobList->rowCount(QModelIndex());

	for(int i = 0; i < rows; i++)
	{
		JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status != JobStatus_Completed && status != JobStatus_Aborted && status != JobStatus_Failed)
		{
			count++;
		}
	}

	return count;
}

/*
 * Jobs that are still active, i.e. not terminated or enqueued
 */
unsigned int MainWindow::countRunningJobs(void)
{
	unsigned int count = 0;
	const int rows = m_jobList->rowCount(QModelIndex());

	for(int i = 0; i < rows; i++)
	{
		JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status != JobStatus_Completed && status != JobStatus_Aborted && status != JobStatus_Failed && status != JobStatus_Enqueued)
		{
			count++;
		}
	}

	return count;
}

/*
 * Update all buttons with respect to current job status
 */
void MainWindow::updateButtons(JobStatus status)
{
	qDebug("MainWindow::updateButtons(void)");

	buttonStartJob->setEnabled(status == JobStatus_Enqueued);
	buttonAbortJob->setEnabled(status == JobStatus_Indexing || status == JobStatus_Running || status == JobStatus_Running_Pass1 || status == JobStatus_Running_Pass2 || status == JobStatus_Paused);
	buttonPauseJob->setEnabled(status == JobStatus_Indexing || status == JobStatus_Running || status == JobStatus_Paused || status == JobStatus_Running_Pass1 || status == JobStatus_Running_Pass2);
	buttonPauseJob->setChecked(status == JobStatus_Paused || status == JobStatus_Pausing);

	actionJob_Delete->setEnabled(status == JobStatus_Completed || status == JobStatus_Aborted || status == JobStatus_Failed || status == JobStatus_Enqueued);
	actionJob_Restart->setEnabled(status == JobStatus_Completed || status == JobStatus_Aborted || status == JobStatus_Failed || status == JobStatus_Enqueued);
	actionJob_Browse->setEnabled(status == JobStatus_Completed);

	actionJob_Start->setEnabled(buttonStartJob->isEnabled());
	actionJob_Abort->setEnabled(buttonAbortJob->isEnabled());
	actionJob_Pause->setEnabled(buttonPauseJob->isEnabled());
	actionJob_Pause->setChecked(buttonPauseJob->isChecked());

	editDetails->setEnabled(status != JobStatus_Paused);
}

/*
 * Update the taskbar with current job status
 */
void MainWindow::updateTaskbar(JobStatus status, const QIcon &icon)
{
	qDebug("MainWindow::updateTaskbar(void)");

	switch(status)
	{
	case JobStatus_Undefined:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarNoState);
		break;
	case JobStatus_Aborting:
	case JobStatus_Starting:
	case JobStatus_Pausing:
	case JobStatus_Resuming:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarIndeterminateState);
		break;
	case JobStatus_Aborted:
	case JobStatus_Failed:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarErrorState);
		break;
	case JobStatus_Paused:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarPausedState);
		break;
	default:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarNormalState);
		break;
	}

	switch(status)
	{
	case JobStatus_Aborting:
	case JobStatus_Starting:
	case JobStatus_Pausing:
	case JobStatus_Resuming:
		break;
	default:
		WinSevenTaskbar::setTaskbarProgress(this, progressBar->value(), progressBar->maximum());
		break;
	}

	WinSevenTaskbar::setOverlayIcon(this, icon.isNull() ? NULL : &icon);
}

/*
 * Read Vapursynth location from registry
 */
QString MainWindow::getVapoursynthLocation(void)
{
	QString vapoursynthPath;
	static const wchar_t *VPS_REG_KEY = L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VapourSynth_is1";
	HKEY hKey = NULL;

	if(RegOpenKey(HKEY_LOCAL_MACHINE, VPS_REG_KEY, &hKey) == ERROR_SUCCESS)
	{
		const size_t DATA_LEN = 2048;
		wchar_t data[DATA_LEN];
		DWORD type = REG_NONE, size = sizeof(wchar_t) * DATA_LEN;
		if(RegQueryValueEx(hKey, L"InstallLocation", NULL, &type, ((BYTE*)&data[0]), &size) == ERROR_SUCCESS)
		{
			if((type == REG_SZ) || (type == REG_EXPAND_SZ))
			{
				vapoursynthPath = QDir::fromNativeSeparators(QString::fromUtf16((const ushort*)&data[0]));
				while(vapoursynthPath.endsWith("/")) { vapoursynthPath.chop(1); }
				qDebug("Vapoursynth location: %s", vapoursynthPath.toUtf8().constData());
			}
		}
		RegCloseKey(hKey);
	}
	else
	{
		qWarning("Vapoursynth registry key not found -> not installed!");
	}

	return vapoursynthPath;
}

bool MainWindow::checkVapourSynth(QFile *vpsExePath, QFile *vpsDllPath)
{
	bool okay = false;
	static const char *VSSCRIPT_ENTRY = "_vsscript_init@0";

	if(vpsExePath->open(QIODevice::ReadOnly) && vpsDllPath->open(QIODevice::ReadOnly))
	{
		DWORD binaryType;
		if(GetBinaryType(QWCHAR(QDir::toNativeSeparators(vpsExePath->fileName())), &binaryType))
		{
			okay = (binaryType == SCS_32BIT_BINARY || binaryType == SCS_64BIT_BINARY);
		}
	}

	if(okay)
	{
		qDebug("VapourSynth binaries found -> load VSSCRIPT.DLL");
		QLibrary vspLibrary("vsscript.dll");
		if(okay = vspLibrary.load())
		{
			if(!(okay = (vspLibrary.resolve(VSSCRIPT_ENTRY) != NULL)))
			{
				qWarning("Entrypoint '%s' not found in VSSCRIPT.DLL !!!", VSSCRIPT_ENTRY);
			}
		}
		else
		{
			qWarning("Failed to load VSSCRIPT.DLL !!!");
		}
	}
	else
	{
		qWarning("VapourSynth binaries could not be found !!!");
	}

	return okay;
}
