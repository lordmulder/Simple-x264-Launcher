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

#include "win_main.h"
#include "uic_win_main.h"

#include "global.h"
#include "model_status.h"
#include "model_jobList.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_recently.h"
#include "thread_avisynth.h"
#include "thread_vapoursynth.h"
#include "thread_encode.h"
#include "taskbar7.h"
#include "win_addJob.h"
#include "win_preferences.h"
#include "win_updater.h"
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

#include <ctime>

const char *home_url = "http://muldersoft.com/";
const char *update_url = "https://github.com/lordmulder/Simple-x264-Launcher/releases/latest";
const char *tpl_last = "<LAST_USED>";

#define SET_FONT_BOLD(WIDGET,BOLD) do { QFont _font = WIDGET->font(); _font.setBold(BOLD); WIDGET->setFont(_font); } while(0)
#define SET_TEXT_COLOR(WIDGET,COLOR) do { QPalette _palette = WIDGET->palette(); _palette.setColor(QPalette::WindowText, (COLOR)); _palette.setColor(QPalette::Text, (COLOR)); WIDGET->setPalette(_palette); } while(0)
#define LINK(URL) "<a href=\"" URL "\">" URL "</a>"
#define INIT_ERROR_EXIT() do { m_initialized = true; close(); qApp->exit(-1); return; } while(0)

//static int exceptionFilter(_EXCEPTION_RECORD *dst, _EXCEPTION_POINTERS *src) { memcpy(dst, src->ExceptionRecord, sizeof(_EXCEPTION_RECORD)); return EXCEPTION_EXECUTE_HANDLER; }

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
	m_firstShow(true),
	m_initialized(false),
	ui(new Ui::MainWindow())
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
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
	//m_ipcThread = new IPCThread();
	//connect(m_ipcThread, SIGNAL(instanceCreated(unsigned int)), this, SLOT(instanceCreated(unsigned int)), Qt::QueuedConnection);

	//Freeze minimum size
	setMinimumSize(size());
	ui->splitter->setSizes(QList<int>() << 16 << 196);

	//Update title
	ui->labelBuildDate->setText(tr("Built on %1 at %2").arg(x264_version_date().toString(Qt::ISODate), QString::fromLatin1(x264_version_time())));
	ui->labelBuildDate->installEventFilter(this);
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
	ui->jobsView->setModel(m_jobList);
	
	//Setup view
	ui->jobsView->horizontalHeader()->setSectionHidden(3, true);
	ui->jobsView->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
	ui->jobsView->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
	ui->jobsView->horizontalHeader()->setResizeMode(2, QHeaderView::Fixed);
	ui->jobsView->horizontalHeader()->resizeSection(1, 150);
	ui->jobsView->horizontalHeader()->resizeSection(2, 90);
	ui->jobsView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	connect(ui->jobsView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(jobSelected(QModelIndex, QModelIndex)));

	//Create context menu
	QAction *actionClipboard = new QAction(QIcon(":/buttons/page_paste.png"), tr("Copy to Clipboard"), ui->logView);
	actionClipboard->setEnabled(false);
	ui->logView->addAction(actionClipboard);
	connect(actionClipboard, SIGNAL(triggered(bool)), this, SLOT(copyLogToClipboard(bool)));
	ui->jobsView->addActions(ui->menuJob->actions());

	//Enable buttons
	connect(ui->buttonAddJob, SIGNAL(clicked()), this, SLOT(addButtonPressed()));
	connect(ui->buttonStartJob, SIGNAL(clicked()), this, SLOT(startButtonPressed()));
	connect(ui->buttonAbortJob, SIGNAL(clicked()), this, SLOT(abortButtonPressed()));
	connect(ui->buttonPauseJob, SIGNAL(toggled(bool)), this, SLOT(pauseButtonPressed(bool)));
	connect(ui->actionJob_Delete, SIGNAL(triggered()), this, SLOT(deleteButtonPressed()));
	connect(ui->actionJob_Restart, SIGNAL(triggered()), this, SLOT(restartButtonPressed()));
	connect(ui->actionJob_Browse, SIGNAL(triggered()), this, SLOT(browseButtonPressed()));

	//Enable menu
	connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(openActionTriggered()));
	connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
	connect(ui->actionWebMulder, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebX264, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebKomisar, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebVideoLAN, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebJEEB, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebAvisynth32, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebAvisynth64, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebVapourSynth, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebVapourSynthDocs, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebWiki, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebBluRay, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebAvsWiki, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebSecret, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionWebSupport, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(ui->actionPreferences, SIGNAL(triggered()), this, SLOT(showPreferences()));
	connect(ui->actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(checkUpdates()));

	//Create floating label
	m_label = new QLabel(ui->jobsView->viewport());
	m_label->setText(tr("No job created yet. Please click the 'Add New Job' button!"));
	m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	SET_TEXT_COLOR(m_label, Qt::darkGray);
	SET_FONT_BOLD(m_label, true);
	m_label->setVisible(true);
	m_label->setContextMenuPolicy(Qt::ActionsContextMenu);
	m_label->addActions(ui->jobsView->actions());
	connect(ui->splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(updateLabelPos()));
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

	/*
	if(m_ipcThread->isRunning())
	{
		m_ipcThread->setAbort();
		if(!m_ipcThread->wait(5000))
		{
			m_ipcThread->terminate();
			m_ipcThread->wait();
		}
	}
	*/

	//X264_DELETE(m_ipcThread);
	X264_DELETE(m_preferences);
	X264_DELETE(m_recentlyUsed);
	VapourSynthCheckThread::unload();
	AvisynthCheckThread::unload();

	delete ui;
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
	m_jobList->startJob(ui->jobsView->currentIndex());
}

/*
 * The "abort" button was clicked
 */
void MainWindow::abortButtonPressed(void)
{
	m_jobList->abortJob(ui->jobsView->currentIndex());
}

/*
 * The "delete" button was clicked
 */
void MainWindow::deleteButtonPressed(void)
{
	m_jobList->deleteJob(ui->jobsView->currentIndex());
	m_label->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
}

/*
 * The "browse" button was clicked
 */
void MainWindow::browseButtonPressed(void)
{
	QString outputFile = m_jobList->getJobOutputFile(ui->jobsView->currentIndex());
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
		m_jobList->pauseJob(ui->jobsView->currentIndex());
	}
	else
	{
		m_jobList->resumeJob(ui->jobsView->currentIndex());
	}
}

/*
 * The "restart" button was clicked
 */
void MainWindow::restartButtonPressed(void)
{
	const QModelIndex index = ui->jobsView->currentIndex();
	const OptionsModel *options = m_jobList->getJobOptions(index);
	QString sourceFileName = m_jobList->getJobSourceFile(index);
	QString outputFileName = m_jobList->getJobOutputFile(index);

	if((options) && (!sourceFileName.isEmpty()) && (!outputFileName.isEmpty()))
	{
		bool runImmediately = (countRunningJobs() < (m_preferences->autoRunNextJob() ? m_preferences->maxRunningJobCount() : 1)); //bool runImmediately = true;
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
	
	if(ui->logView->model())
	{
		disconnect(ui->logView->model(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(jobLogExtended(QModelIndex, int, int)));
	}
	
	if(current.isValid())
	{
		ui->logView->setModel(m_jobList->getLogFile(current));
		connect(ui->logView->model(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(jobLogExtended(QModelIndex, int, int)));
		ui->logView->actions().first()->setEnabled(true);
		QTimer::singleShot(0, ui->logView, SLOT(scrollToBottom()));

		ui->progressBar->setValue(m_jobList->getJobProgress(current));
		ui->editDetails->setText(m_jobList->data(m_jobList->index(current.row(), 3, QModelIndex()), Qt::DisplayRole).toString());
		updateButtons(m_jobList->getJobStatus(current));
		updateTaskbar(m_jobList->getJobStatus(current), m_jobList->data(m_jobList->index(current.row(), 0, QModelIndex()), Qt::DecorationRole).value<QIcon>());
	}
	else
	{
		ui->logView->setModel(NULL);
		ui->logView->actions().first()->setEnabled(false);
		ui->progressBar->setValue(0);
		ui->editDetails->clear();
		updateButtons(JobStatus_Undefined);
		updateTaskbar(JobStatus_Undefined, QIcon());
	}

	ui->progressBar->repaint();
}

/*
 * Handle update of job info (status, progress, details, etc)
 */
void MainWindow::jobChangedData(const QModelIndex &topLeft, const  QModelIndex &bottomRight)
{
	int selected = ui->jobsView->currentIndex().row();
	
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
				ui->progressBar->setValue(m_jobList->getJobProgress(m_jobList->index(i, 0, QModelIndex())));
				WinSevenTaskbar::setTaskbarProgress(this, ui->progressBar->value(), ui->progressBar->maximum());
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
				ui->editDetails->setText(m_jobList->data(m_jobList->index(i, 3, QModelIndex()), Qt::DisplayRole).toString());
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
	QTimer::singleShot(0, ui->logView, SLOT(scrollToBottom()));
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
	aboutBox.addButton(tr("About VPY"), QMessageBox::NoRole);
	aboutBox.addButton(tr("About Qt"), QMessageBox::NoRole);
	aboutBox.setEscapeButton(aboutBox.addButton(tr("Close"), QMessageBox::NoRole));
		
	forever
	{
		x264_beep(x264_beep_info);
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
				x264_beep(x264_beep_info);
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
				text2 += tr("Read the <a href=\"%1\">guide</a> to get started and use the <a href=\"%2\">support forum</a> for help!<br></tt></nobr>").arg("http://avisynth.nl/index.php/First_script", "http://forum.doom9.org/forumdisplay.php?f=33");

				QMessageBox x264Box(this);
				x264Box.setIconPixmap(QIcon(":/images/avisynth.png").pixmap(48,67));
				x264Box.setWindowTitle(tr("About Avisynth"));
				x264Box.setText(text2.replace("-", "&minus;"));
				x264Box.setEscapeButton(x264Box.addButton(tr("Close"), QMessageBox::NoRole));
				x264_beep(x264_beep_info);
				x264Box.exec();
			}
			break;
		case 2:
			{
				QString text2;
				text2 += tr("<nobr><tt>VapourSynth - application for video manipulation based on Python.<br>");
				text2 += tr("Copyright (c) 2012 Fredrik Mellbin.<br>");
				text2 += tr("Released under the terms of the GNU Lesser General Public.<br><br>");
				text2 += tr("Please visit the web-site <a href=\"%1\">%1</a> for more information.<br>").arg("http://www.vapoursynth.com/");
				text2 += tr("Read the <a href=\"%1\">documentation</a> to get started and use the <a href=\"%2\">support forum</a> for help!<br></tt></nobr>").arg("http://www.vapoursynth.com/doc/", "http://forum.doom9.org/showthread.php?t=165771");

				QMessageBox x264Box(this);
				x264Box.setIconPixmap(QIcon(":/images/python.png").pixmap(48,48));
				x264Box.setWindowTitle(tr("About VapourSynth"));
				x264Box.setText(text2.replace("-", "&minus;"));
				x264Box.setEscapeButton(x264Box.addButton(tr("Close"), QMessageBox::NoRole));
				x264_beep(x264_beep_info);
				x264Box.exec();
			}
			break;
		case 3:
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
	if(QObject::sender() == ui->actionWebMulder)          QDesktopServices::openUrl(QUrl(home_url));
	if(QObject::sender() == ui->actionWebX264)            QDesktopServices::openUrl(QUrl("http://www.x264.com/"));
	if(QObject::sender() == ui->actionWebKomisar)         QDesktopServices::openUrl(QUrl("http://komisar.gin.by/"));
	if(QObject::sender() == ui->actionWebVideoLAN)        QDesktopServices::openUrl(QUrl("http://download.videolan.org/pub/x264/binaries/"));
	if(QObject::sender() == ui->actionWebJEEB)            QDesktopServices::openUrl(QUrl("http://x264.fushizen.eu/"));
	if(QObject::sender() == ui->actionWebAvisynth32)      QDesktopServices::openUrl(QUrl("http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/"));
	if(QObject::sender() == ui->actionWebAvisynth64)      QDesktopServices::openUrl(QUrl("http://code.google.com/p/avisynth64/downloads/list"));
	if(QObject::sender() == ui->actionWebVapourSynth)     QDesktopServices::openUrl(QUrl("http://www.vapoursynth.com/"));
	if(QObject::sender() == ui->actionWebVapourSynthDocs) QDesktopServices::openUrl(QUrl("http://www.vapoursynth.com/doc/"));
	if(QObject::sender() == ui->actionWebWiki)            QDesktopServices::openUrl(QUrl("http://mewiki.project357.com/wiki/X264_Settings"));
	if(QObject::sender() == ui->actionWebBluRay)          QDesktopServices::openUrl(QUrl("http://www.x264bluray.com/"));
	if(QObject::sender() == ui->actionWebAvsWiki)         QDesktopServices::openUrl(QUrl("http://avisynth.nl/index.php/Main_Page#Usage"));
	if(QObject::sender() == ui->actionWebSupport)         QDesktopServices::openUrl(QUrl("http://forum.doom9.org/showthread.php?t=144140"));
	if(QObject::sender() == ui->actionWebSecret)          QDesktopServices::openUrl(QUrl("http://www.youtube.com/watch_popup?v=AXIeHY-OYNI"));
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

	int startIdx= ui->jobsView->currentIndex().isValid() ? qBound(0, ui->jobsView->currentIndex().row(), rows-1) : 0;

	for(int i = 0; i < rows; i++)
	{
		int currentIdx = (i + startIdx) % rows;
		JobStatus status = m_jobList->getJobStatus(m_jobList->index(currentIdx, 0, QModelIndex()));
		if(status == JobStatus_Enqueued)
		{
			if(m_jobList->startJob(m_jobList->index(currentIdx, 0, QModelIndex())))
			{
				ui->jobsView->selectRow(currentIdx);
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
	x264_play_sound(IDR_WAVE1, false);
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
		x264_play_sound(((i < iTimeout) ? IDR_WAVE2 : IDR_WAVE3), false);
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
	/*
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
			INIT_ERROR_EXIT();
		}
	}
	*/

	//Check all binaries
	while(!binaries.isEmpty())
	{
		qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
		QString current = binaries.takeFirst();
		QFile *file = new QFile(QString("%1/toolset/%2").arg(m_appDir, current));
		if(file->open(QIODevice::ReadOnly))
		{
			if(!x264_is_executable(file->fileName()))
			{
				QMessageBox::critical(this, tr("Invalid File!"), tr("<nobr>At least on required tool is not a valid Win32 or Win64 binary:<br><tt style=\"whitespace:nowrap\">%1</tt><br><br>Please re-install the program in order to fix the problem!</nobr>").arg(QDir::toNativeSeparators(QString("%1/toolset/%2").arg(m_appDir, current))).replace("-", "&minus;"));
				qFatal(QString("Binary is invalid: %1/toolset/%2").arg(m_appDir, current).toLatin1().constData());
				INIT_ERROR_EXIT();
			}
			m_toolsList << file;
		}
		else
		{
			X264_DELETE(file);
			QMessageBox::critical(this, tr("File Not Found!"), tr("<nobr>At least on required tool could not be found:<br><tt style=\"whitespace:nowrap\">%1</tt><br><br>Please re-install the program in order to fix the problem!</nobr>").arg(QDir::toNativeSeparators(QString("%1/toolset/%2").arg(m_appDir, current))).replace("-", "&minus;"));
			qFatal(QString("Binary not found: %1/toolset/%2").arg(m_appDir, current).toLatin1().constData());
			INIT_ERROR_EXIT();
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
			if(val != 1) INIT_ERROR_EXIT();
		}
	}

	//Pre-release popup
	if(x264_is_prerelease())
	{
		qsrand(time(NULL)); int rnd = qrand() % 3;
		int val = QMessageBox::information(this, tr("Pre-Release Version"), tr("Note: This is a pre-release version. Please do NOT use for production!<br>Click the button #%1 in order to continue...<br><br>(There will be no such message box in the final version of this application)").arg(QString::number(rnd + 1)), tr("(1)"), tr("(2)"), tr("(3)"), qrand() % 3);
		if(rnd != val) INIT_ERROR_EXIT();
	}

	//Make sure this CPU can run x264 (requires MMX + MMXEXT/iSSE to run x264 with ASM enabled, additionally requires SSE1 for most x264 builds)
	if(!(m_cpuFeatures->mmx && m_cpuFeatures->mmx2))
	{
		QMessageBox::critical(this, tr("Unsupported CPU"), tr("<nobr>Sorry, but this machine is <b>not</b> physically capable of running x264 (with assembly).<br>Please get a CPU that supports at least the MMX and MMXEXT instruction sets!</nobr>"), tr("Quit"));
		qFatal("System does not support MMX and MMXEXT, x264 will not work !!!");
		INIT_ERROR_EXIT();
	}
	else if(!(m_cpuFeatures->mmx && m_cpuFeatures->sse))
	{
		qWarning("WARNING: System does not support SSE1, most x264 builds will not work !!!\n");
		int val = QMessageBox::warning(this, tr("Unsupported CPU"), tr("<nobr>It appears that this machine does <b>not</b> support the SSE1 instruction set.<br>Thus most builds of x264 will <b>not</b> run on this computer at all.<br><br>Please get a CPU that supports the MMX and SSE1 instruction sets!</nobr>"), tr("Quit"), tr("Ignore"));
		if(val != 1) INIT_ERROR_EXIT();
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
			if(val != 1) INIT_ERROR_EXIT();
		}
		if((!result) || (avisynthVersion < 2.5))
		{
			if(!m_preferences->disableWarnings())
			{
				QString text = tr("It appears that Avisynth is <b>not</b> currently installed on your computer.<br>Therefore Avisynth (.avs) input will <b>not</b> be working at all!").append("<br><br>");
				text += tr("Please download and install Avisynth:").append("<br>").append(LINK("http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/"));
				int val = QMessageBox::warning(this, tr("Avisynth Missing"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
				if(val != 1) INIT_ERROR_EXIT();
			}
		}
		qDebug(" ");
	}

	//Check for VapourSynth support
	if(!qApp->arguments().contains("--skip-vapoursynth-check", Qt::CaseInsensitive))
	{
		qDebug("[Check for VapourSynth support]");
		volatile double avisynthVersion = 0.0;
		const int result = VapourSynthCheckThread::detect(m_vapoursynthPath);
		if(result < 0)
		{
			QString text = tr("A critical error was encountered while checking your VapourSynth installation.").append("<br>");
			text += tr("This is most likely caused by an erroneous VapourSynth Plugin, please try to clean your Filters folder!").append("<br>");
			text += tr("We suggest to move all .dll files out of your VapourSynth Filters folder and try again.");
			int val = QMessageBox::critical(this, tr("VapourSynth Error"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
			if(val != 1) INIT_ERROR_EXIT();
		}
		if((!result) || (m_vapoursynthPath.isEmpty()))
		{
			if(!m_preferences->disableWarnings())
			{
				QString text = tr("It appears that VapourSynth is <b>not</b> currently installed on your computer.<br>Therefore VapourSynth (.vpy) input will <b>not</b> be working at all!").append("<br><br>");
				text += tr("Please download and install VapourSynth for Windows (R19 or later):").append("<br>").append(LINK("http://www.vapoursynth.com/")).append("<br><br>");
				text += tr("Note that Python 3.3 (x86) is a prerequisite for installing VapourSynth:").append("<br>").append(LINK("http://www.python.org/getit/")).append("<br>");
				int val = QMessageBox::warning(this, tr("VapourSynth Missing"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
				if(val != 1) INIT_ERROR_EXIT();
			}
		}
		qDebug(" ");
	}

	//Update initialized flag (must do this before update check!)
	m_initialized = true;

	//Enable drag&drop support for this window, required for Qt v4.8.4+
	setAcceptDrops(true);

	//Check for expiration
	if(x264_version_date().addMonths(6) < x264_current_date_safe())
	{
		QString text;
		text += QString("<nobr><tt>%1</tt></nobr><br><br>").arg(tr("Your version of Simple x264 Launcher is more than 6 months old!").replace("-", "&minus;"));
		text += QString("<nobr><tt>%1<br><a href=\"%2\">%2</a><br><br>").arg(tr("You can download the most recent version from the official web-site now:").replace("-", "&minus;"), QString::fromLatin1(update_url));
		text += QString("<nobr><tt>%1</tt></nobr><br>").arg(tr("Alternatively, click 'Check for Updates' to run the auto-update utility.").replace("-", "&minus;"));
		QMessageBox msgBox(this);
		msgBox.setIconPixmap(QIcon(":/images/update.png").pixmap(56,56));
		msgBox.setWindowTitle(tr("Update Notification"));
		msgBox.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
		msgBox.setText(text);
		QPushButton *btn1 = msgBox.addButton(tr("Check for Updates"), QMessageBox::AcceptRole);
		QPushButton *btn2 = msgBox.addButton(tr("Discard"), QMessageBox::NoRole);
		QPushButton *btn3 = msgBox.addButton(btn2->text(), QMessageBox::RejectRole);
		btn2->setEnabled(false);
		btn3->setVisible(false);
		QTimer::singleShot(7500, btn2, SLOT(hide()));
		QTimer::singleShot(7500, btn3, SLOT(show()));
		if(msgBox.exec() == 0)
		{
			QTimer::singleShot(0, this, SLOT(checkUpdates()));
			return;
		}
	}
	else if((!m_preferences->noUpdateReminder()) && (m_recentlyUsed->lastUpdateCheck() + 14 < x264_current_date_safe().toJulianDay()))
	{
		if(QMessageBox::warning(this, tr("Update Notification"), QString("<nobr>%1</nobr>").arg(tr("Your last update check was more than 14 days ago. Check for updates now?")), tr("Check for Updates"), tr("Discard")) == 0)
		{
			QTimer::singleShot(0, this, SLOT(checkUpdates()));
			return;
		}
	}

	//Add files from command-line
	parseCommandLineArgs();
}

/*
 * Update the label position
 */
void MainWindow::updateLabelPos(void)
{
	const QWidget *const viewPort = ui->jobsView->viewport();
	m_label->setGeometry(0, 0, viewPort->width(), viewPort->height());
}

/*
 * Copy the complete log to the clipboard
 */
void MainWindow::copyLogToClipboard(bool checked)
{
	qDebug("copyLogToClipboard");
	
	if(LogFileModel *log = dynamic_cast<LogFileModel*>(ui->logView->model()))
	{
		log->copyToClipboard();
		x264_beep(x264_beep_info);
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

void MainWindow::instanceCreated(unsigned int pid)
{
	qDebug("Notification from other instance (PID=0x%X) received!", pid);
	
	x264_blink_window(this, 5, 125);
	x264_bring_to_front(this);
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
	x264_blink_window(this, 5, 125);
}

void MainWindow::checkUpdates(void)
{
	if(!m_initialized)
	{
		return;
	}

	if(countRunningJobs() > 0)
	{
		QMessageBox::warning(this, tr("Jobs Are Running"), tr("Sorry, can not update while there still are running jobs!"));
		return;
	}

	UpdaterDialog *updater = new UpdaterDialog(this, QString("%1/toolset").arg(m_appDir));
	const int ret = updater->exec();

	if(updater->getSuccess())
	{
		m_recentlyUsed->setLastUpdateCheck(x264_current_date_safe().toJulianDay());
		RecentlyUsed::saveRecentlyUsed(m_recentlyUsed);
	}

	if(ret == UpdaterDialog::READY_TO_INSTALL_UPDATE)
	{
		qWarning("Exitting program to install update...");
		close();
		QApplication::quit();
	}

	X264_DELETE(updater);
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
	if(!m_initialized)
	{
		e->ignore();
		return;
	}

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
	if((o == ui->labelBuildDate) && (e->type() == QEvent::MouseButtonPress))
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
			ui->jobsView->selectRow(newIndex.row());
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

	ui->buttonStartJob->setEnabled(status == JobStatus_Enqueued);
	ui->buttonAbortJob->setEnabled(status == JobStatus_Indexing || status == JobStatus_Running || status == JobStatus_Running_Pass1 || status == JobStatus_Running_Pass2 || status == JobStatus_Paused);
	ui->buttonPauseJob->setEnabled(status == JobStatus_Indexing || status == JobStatus_Running || status == JobStatus_Paused || status == JobStatus_Running_Pass1 || status == JobStatus_Running_Pass2);
	ui->buttonPauseJob->setChecked(status == JobStatus_Paused || status == JobStatus_Pausing);

	ui->actionJob_Delete->setEnabled(status == JobStatus_Completed || status == JobStatus_Aborted || status == JobStatus_Failed || status == JobStatus_Enqueued);
	ui->actionJob_Restart->setEnabled(status == JobStatus_Completed || status == JobStatus_Aborted || status == JobStatus_Failed || status == JobStatus_Enqueued);
	ui->actionJob_Browse->setEnabled(status == JobStatus_Completed);

	ui->actionJob_Start->setEnabled(ui->buttonStartJob->isEnabled());
	ui->actionJob_Abort->setEnabled(ui->buttonAbortJob->isEnabled());
	ui->actionJob_Pause->setEnabled(ui->buttonPauseJob->isEnabled());
	ui->actionJob_Pause->setChecked(ui->buttonPauseJob->isChecked());

	ui->editDetails->setEnabled(status != JobStatus_Paused);
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
		WinSevenTaskbar::setTaskbarProgress(this, ui->progressBar->value(), ui->progressBar->maximum());
		break;
	}

	WinSevenTaskbar::setOverlayIcon(this, icon.isNull() ? NULL : &icon);
}

/*
 * Parse command-line arguments
 */
void MainWindow::parseCommandLineArgs(void)
{
	QStringList files, args = x264_arguments();
	while(!args.isEmpty())
	{
		QString current = args.takeFirst();
		if((current.compare("--add", Qt::CaseInsensitive) == 0) || (current.compare("--add-file", Qt::CaseInsensitive) == 0))
		{
			if(!args.isEmpty())
			{
				current = args.takeFirst();
				if(QFileInfo(current).exists() && QFileInfo(current).isFile())
				{
					files << QFileInfo(current).canonicalFilePath();
				}
				else
				{
					qWarning("File '%s' not found!", current.toUtf8().constData());
				}
			}
			else
			{
				qWarning("Argument for '--add-file' is missing!");
			}
		}
		else if(current.compare("--add-job", Qt::CaseInsensitive) == 0)
		{
			if(args.size() >= 3)
			{
				const QString fileSrc = args.takeFirst();
				const QString fileOut = args.takeFirst();
				const QString templId = args.takeFirst();
				if(QFileInfo(fileSrc).exists() && QFileInfo(fileSrc).isFile())
				{
					OptionsModel options;
					if(!(templId.isEmpty() || (templId.compare("-", Qt::CaseInsensitive) == 0)))
					{
						if(!OptionsModel::loadTemplate(&options, templId.trimmed()))
						{
							qWarning("Template '%s' could not be found -> using defaults!", templId.trimmed().toUtf8().constData());
						}
					}
					appendJob(fileSrc, fileOut, &options, true);
				}
				else
				{
					qWarning("Source file '%s' not found!", fileSrc.toUtf8().constData());
				}
			}
			else
			{
				qWarning("Argument(s) for '--add-job' are missing!");
				args.clear();
			}
		}
		else
		{
			qWarning("Unknown argument: %s", current.toUtf8().constData());
		}
	}

	if(files.count() > 0)
	{
		createJobMultiple(files);
	}
}
