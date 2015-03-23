///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include "UIC_win_main.h"

//Internal
#include "global.h"
#include "cli.h"
#include "ipc.h"
#include "model_status.h"
#include "model_sysinfo.h"
#include "model_jobList.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_recently.h"
#include "thread_avisynth.h"
#include "thread_binaries.h"
#include "thread_vapoursynth.h"
#include "thread_encode.h"
#include "thread_ipc_recv.h"
#include "input_filter.h"
#include "win_addJob.h"
#include "win_about.h"
#include "win_preferences.h"
#include "win_updater.h"
#include "binaries.h"
#include "resource.h"

//MUtils
#include <MUtils/OSSupport.h>
#include <MUtils/CPUFeatures.h>
#include <MUtils/IPCChannel.h>
#include <MUtils/GUI.h>
#include <MUtils/Sound.h>
#include <MUtils/Exception.h>
#include <MUtils/Taskbar7.h>
#include <MUtils/Version.h>

//Qt
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
#include <QSystemTrayIcon>
#include <QMovie>

#include <ctime>

//Constants
static const char *tpl_last   = "<LAST_USED>";
static const char *home_url   = "http://muldersoft.com/";
static const char *update_url = "https://github.com/lordmulder/Simple-x264-Launcher/releases/latest";
static const char *avs_dl_url = "http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/";
static const char *python_url = "https://www.python.org/downloads/";
static const char *vsynth_url = "http://www.vapoursynth.com/";
static const int   vsynth_rev = 24;

//Macros
#define SET_FONT_BOLD(WIDGET,BOLD) do { QFont _font = WIDGET->font(); _font.setBold(BOLD); WIDGET->setFont(_font); } while(0)
#define SET_TEXT_COLOR(WIDGET,COLOR) do { QPalette _palette = WIDGET->palette(); _palette.setColor(QPalette::WindowText, (COLOR)); _palette.setColor(QPalette::Text, (COLOR)); WIDGET->setPalette(_palette); } while(0)
#define LINK(URL) (QString("<a href=\"%1\">%1</a>").arg((URL)))
#define INIT_ERROR_EXIT() do { close(); qApp->exit(-1); return; } while(0)
#define SETUP_WEBLINK(OBJ, URL) do { (OBJ)->setData(QVariant(QUrl(URL))); connect((OBJ), SIGNAL(triggered()), this, SLOT(showWebLink())); } while(0)
#define APP_IS_READY (m_initialized && (!m_fileTimer->isActive()) && (QApplication::activeModalWidget() == NULL))
#define ENSURE_APP_IS_READY() do { if(!APP_IS_READY) { 		MUtils::Sound::beep(MUtils::Sound::BEEP_WRN); qWarning("Cannot perfrom this action at this time!"); return; } } while(0)
#define X264_STRCMP(X,Y) ((X).compare((Y), Qt::CaseInsensitive) == 0)

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

/*
 * Constructor
 */
MainWindow::MainWindow(const MUtils::CPUFetaures::cpu_info_t &cpuFeatures, MUtils::IPCChannel *const ipcChannel)
:
	m_ipcChannel(ipcChannel),
	m_sysinfo(NULL),
	m_options(NULL),
	m_jobList(NULL),
	m_pendingFiles(new QStringList()),
	m_preferences(NULL),
	m_recentlyUsed(NULL),
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

	//Create and initialize the sysinfo object
	m_sysinfo.reset(new SysinfoModel());
	m_sysinfo->setAppPath(QApplication::applicationDirPath());
	m_sysinfo->setCPUFeatures(SysinfoModel::CPUFeatures_MMX, cpuFeatures.features & MUtils::CPUFetaures::FLAG_MMX);
	m_sysinfo->setCPUFeatures(SysinfoModel::CPUFeatures_SSE, cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE);
	m_sysinfo->setCPUFeatures(SysinfoModel::CPUFeatures_X64, cpuFeatures.x64 && (cpuFeatures.features & MUtils::CPUFetaures::FLAG_SSE2)); //X64 implies SSE2

	//Load preferences
	m_preferences.reset(new PreferencesModel());
	PreferencesModel::loadPreferences(m_preferences.data());

	//Load recently used
	m_recentlyUsed.reset(new RecentlyUsed());
	RecentlyUsed::loadRecentlyUsed(m_recentlyUsed.data());

	//Create options object
	m_options.reset(new OptionsModel(m_sysinfo.data()));
	OptionsModel::loadTemplate(m_options.data(), QString::fromLatin1(tpl_last));

	//Freeze minimum size
	setMinimumSize(size());
	ui->splitter->setSizes(QList<int>() << 16 << 196);

	//Update title
	ui->labelBuildDate->setText(tr("Built on %1 at %2").arg(MUtils::Version::app_build_date().toString(Qt::ISODate), MUtils::Version::app_build_time().toString(Qt::ISODate)));
	
	if(MUTILS_DEBUG)
	{
		setWindowTitle(QString("%1 | !!! DEBUG VERSION !!!").arg(windowTitle()));
		setStyleSheet("QMenuBar, QMainWindow { background-color: yellow }");
	}
	else if(x264_is_prerelease())
	{
		setWindowTitle(QString("%1 | PRE-RELEASE VERSION").arg(windowTitle()));
	}
	
	//Create model
	m_jobList.reset(new JobListModel(m_preferences.data()));
	connect(m_jobList.data(), SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(jobChangedData(QModelIndex, QModelIndex)));
	ui->jobsView->setModel(m_jobList.data());
	
	//Setup view
	ui->jobsView->horizontalHeader()->setSectionHidden(3, true);
	ui->jobsView->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
	ui->jobsView->horizontalHeader()->setResizeMode(1, QHeaderView::Fixed);
	ui->jobsView->horizontalHeader()->setResizeMode(2, QHeaderView::Fixed);
	ui->jobsView->horizontalHeader()->resizeSection(1, 150);
	ui->jobsView->horizontalHeader()->resizeSection(2, 90);
	ui->jobsView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
	connect(ui->jobsView->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this, SLOT(jobSelected(QModelIndex, QModelIndex)));

	//Setup key listener
	m_inputFilter_jobList.reset(new InputEventFilter(ui->jobsView));
	m_inputFilter_jobList->addKeyFilter(Qt::ControlModifier | Qt::Key_Up,   1);
	m_inputFilter_jobList->addKeyFilter(Qt::ControlModifier | Qt::Key_Down, 2);
	connect(m_inputFilter_jobList.data(), SIGNAL(keyPressed(int)), this, SLOT(jobListKeyPressed(int)));
	
	//Setup mouse listener
	m_inputFilter_version.reset(new InputEventFilter(ui->labelBuildDate));
	m_inputFilter_version->addMouseFilter(Qt::LeftButton,  0);
	m_inputFilter_version->addMouseFilter(Qt::RightButton, 0);
	connect(m_inputFilter_version.data(), SIGNAL(mouseClicked(int)), this, SLOT(versionLabelMouseClicked(int)));

	//Create context menu
	QAction *actionClipboard = new QAction(QIcon(":/buttons/page_paste.png"), tr("Copy to Clipboard"), ui->logView);
	QAction *actionSaveToLog = new QAction(QIcon(":/buttons/disk.png"), tr("Save to File..."), ui->logView);
	QAction *actionSeparator = new QAction(ui->logView);
	QAction *actionWordwraps = new QAction(QIcon(":/buttons/text_wrapping.png"), tr("Enable Line-Wrapping"), ui->logView);
	actionSeparator->setSeparator(true);
	actionWordwraps->setCheckable(true);
	actionClipboard->setEnabled(false);
	actionSaveToLog->setEnabled(false);
	actionWordwraps->setEnabled(false);
	ui->logView->addAction(actionClipboard);
	ui->logView->addAction(actionSaveToLog);
	ui->logView->addAction(actionSeparator);
	ui->logView->addAction(actionWordwraps);
	connect(actionClipboard, SIGNAL(triggered(bool)), this, SLOT(copyLogToClipboard(bool)));
	connect(actionSaveToLog, SIGNAL(triggered(bool)), this, SLOT(saveLogToLocalFile(bool)));
	connect(actionWordwraps, SIGNAL(triggered(bool)), this, SLOT(toggleLineWrapping(bool)));
	ui->jobsView->addActions(ui->menuJob->actions());

	//Enable buttons
	connect(ui->buttonAddJob,       SIGNAL(clicked()),     this, SLOT(addButtonPressed()      ));
	connect(ui->buttonStartJob,     SIGNAL(clicked()),     this, SLOT(startButtonPressed()    ));
	connect(ui->buttonAbortJob,     SIGNAL(clicked()),     this, SLOT(abortButtonPressed()    ));
	connect(ui->buttonPauseJob,     SIGNAL(toggled(bool)), this, SLOT(pauseButtonPressed(bool)));
	connect(ui->actionJob_Delete,   SIGNAL(triggered()),   this, SLOT(deleteButtonPressed()   ));
	connect(ui->actionJob_Restart,  SIGNAL(triggered()),   this, SLOT(restartButtonPressed()  ));
	connect(ui->actionJob_Browse,   SIGNAL(triggered()),   this, SLOT(browseButtonPressed()   ));
	connect(ui->actionJob_MoveUp,   SIGNAL(triggered()),   this, SLOT(moveButtonPressed()     ));
	connect(ui->actionJob_MoveDown, SIGNAL(triggered()),   this, SLOT(moveButtonPressed()     ));

	//Enable menu
	connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(openActionTriggered()));
	connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
	connect(ui->actionPreferences, SIGNAL(triggered()), this, SLOT(showPreferences()));
	connect(ui->actionCheckForUpdates, SIGNAL(triggered()), this, SLOT(checkUpdates()));

	//Setup web-links
	SETUP_WEBLINK(ui->actionWebMulder,          home_url);
	SETUP_WEBLINK(ui->actionWebX264,            "http://www.videolan.org/developers/x264.html");
	SETUP_WEBLINK(ui->actionWebX265,            "http://www.videolan.org/developers/x265.html");
	SETUP_WEBLINK(ui->actionWebKomisar,         "http://komisar.gin.by/");
	SETUP_WEBLINK(ui->actionWebVideoLAN,        "http://download.videolan.org/pub/x264/binaries/");
	SETUP_WEBLINK(ui->actionWebJEEB,            "http://x264.fushizen.eu/");
	SETUP_WEBLINK(ui->actionWebFreeCodecs,      "http://www.free-codecs.com/x264_video_codec_download.htm");
	SETUP_WEBLINK(ui->actionWebX265BinRU,       "http://x265.ru/en/builds/");
	SETUP_WEBLINK(ui->actionWebX265BinEU,       "http://builds.x265.eu/");
	SETUP_WEBLINK(ui->actionWebX265BinORG,      "http://chromashift.org/x265_builds/");
	SETUP_WEBLINK(ui->actionWebX265BinFF,       "http://ffmpeg.zeranoe.com/builds/");
	SETUP_WEBLINK(ui->actionWebAvisynth32,      "http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/");
	SETUP_WEBLINK(ui->actionWebAvisynth64,      "http://code.google.com/p/avisynth64/downloads/list");
	SETUP_WEBLINK(ui->actionWebAvisynthPlus,    "http://www.avs-plus.net/");
	SETUP_WEBLINK(ui->actionWebVapourSynth,     "http://www.vapoursynth.com/");
	SETUP_WEBLINK(ui->actionWebVapourSynthDocs, "http://www.vapoursynth.com/doc/");
	SETUP_WEBLINK(ui->actionOnlineDocX264,      "http://en.wikibooks.org/wiki/MeGUI/x264_Settings");			//http://mewiki.project357.com/wiki/X264_Settings
	SETUP_WEBLINK(ui->actionOnlineDocX265,      "http://x265.readthedocs.org/en/default/");
	SETUP_WEBLINK(ui->actionWebBluRay,          "http://www.x264bluray.com/");
	SETUP_WEBLINK(ui->actionWebAvsWiki,         "http://avisynth.nl/index.php/Main_Page#Usage");
	SETUP_WEBLINK(ui->actionWebSupport,         "http://forum.doom9.org/showthread.php?t=144140");
	SETUP_WEBLINK(ui->actionWebSecret,          "http://www.youtube.com/watch_popup?v=AXIeHY-OYNI");

	//Create floating label
	m_label[0].reset(new QLabel(ui->jobsView->viewport()));
	m_label[1].reset(new QLabel(ui->logView->viewport()));
	if(!m_label[0].isNull())
	{
		m_label[0]->setText(tr("No job created yet. Please click the 'Add New Job' button!"));
		m_label[0]->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		SET_TEXT_COLOR(m_label[0], Qt::darkGray);
		SET_FONT_BOLD(m_label[0], true);
		m_label[0]->setVisible(true);
		m_label[0]->setContextMenuPolicy(Qt::ActionsContextMenu);
		m_label[0]->addActions(ui->jobsView->actions());
	}
	if(!m_label[1].isNull())
	{
		m_animation.reset(new QMovie(":/images/spinner.gif"));
		m_label[1]->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
		if(!m_animation.isNull())
		{
			m_label[1]->setMovie(m_animation.data());
			m_animation->start();
		}
	}
	connect(ui->splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(updateLabelPos()));
	updateLabelPos();

	//Init system tray icon
	m_sysTray.reset(new QSystemTrayIcon(this));
	m_sysTray->setToolTip(this->windowTitle());
	m_sysTray->setIcon(this->windowIcon());
	connect(m_sysTray.data(), SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(sysTrayActived()));

	//Init taskbar progress
	m_taskbar.reset(new MUtils::Taskbar7(this));

	//Create corner widget
	QLabel *checkUp = new QLabel(ui->menubar);
	checkUp->setText(QString("<nobr><img src=\":/buttons/exclamation_small.png\">&nbsp;<b style=\"color:darkred\">%1</b>&nbsp;&nbsp;&nbsp;</nobr>").arg(tr("Check for Updates")));
	checkUp->setFixedHeight(ui->menubar->height());
	checkUp->setCursor(QCursor(Qt::PointingHandCursor));
	m_inputFilter_checkUp.reset(new InputEventFilter(checkUp));
	m_inputFilter_checkUp->addMouseFilter(Qt::LeftButton,  0);
	m_inputFilter_checkUp->addMouseFilter(Qt::RightButton, 0);
	connect(m_inputFilter_checkUp.data(), SIGNAL(mouseClicked(int)), this, SLOT(checkUpdates()));
	checkUp->hide();
	ui->menubar->setCornerWidget(checkUp);

	//Create timer
	m_fileTimer.reset(new QTimer(this));
	connect(m_fileTimer.data(), SIGNAL(timeout()), this, SLOT(handlePendingFiles()));
}

/*
 * Destructor
 */
MainWindow::~MainWindow(void)
{
	OptionsModel::saveTemplate(m_options.data(), QString::fromLatin1(tpl_last));
	
	if(!m_ipcThread.isNull())
	{
		m_ipcThread->stop();
		if(!m_ipcThread->wait(5000))
		{
			m_ipcThread->terminate();
			m_ipcThread->wait();
		}
	}

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
	ENSURE_APP_IS_READY();

	qDebug("MainWindow::addButtonPressed");
	bool runImmediately = (countRunningJobs() < (m_preferences->getAutoRunNextJob() ? m_preferences->getMaxRunningJobCount() : 1));
	QString sourceFileName, outputFileName;

	if(createJob(sourceFileName, outputFileName, m_options.data(), runImmediately))
	{
		appendJob(sourceFileName, outputFileName, m_options.data(), runImmediately);
	}
}

/*
 * The "open" action was triggered
 */
void MainWindow::openActionTriggered()
{
	ENSURE_APP_IS_READY();

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
			bool runImmediately = (countRunningJobs() < (m_preferences->getAutoRunNextJob() ? m_preferences->getMaxRunningJobCount() : 1));
			QString sourceFileName(fileList.first()), outputFileName;
			if(createJob(sourceFileName, outputFileName, m_options.data(), runImmediately))
			{
				appendJob(sourceFileName, outputFileName, m_options.data(), runImmediately);
			}
		}
	}
}

/*
 * The "start" button was clicked
 */
void MainWindow::startButtonPressed(void)
{
	ENSURE_APP_IS_READY();
	m_jobList->startJob(ui->jobsView->currentIndex());
}

/*
 * The "abort" button was clicked
 */
void MainWindow::abortButtonPressed(void)
{
	ENSURE_APP_IS_READY();

	if(QMessageBox::question(this, tr("Abort Job?"), tr("<nobr>Do you really want to <b>abort</b> the selected job now?</nobr>"), tr("Back"), tr("Abort Job")) == 1)
	{
		m_jobList->abortJob(ui->jobsView->currentIndex());
	}
}

/*
 * The "delete" button was clicked
 */
void MainWindow::deleteButtonPressed(void)
{
	ENSURE_APP_IS_READY();

	m_jobList->deleteJob(ui->jobsView->currentIndex());
	m_label[0]->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
}

/*
 * The "browse" button was clicked
 */
void MainWindow::browseButtonPressed(void)
{
	ENSURE_APP_IS_READY();

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
 * The "browse" button was clicked
 */
void MainWindow::moveButtonPressed(void)
{
	ENSURE_APP_IS_READY();

	if(sender() == ui->actionJob_MoveUp)
	{
		qDebug("Move job %d (direction: UP)", ui->jobsView->currentIndex().row());
		if(!m_jobList->moveJob(ui->jobsView->currentIndex(), JobListModel::MOVE_UP))
		{
			MUtils::Sound::beep(MUtils::Sound::BEEP_ERR);
		}
		ui->jobsView->scrollTo(ui->jobsView->currentIndex(), QAbstractItemView::PositionAtCenter);
	}
	else if(sender() == ui->actionJob_MoveDown)
	{
		qDebug("Move job %d (direction: DOWN)", ui->jobsView->currentIndex().row());
		if(!m_jobList->moveJob(ui->jobsView->currentIndex(), JobListModel::MOVE_DOWN))
		{
			MUtils::Sound::beep(MUtils::Sound::BEEP_ERR);
		}
		ui->jobsView->scrollTo(ui->jobsView->currentIndex(), QAbstractItemView::PositionAtCenter);
	}
	else
	{
		qWarning("[moveButtonPressed] Error: Unknown sender!");
	}
}

/*
 * The "pause" button was clicked
 */
void MainWindow::pauseButtonPressed(bool checked)
{
	if(!APP_IS_READY)
	{
		MUtils::Sound::beep(MUtils::Sound::BEEP_WRN);
		qWarning("Cannot perfrom this action at this time!");
		ui->buttonPauseJob->setChecked(!checked);
	}

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
	ENSURE_APP_IS_READY();

	const QModelIndex index = ui->jobsView->currentIndex();
	const OptionsModel *options = m_jobList->getJobOptions(index);
	QString sourceFileName = m_jobList->getJobSourceFile(index);
	QString outputFileName = m_jobList->getJobOutputFile(index);

	if((options) && (!sourceFileName.isEmpty()) && (!outputFileName.isEmpty()))
	{
		bool runImmediately = (countRunningJobs() < (m_preferences->getAutoRunNextJob() ? m_preferences->getMaxRunningJobCount() : 1));
		OptionsModel *tempOptions = new OptionsModel(*options);
		if(createJob(sourceFileName, outputFileName, tempOptions, runImmediately, true))
		{
			appendJob(sourceFileName, outputFileName, tempOptions, runImmediately);
		}
		MUTILS_DELETE(tempOptions);
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
		foreach(QAction *action, ui->logView->actions())
		{
			action->setEnabled(true);
		}
		QTimer::singleShot(0, ui->logView, SLOT(scrollToBottom()));

		ui->progressBar->setValue(m_jobList->getJobProgress(current));
		ui->editDetails->setText(m_jobList->data(m_jobList->index(current.row(), 3, QModelIndex()), Qt::DisplayRole).toString());
		updateButtons(m_jobList->getJobStatus(current));
		updateTaskbar(m_jobList->getJobStatus(current), m_jobList->data(m_jobList->index(current.row(), 0, QModelIndex()), Qt::DecorationRole).value<QIcon>());
	}
	else
	{
		ui->logView->setModel(NULL);
		foreach(QAction *action, ui->logView->actions())
		{
			action->setEnabled(false);
		}
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
				if(m_preferences->getAutoRunNextJob()) QTimer::singleShot(0, this, SLOT(launchNextJob()));
				if(m_preferences->getSaveLogFiles()) saveLogFile(m_jobList->index(i, 1, QModelIndex()));
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
				if(!m_taskbar.isNull())
				{
					m_taskbar->setTaskbarProgress(ui->progressBar->value(), ui->progressBar->maximum());
				}
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
	ENSURE_APP_IS_READY();
	
	if(AboutDialog *aboutDialog = new AboutDialog(this))
	{
		aboutDialog->exec();
		MUTILS_DELETE(aboutDialog);
	}
}

/*
 * Open web-link
 */
void MainWindow::showWebLink(void)
{
	ENSURE_APP_IS_READY();
	
	if(QObject *obj = QObject::sender())
	{
		if(QAction *action = dynamic_cast<QAction*>(obj))
		{
			if(action->data().type() == QVariant::Url)
			{
				QDesktopServices::openUrl(action->data().toUrl());
			}
		}
	}
}

/*
 * Pereferences dialog
 */
void MainWindow::showPreferences(void)
{
	ENSURE_APP_IS_READY();

	PreferencesDialog *preferences = new PreferencesDialog(this, m_preferences.data(), m_sysinfo.data());
	preferences->exec();

	MUTILS_DELETE(preferences);
}

/*
 * Launch next job, after running job has finished
 */
void MainWindow::launchNextJob(void)
{
	qDebug("Launching next job...");

	if(countRunningJobs() >= m_preferences->getMaxRunningJobCount())
	{
		qDebug("Still have too many jobs running, won't launch next one yet!");
		return;
	}

	const int rows = m_jobList->rowCount(QModelIndex());

	for(int i = 0; i < rows; i++)
	{
		const QModelIndex currentIndex = m_jobList->index(i, 0, QModelIndex());
		if(m_jobList->getJobStatus(currentIndex) == JobStatus_Enqueued)
		{
			if(m_jobList->startJob(currentIndex))
			{
				ui->jobsView->selectRow(currentIndex.row());
				return;
			}
		}
	}
		
	qWarning("No enqueued jobs left to be started!");

	if(m_preferences->getShutdownComputer())
	{
		QTimer::singleShot(0, this, SLOT(shutdownComputer()));
	}
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
	ENSURE_APP_IS_READY();

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
	MUtils::Sound::play_sound("shutdown", false);
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
		MUtils::Sound::play_sound(((i < iTimeout) ? "beep" : "beep2"), false);
	}
	
	qWarning("Shutting down !!!");

	if(MUtils::OS::shutdown_computer("Simple x264 Launcher: All jobs completed, shutting down!", 10, true, false))
	{
		qApp->closeAllWindows();
	}

}

/*
 * Main initialization function (called only once!)
 */
void MainWindow::init(void)
{
	if(m_initialized)
	{
		qWarning("Already initialized -> skipping!");
		return;
	}

	updateLabelPos();
	const MUtils::OS::ArgumentMap &arguments = MUtils::OS::arguments();
	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

	//---------------------------------------
	// Check required binaries
	//---------------------------------------
		
	qDebug("[Validating binaries]");
	if(!BinariesCheckThread::check(m_sysinfo.data()))
	{
		QMessageBox::critical(this, tr("Invalid File!"), tr("<nobr>At least one required tool is missing or is not a valid Win32/Win64 binary.<br>Please re-install the program in order to fix the problem!</nobr>").replace("-", "&minus;"));
		qFatal("At least one required tool is missing or is not a valid Win32/Win64 binary!");
	}
	qDebug(" ");
	
	//---------------------------------------
	// Check for portable mode
	//---------------------------------------

	if(x264_is_portable())
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

	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

	//---------------------------------------
	// Check CPU capabilities
	//---------------------------------------

	//Make sure this CPU can run x264 (requires MMX + MMXEXT/iSSE to run x264 with ASM enabled, additionally requires SSE1 for most x264 builds)
	if(!m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_MMX))
	{
		QMessageBox::critical(this, tr("Unsupported CPU"), tr("<nobr>Sorry, but this machine is <b>not</b> physically capable of running x264 (with assembly).<br>Please get a CPU that supports at least the MMX and MMXEXT instruction sets!</nobr>"), tr("Quit"));
		qFatal("System does not support MMX and MMXEXT, x264 will not work !!!");
		INIT_ERROR_EXIT();
	}
	else if(!m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_SSE))
	{
		qWarning("WARNING: System does not support SSE (v1), x264/x265 probably will *not* work !!!\n");
		int val = QMessageBox::warning(this, tr("Unsupported CPU"), tr("<nobr>It appears that this machine does <b>not</b> support the SSE1 instruction set.<br>Thus most builds of x264/x265 will <b>not</b> run on this computer at all.<br><br>Please get a CPU that supports the MMX and SSE1 instruction sets!</nobr>"), tr("Quit"), tr("Ignore"));
		if(val != 1) INIT_ERROR_EXIT();
	}

	//Skip version check (not recommended!)
	if(arguments.contains(CLI_PARAM_SKIP_VERSION_CHECK))
	{
		qWarning("Version checks are disabled now, you have been warned!\n");
		m_preferences->setSkipVersionTest(true);
	}
	
	//Don't abort encoding process on timeout (not recommended!)
	if(arguments.contains(CLI_PARAM_NO_DEADLOCK))
	{
		qWarning("Deadlock detection disabled, you have been warned!\n");
		m_preferences->setAbortOnTimeout(false);
	}

	qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

	//---------------------------------------
	// Check Avisynth support
	//---------------------------------------

	if(!arguments.contains(CLI_PARAM_SKIP_AVS_CHECK))
	{
		qDebug("[Check for Avisynth support]");
		if(!AvisynthCheckThread::detect(m_sysinfo.data()))
		{
			QString text = tr("A critical error was encountered while checking your Avisynth version.").append("<br>");
			text += tr("This is most likely caused by an erroneous Avisynth Plugin, please try to clean your Plugins folder!").append("<br>");
			text += tr("We suggest to move all .dll and .avsi files out of your Avisynth Plugins folder and try again.");
			int val = QMessageBox::critical(this, tr("Avisynth Error"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
			if(val != 1) INIT_ERROR_EXIT();
		}
		else if((!m_sysinfo->hasAvisynth()) && (!m_preferences->getDisableWarnings()))
		{
			QString text = tr("It appears that Avisynth is <b>not</b> currently installed on your computer.<br>Therefore Avisynth (.avs) input will <b>not</b> be working at all!").append("<br><br>");
			text += tr("Please download and install Avisynth:").append("<br>").append(LINK(avs_dl_url));
			int val = QMessageBox::warning(this, tr("Avisynth Missing"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Close"), tr("Disable this Warning"));
			if(val == 1)
			{
				m_preferences->setDisableWarnings(true);
				PreferencesModel::savePreferences(m_preferences.data());
			}
		}
		qDebug(" ");
	}

	//---------------------------------------
	// Check VapurSynth support
	//---------------------------------------

	if(!arguments.contains(CLI_PARAM_SKIP_VPS_CHECK))
	{
		qDebug("[Check for VapourSynth support]");
		if(!VapourSynthCheckThread::detect(m_sysinfo.data()))
		{
			QString text = tr("A critical error was encountered while checking your VapourSynth installation.").append("<br>");
			text += tr("This is most likely caused by an erroneous VapourSynth Plugin, please try to clean your Filters folder!").append("<br>");
			text += tr("We suggest to move all .dll files out of your VapourSynth Filters folder and try again.");
			const int val = QMessageBox::critical(this, tr("VapourSynth Error"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
			if(val != 1) INIT_ERROR_EXIT();
		}
		else if((!m_sysinfo->hasVapourSynth()) && (!m_preferences->getDisableWarnings()))
		{
			QString text = tr("It appears that VapourSynth is <b>not</b> currently installed on your computer.<br>Therefore VapourSynth (.vpy) input will <b>not</b> be working at all!").append("<br><br>");
			text += tr("Please download and install VapourSynth (<b>r%1</b> or later) for Windows:").arg(QString::number(vsynth_rev)).append("<br>").append(LINK(vsynth_url)).append("<br><br>");
			text += tr("Note that Python v3.4 is a prerequisite for installing VapourSynth:").append("<br>").append(LINK(python_url)).append("<br>");
			const int val = QMessageBox::warning(this, tr("VapourSynth Missing"), QString("<nobr>%1</nobr>").arg(text).replace("-", "&minus;"), tr("Close"), tr("Disable this Warning"));
			if(val == 1)
			{
				m_preferences->setDisableWarnings(true);
				PreferencesModel::savePreferences(m_preferences.data());
			}
		}
		qDebug(" ");
	}
	
	//---------------------------------------
	// Create the IPC listener thread
	//---------------------------------------

	if(m_ipcChannel)
	{
		m_ipcThread.reset(new IPCThread_Recv(m_ipcChannel));
		connect(m_ipcThread.data(), SIGNAL(receivedCommand(int,QStringList,quint32)), this, SLOT(handleCommand(int,QStringList,quint32)), Qt::QueuedConnection);
		m_ipcThread->start();
	}

	//---------------------------------------
	// Finish initialization
	//---------------------------------------

	//Set Window title
	setWindowTitle(QString("%1 (%2)").arg(windowTitle(), m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64) ? "64-Bit" : "32-Bit"));

	//Enable drag&drop support for this window, required for Qt v4.8.4+
	setAcceptDrops(true);

	//Update flag
	m_initialized = true;

	//Hide the spinner animation
	if(!m_label[1].isNull())
	{
		if(!m_animation.isNull())
		{
			m_animation->stop();
		}
		m_label[1]->setVisible(false);
	}

	//---------------------------------------
	// Check for Expiration
	//---------------------------------------

	if(MUtils::Version::app_build_date().addMonths(6) < MUtils::OS::current_date())
	{
		if(QWidget *cornerWidget = ui->menubar->cornerWidget()) cornerWidget->show();
		QString text;
		text += QString("<nobr><tt>%1</tt></nobr><br><br>").arg(tr("Your version of Simple x264 Launcher is more than 6 months old!").replace('-', "&minus;"));
		text += QString("<nobr><tt>%1<br><a href=\"%2\">%3</a><br><br>").arg(tr("You can download the most recent version from the official web-site now:").replace('-', "&minus;"), QString::fromLatin1(update_url), QString::fromLatin1(update_url).replace("-", "&minus;"));
		text += QString("<nobr><tt>%1</tt></nobr><br>").arg(tr("Alternatively, click 'Check for Updates' to run the auto-update utility.").replace('-', "&minus;"));
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
	else if(!parseCommandLineArgs())
	{
		//Update reminder
		if(arguments.contains(CLI_PARAM_FIRST_RUN))
		{
			qWarning("First run -> resetting update check now!");
			m_recentlyUsed->setLastUpdateCheck(0);
			RecentlyUsed::saveRecentlyUsed(m_recentlyUsed.data());
		}
		else if(m_recentlyUsed->lastUpdateCheck() + 14 < MUtils::OS::current_date().toJulianDay())
		{
			if(QWidget *cornerWidget = ui->menubar->cornerWidget()) cornerWidget->show();
			if(!m_preferences->getNoUpdateReminder())
			{
				if(QMessageBox::warning(this, tr("Update Notification"), QString("<nobr>%1</nobr>").arg(tr("Your last update check was more than 14 days ago. Check for updates now?")), tr("Check for Updates"), tr("Discard")) == 0)
				{
					QTimer::singleShot(0, this, SLOT(checkUpdates()));
					return;
				}
			}
		}
	}

	//Load queued jobs
	if(m_jobList->loadQueuedJobs(m_sysinfo.data()) > 0)
	{
		m_label[0]->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
		m_jobList->clearQueuedJobs();
	}
}

/*
 * Update the label position
 */
void MainWindow::updateLabelPos(void)
{
	for(int i = 0; i < 2; i++)
	{
		//const QWidget *const viewPort = ui->jobsView->viewport();
		const QWidget *const viewPort = dynamic_cast<QWidget*>(m_label[i]->parent());
		if(viewPort)
		{
			m_label[i]->setGeometry(0, 0, viewPort->width(), viewPort->height());
		}
	}
}

/*
 * Copy the complete log to the clipboard
 */
void MainWindow::copyLogToClipboard(bool checked)
{
	qDebug("Coyping logfile to clipboard...");
	
	if(LogFileModel *log = dynamic_cast<LogFileModel*>(ui->logView->model()))
	{
		log->copyToClipboard();
		MUtils::Sound::beep(MUtils::Sound::BEEP_NFO);
	}
}

/*
 * Save log to local file
 */
void MainWindow::saveLogToLocalFile(bool checked)
{
	ENSURE_APP_IS_READY();

	const QModelIndex index = ui->jobsView->currentIndex();
	const QString initialName = index.isValid() ? QFileInfo(m_jobList->getJobOutputFile(index)).completeBaseName() : tr("Logfile");
	const QString fileName = QFileDialog::getSaveFileName(this, tr("Save Log File"), initialName, tr("Log File (*.log)"));
	if(!fileName.isEmpty())
	{
		if(LogFileModel *log = dynamic_cast<LogFileModel*>(ui->logView->model()))
		{
			if(!log->saveToLocalFile(fileName))
			{
				QMessageBox::warning(this, this->windowTitle(), tr("Error: Log file could not be saved!"));
			}
		}
	}
}

/*
 * Toggle line-wrapping
 */
void MainWindow::toggleLineWrapping(bool checked)
{
	ui->logView->setWordWrap(checked);
}

/*
 * Process the dropped files
 */
void MainWindow::handlePendingFiles(void)
{
	qDebug("MainWindow::handlePendingFiles");

	if(!m_pendingFiles->isEmpty())
	{
		QStringList pendingFiles(*m_pendingFiles);
		m_pendingFiles->clear();
		createJobMultiple(pendingFiles);
	}

	qDebug("Leave from MainWindow::handlePendingFiles!");
}

/*
 * Handle incoming IPC command
 */
void MainWindow::handleCommand(const int &command, const QStringList &args, const quint32 &flags)
{
	if(!(m_initialized && (QApplication::activeModalWidget() == NULL)))
	{
		qWarning("Cannot accapt commands at this time -> discarding!");
		return;
	}
	
	if((!isVisible()) || m_sysTray->isVisible())
	{
		sysTrayActived();
	}

	MUtils::GUI::bring_to_front(this);
	
#ifdef IPC_LOGGING
	qDebug("\n---------- IPC ----------");
	qDebug("CommandId: %d", command);
	for(QStringList::ConstIterator iter = args.constBegin(); iter != args.constEnd(); iter++)
	{
		qDebug("Arguments: %s", iter->toUtf8().constData());
	}
	qDebug("The Flags: 0x%08X", flags);
	qDebug("---------- IPC ----------\n");
#endif //IPC_LOGGING

	switch(command)
	{
	case IPC_OPCODE_PING:
		qDebug("Received a PING request from another instance!");
		MUtils::GUI::blink_window(this, 5, 125);
		break;
	case IPC_OPCODE_ADD_FILE:
		if(!args.isEmpty())
		{
			if(QFileInfo(args[0]).exists() && QFileInfo(args[0]).isFile())
			{
				*m_pendingFiles << QFileInfo(args[0]).canonicalFilePath();
				if(!m_fileTimer->isActive())
				{
					m_fileTimer->setSingleShot(true);
					m_fileTimer->start(5000);
				}
			}
			else
			{
				qWarning("File '%s' not found!", args[0].toUtf8().constData());
			}
		}
		break;
	case IPC_OPCODE_ADD_JOB:
		if(args.size() >= 3)
		{
			if(QFileInfo(args[0]).exists() && QFileInfo(args[0]).isFile())
			{
				OptionsModel options(m_sysinfo.data());
				bool runImmediately = (countRunningJobs() < (m_preferences->getAutoRunNextJob() ? m_preferences->getMaxRunningJobCount() : 1));
				if(!(args[2].isEmpty() || X264_STRCMP(args[2], "-")))
				{
					if(!OptionsModel::loadTemplate(&options, args[2].trimmed()))
					{
						qWarning("Template '%s' could not be found -> using defaults!", args[2].trimmed().toUtf8().constData());
					}
				}
				if((flags & IPC_FLAG_FORCE_START) && (!(flags & IPC_FLAG_FORCE_ENQUEUE))) runImmediately = true;
				if((flags & IPC_FLAG_FORCE_ENQUEUE) && (!(flags & IPC_FLAG_FORCE_START))) runImmediately = false;
				appendJob(args[0], args[1], &options, runImmediately);
			}
			else
			{
				qWarning("Source file '%s' not found!", args[0].toUtf8().constData());
			}
		}
		break;
	default:
		MUTILS_THROW("Unknown command received!");
	}
}

/*
 * Check for new updates
 */
void MainWindow::checkUpdates(void)
{
	ENSURE_APP_IS_READY();

	if(countRunningJobs() > 0)
	{
		QMessageBox::warning(this, tr("Jobs Are Running"), tr("Sorry, can not update while there still are running jobs!"));
		return;
	}

	UpdaterDialog *updater = new UpdaterDialog(this, m_sysinfo.data(), update_url);
	const int ret = updater->exec();

	if(updater->getSuccess())
	{
		m_recentlyUsed->setLastUpdateCheck(MUtils::OS::current_date().toJulianDay());
		RecentlyUsed::saveRecentlyUsed(m_recentlyUsed.data());
		if(QWidget *cornerWidget = ui->menubar->cornerWidget()) cornerWidget->hide();
	}

	if(ret == UpdaterDialog::READY_TO_INSTALL_UPDATE)
	{
		qWarning("Exitting program to install update...");
		close();
		QApplication::quit();
	}

	MUTILS_DELETE(updater);
}

/*
 * Handle mouse event for version label
 */
void MainWindow::versionLabelMouseClicked(const int &tag)
{
	if(tag == 0)
	{
		QTimer::singleShot(0, this, SLOT(showAbout()));
	}
}

/*
 * Handle key event for job list
 */
void MainWindow::jobListKeyPressed(const int &tag)
{
	switch(tag)
	{
	case 1:
		ui->actionJob_MoveUp->trigger();
		break;
	case 2:
		ui->actionJob_MoveDown->trigger();
		break;
	}
}

/*
 * System tray was activated
 */
void MainWindow::sysTrayActived(void)
{
	m_sysTray->hide();
	showNormal();
	MUtils::GUI::bring_to_front(this);
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

	if(!m_initialized)
	{
		QTimer::singleShot(0, this, SLOT(init()));
	}
}

/*
 * Window close event
 */
void MainWindow::closeEvent(QCloseEvent *e)
{
	if(!APP_IS_READY)
	{
		e->ignore();
		qWarning("Cannot close window at this time!");
		return;
	}

	//Make sure we have no running jobs left!
	if(countRunningJobs() > 0)
	{
		e->ignore();
		if(!m_preferences->getNoSystrayWarning())
		{
			if(QMessageBox::warning(this, tr("Jobs Are Running"), tr("<nobr>You still have running jobs, application will be minimized to notification area!<nobr>"), tr("OK"), tr("Don't Show Again")) == 1)
			{
				m_preferences->setNoSystrayWarning(true);
				PreferencesModel::savePreferences(m_preferences.data());
			}
		}
		hide();
		m_sysTray->show();
		return;
	}

	//Save pending jobs for next time, if desired by user
	if(countPendingJobs() > 0)
	{
		int ret = QMessageBox::question(this, tr("Jobs Are Pending"), tr("You still have pending jobs. How do you want to proceed?"), tr("Save Pending Jobs"), tr("Discard"));
		if(ret == 0)
		{
			m_jobList->saveQueuedJobs();
		}
		else
		{
			if(QMessageBox::warning(this, tr("Jobs Are Pending"), tr("Do you really want to discard all pending jobs?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
			{
				e->ignore();
				return;
			}
		}
	}
	
	//Delete remaining jobs
	while(m_jobList->rowCount(QModelIndex()) > 0)
	{
		if((m_jobList->rowCount(QModelIndex()) % 10) == 0)
		{
			qApp->processEvents(QEventLoop::ExcludeUserInputEvents);
		}
		if(!m_jobList->deleteJob(m_jobList->index(0, 0, QModelIndex())))
		{
			e->ignore();
			QMessageBox::warning(this, tr("Failed To Exit"), tr("Warning: At least one job could not be deleted!"));
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
	if(!(m_initialized && (QApplication::activeModalWidget() == NULL)))
	{
		qWarning("Cannot accept dropped files at this time -> discarding!");
		return;
	}

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
		m_pendingFiles->append(droppedFiles);
		m_pendingFiles->sort();
		if(!m_fileTimer->isActive())
		{
			m_fileTimer->setSingleShot(true);
			m_fileTimer->start(5000);
		}
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
	AddJobDialog *addDialog = new AddJobDialog(this, options, m_recentlyUsed.data(), m_sysinfo.data(), m_preferences.data());

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

	MUTILS_DELETE(addDialog);
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
		runImmediately = (countRunningJobs() < (m_preferences->getAutoRunNextJob() ? m_preferences->getMaxRunningJobCount() : 1));
		QString sourceFileName(*iter), outputFileName;
		if(createJob(sourceFileName, outputFileName, m_options.data(), runImmediately, false, counter++, filePathIn.count(), &applyToAll))
		{
			if(appendJob(sourceFileName, outputFileName, m_options.data(), runImmediately))
			{
				continue;
			}
		}
		return false;
	}

	//Add remaining files
	while(applyToAll && (iter != filePathIn.constEnd()))
	{
		const bool runImmediatelyTmp = runImmediately && (countRunningJobs() < (m_preferences->getAutoRunNextJob() ? m_preferences->getMaxRunningJobCount() : 1));
		const QString sourceFileName = *iter;
		const QString outputFileName = AddJobDialog::generateOutputFileName(sourceFileName, m_recentlyUsed->outputDirectory(), m_recentlyUsed->filterIndex(), m_preferences->getSaveToSourcePath());
		if(!appendJob(sourceFileName, outputFileName, m_options.data(), runImmediatelyTmp))
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
	EncodeThread *thrd = new EncodeThread(sourceFileName, outputFileName, options, m_sysinfo.data(), m_preferences.data());
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

	m_label[0]->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
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
	ui->actionJob_MoveUp->setEnabled(status != JobStatus_Undefined);
	ui->actionJob_MoveDown->setEnabled(status != JobStatus_Undefined);

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

	if(m_taskbar.isNull())
	{
		return; /*taskbar object not created yet*/
	}

	switch(status)
	{
	case JobStatus_Undefined:
		m_taskbar->setTaskbarState(MUtils::Taskbar7::TASKBAR_STATE_NONE);
		break;
	case JobStatus_Aborting:
	case JobStatus_Starting:
	case JobStatus_Pausing:
	case JobStatus_Resuming:
		m_taskbar->setTaskbarState(MUtils::Taskbar7::TASKBAR_STATE_INTERMEDIATE);
		break;
	case JobStatus_Aborted:
	case JobStatus_Failed:
		m_taskbar->setTaskbarState(MUtils::Taskbar7::TASKBAR_STATE_ERROR);
		break;
	case JobStatus_Paused:
		m_taskbar->setTaskbarState(MUtils::Taskbar7::TASKBAR_STATE_PAUSED);
		break;
	default:
		m_taskbar->setTaskbarState(MUtils::Taskbar7::TASKBAR_STATE_NORMAL);
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
		m_taskbar->setTaskbarProgress(ui->progressBar->value(), ui->progressBar->maximum());
		break;
	}

	m_taskbar->setOverlayIcon(icon.isNull() ? NULL : &icon);
}

/*
 * Parse command-line arguments
 */
bool MainWindow::parseCommandLineArgs(void)
{
	const MUtils::OS::ArgumentMap &args = MUtils::OS::arguments();

	quint32 flags = 0;
	bool commandSent = false;

	//Handle flags
	if(args.contains(CLI_PARAM_FORCE_START))
	{
		flags = ((flags | IPC_FLAG_FORCE_START) & (~IPC_FLAG_FORCE_ENQUEUE));
	}
	if(args.contains(CLI_PARAM_FORCE_ENQUEUE))
	{
		flags = ((flags | IPC_FLAG_FORCE_ENQUEUE) & (~IPC_FLAG_FORCE_START));
	}

	//Process all command-line arguments
	if(args.contains(CLI_PARAM_ADD_FILE))
	{
		foreach(const QString &fileName, args.values(CLI_PARAM_ADD_FILE))
		{
			handleCommand(IPC_OPCODE_ADD_FILE, QStringList() << fileName, flags);
		}
		commandSent = true;
	}
	if(args.contains(CLI_PARAM_ADD_JOB))
	{
		foreach(const QString &options, args.values(CLI_PARAM_ADD_JOB))
		{
			const QStringList optionValues = options.split('|', QString::SkipEmptyParts);
			if(optionValues.count() == 3)
			{
				handleCommand(IPC_OPCODE_ADD_JOB, optionValues, flags);
			}
			else
			{
				qWarning("Invalid number of arguments for parameter \"--%s\" detected!", CLI_PARAM_ADD_JOB);
			}
		}
		commandSent = true;
	}

	return commandSent;
}
