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

#include "win_main.h"

#include "model_jobList.h"
#include "model_options.h"
#include "win_addJob.h"
#include "win_preferences.h"
#include "taskbar7.h"
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

#include <Mmsystem.h>

const char *home_url = "http://mulder.brhack.net/";
const char *update_url = "http://code.google.com/p/mulder/downloads/list";
const char *tpl_last = "<LAST_USED>";

#define SET_FONT_BOLD(WIDGET,BOLD) { QFont _font = WIDGET->font(); _font.setBold(BOLD); WIDGET->setFont(_font); }
#define SET_TEXT_COLOR(WIDGET,COLOR) { QPalette _palette = WIDGET->palette(); _palette.setColor(QPalette::WindowText, (COLOR)); _palette.setColor(QPalette::Text, (COLOR)); WIDGET->setPalette(_palette); }

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
	m_firstShow(true)
{
	//Init the dialog, from the .ui file
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowMaximizeButtonHint));

	//Register meta types
	qRegisterMetaType<QUuid>("QUuid");
	qRegisterMetaType<EncodeThread::JobStatus>("EncodeThread::JobStatus");

	//Load preferences
	PreferencesDialog::initPreferences(&m_preferences);
	PreferencesDialog::loadPreferences(&m_preferences);

	//Create options object
	m_options = new OptionsModel();
	OptionsModel::loadTemplate(m_options, QString::fromLatin1(tpl_last));

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
	m_jobList = new JobListModel();
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
	connect(actionJob_Browse, SIGNAL(triggered()), this, SLOT(browseButtonPressed()));

	//Enable menu
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
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

/*
 * The "add" button was clicked
 */
void MainWindow::addButtonPressed(const QString &filePath, int fileNo, int fileTotal, bool *ok)
{
	qDebug("MainWindow::addButtonPressed");
	
	if(ok) *ok = false;
	
	AddJobDialog *addDialog = new AddJobDialog(this, m_options, m_cpuFeatures->x64);
	addDialog->setRunImmediately(countRunningJobs() < (m_preferences.autoRunNextJob ? m_preferences.maxRunningJobCount : 1));
	if((fileNo >= 0) && (fileTotal > 1)) addDialog->setWindowTitle(addDialog->windowTitle().append(tr(" (File %1 of %2)").arg(QString::number(fileNo+1), QString::number(fileTotal))));
	if(!filePath.isEmpty()) addDialog->setSourceFile(filePath);

	int result = addDialog->exec();
	if(result == QDialog::Accepted)
	{
		EncodeThread *thrd = new EncodeThread
		(
			addDialog->sourceFile(),
			addDialog->outputFile(),
			m_options,
			QString("%1/toolset").arg(m_appDir),
			m_cpuFeatures->x64,
			m_cpuFeatures->x64 && m_preferences.useAvisyth64Bit
		);

		QModelIndex newIndex = m_jobList->insertJob(thrd);

		if(newIndex.isValid())
		{
			if(addDialog->runImmediately())
			{
				jobsView->selectRow(newIndex.row());
				QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
				m_jobList->startJob(newIndex);
			}

			if(ok) *ok = true;
		}
	}

	m_label->setVisible(m_jobList->rowCount(QModelIndex()) == 0);
	X264_DELETE(addDialog);
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
		updateButtons(EncodeThread::JobStatus_Undefined);
		updateTaskbar(EncodeThread::JobStatus_Undefined, QIcon());
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
			EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
			if(i == selected)
			{
				qDebug("Current job changed status!");
				updateButtons(status);
				updateTaskbar(status, m_jobList->data(m_jobList->index(i, 0, QModelIndex()), Qt::DecorationRole).value<QIcon>());
			}
			if((status == EncodeThread::JobStatus_Completed) || (status == EncodeThread::JobStatus_Failed))
			{
				if(m_preferences.autoRunNextJob) QTimer::singleShot(0, this, SLOT(launchNextJob()));
				if(m_preferences.shutdownComputer) QTimer::singleShot(0, this, SLOT(shutdownComputer()));
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
				text2 += tr("<nobr><tt>x264 - the best H.264/AVC encoder. Copyright (c) 2003-2012 x264 project.<br>");
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
				text2 += tr("<nobr><tt>Avisynth - powerful tool for video post-production.<br>");
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
	if(QObject::sender() == actionWebSecret)     QDesktopServices::openUrl(QUrl("http://www.youtube.com/watch_popup?v=AXIeHY-OYNI"));
}

/*
 * Pereferences dialog
 */
void MainWindow::showPreferences(void)
{
	PreferencesDialog *preferences = new PreferencesDialog(this, &m_preferences, m_cpuFeatures->x64);
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

	if(countRunningJobs() >= m_preferences.maxRunningJobCount)
	{
		qDebug("Still have too many jobs running, won't launch next one yet!");
		return;
	}

	int startIdx= jobsView->currentIndex().isValid() ? qBound(0, jobsView->currentIndex().row(), rows-1) : 0;

	for(int i = 0; i < rows; i++)
	{
		int currentIdx = (i + startIdx) % rows;
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(currentIdx, 0, QModelIndex()));
		if(status == EncodeThread::JobStatus_Enqueued)
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
	static const char *binFiles = "x264.exe:x264_x64.exe:avs2yuv.exe:avs2yuv_x64.exe";
	QStringList binaries = QString::fromLatin1(binFiles).split(":", QString::SkipEmptyParts);

	updateLabelPos();

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
			if(GetBinaryType(QWCHAR(file->fileName()), &binaryType))
			{
				binaryTypeOkay = (binaryType == SCS_32BIT_BINARY || binaryType == SCS_64BIT_BINARY);
			}
			if(!binaryTypeOkay)
			{
				QMessageBox::critical(this, tr("Invalid File!"), tr("<nobr>At least on required tool is not a valid Win32 or Win64 binary:<br>%1<br><br>Please re-install the program in order to fix the problem!</nobr>").arg(QDir::toNativeSeparators(QString("%1/toolset/%2").arg(m_appDir, current))).replace("-", "&minus;"));
				qFatal(QString("Binary is invalid: %1/toolset/%2").arg(m_appDir, current).toLatin1().constData());
				close(); qApp->exit(-1); return;
			}
			m_toolsList << file;
		}
		else
		{
			X264_DELETE(file);
			QMessageBox::critical(this, tr("File Not Found!"), tr("<nobr>At least on required tool could not be found:<br>%1<br><br>Please re-install the program in order to fix the problem!</nobr>").arg(QDir::toNativeSeparators(QString("%1/toolset/%2").arg(m_appDir, current))).replace("-", "&minus;"));
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

	//Check for Avisynth support
	bool avsAvailable = false;
	QLibrary *avsLib = new QLibrary("avisynth.dll");
	if(avsLib->load())
	{
		avsAvailable = (avsLib->resolve("avs_create_script_environment") != NULL);
	}
	if(!avsAvailable)
	{
		avsLib->unload(); X264_DELETE(avsLib);
		int val = QMessageBox::warning(this, tr("Avisynth Missing"), tr("<nobr>It appears that Avisynth is <b>not</b> currently installed on your computer.<br>Thus Avisynth (.avs) input will <b>not</b> be working at all!<br><br>Please download and install Avisynth:<br><a href=\"http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/\">http://sourceforge.net/projects/avisynth2/files/AviSynth 2.5/</a></nobr>").replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
		if(val != 1) { close(); qApp->exit(-1); return; }
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
	if(int totalFiles = files.count())
	{
		bool ok = true; int n = 0;
		while((!files.isEmpty()) && ok)
		{
			QString currentFile = files.takeFirst();
			qDebug("Adding file: %s", currentFile.toUtf8().constData());
			addButtonPressed(currentFile, n++, totalFiles, &ok);
		}
	}
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
		int totalFiles = droppedFiles.count();
		bool ok = true; int n = 0;
		while((!droppedFiles.isEmpty()) && ok)
		{
			QString currentFile = droppedFiles.takeFirst();
			qDebug("Adding file: %s", currentFile.toUtf8().constData());
			addButtonPressed(currentFile, n++, totalFiles, &ok);
		}
	}
	qDebug("Leave from MainWindow::handleDroppedFiles!");
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
	QStringList formats = event->mimeData()->formats();
	
	if(formats.contains("application/x-qt-windows-mime;value=\"FileNameW\"", Qt::CaseInsensitive) && formats.contains("text/uri-list", Qt::CaseInsensitive))
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
 * Jobs that are not completed (or failed, or aborted) yet
 */
unsigned int MainWindow::countPendingJobs(void)
{
	unsigned int count = 0;
	const int rows = m_jobList->rowCount(QModelIndex());

	for(int i = 0; i < rows; i++)
	{
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status != EncodeThread::JobStatus_Completed && status != EncodeThread::JobStatus_Aborted && status != EncodeThread::JobStatus_Failed)
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
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status != EncodeThread::JobStatus_Completed && status != EncodeThread::JobStatus_Aborted && status != EncodeThread::JobStatus_Failed && status != EncodeThread::JobStatus_Enqueued)
		{
			count++;
		}
	}

	return count;
}

/*
 * Update all buttons with respect to current job status
 */
void MainWindow::updateButtons(EncodeThread::JobStatus status)
{
	qDebug("MainWindow::updateButtons(void)");

	buttonStartJob->setEnabled(status == EncodeThread::JobStatus_Enqueued);
	buttonAbortJob->setEnabled(status == EncodeThread::JobStatus_Indexing || status == EncodeThread::JobStatus_Running || status == EncodeThread::JobStatus_Running_Pass1 || status == EncodeThread::JobStatus_Running_Pass2 || status == EncodeThread::JobStatus_Paused);
	buttonPauseJob->setEnabled(status == EncodeThread::JobStatus_Indexing || status == EncodeThread::JobStatus_Running || status == EncodeThread::JobStatus_Paused || status == EncodeThread::JobStatus_Running_Pass1 || status == EncodeThread::JobStatus_Running_Pass2);
	buttonPauseJob->setChecked(status == EncodeThread::JobStatus_Paused || status == EncodeThread::JobStatus_Pausing);

	actionJob_Delete->setEnabled(status == EncodeThread::JobStatus_Completed || status == EncodeThread::JobStatus_Aborted || status == EncodeThread::JobStatus_Failed || status == EncodeThread::JobStatus_Enqueued);
	actionJob_Browse->setEnabled(status == EncodeThread::JobStatus_Completed);

	actionJob_Start->setEnabled(buttonStartJob->isEnabled());
	actionJob_Abort->setEnabled(buttonAbortJob->isEnabled());
	actionJob_Pause->setEnabled(buttonPauseJob->isEnabled());
	actionJob_Pause->setChecked(buttonPauseJob->isChecked());

	editDetails->setEnabled(status != EncodeThread::JobStatus_Paused);
}

/*
 * Update the taskbar with current job status
 */
void MainWindow::updateTaskbar(EncodeThread::JobStatus status, const QIcon &icon)
{
	qDebug("MainWindow::updateTaskbar(void)");

	switch(status)
	{
	case EncodeThread::JobStatus_Undefined:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarNoState);
		break;
	case EncodeThread::JobStatus_Aborting:
	case EncodeThread::JobStatus_Starting:
	case EncodeThread::JobStatus_Pausing:
	case EncodeThread::JobStatus_Resuming:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarIndeterminateState);
		break;
	case EncodeThread::JobStatus_Aborted:
	case EncodeThread::JobStatus_Failed:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarErrorState);
		break;
	case EncodeThread::JobStatus_Paused:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarPausedState);
		break;
	default:
		WinSevenTaskbar::setTaskbarState(this, WinSevenTaskbar::WinSevenTaskbarNormalState);
		break;
	}

	switch(status)
	{
	case EncodeThread::JobStatus_Aborting:
	case EncodeThread::JobStatus_Starting:
	case EncodeThread::JobStatus_Pausing:
	case EncodeThread::JobStatus_Resuming:
		break;
	default:
		WinSevenTaskbar::setTaskbarProgress(this, progressBar->value(), progressBar->maximum());
		break;
	}

	WinSevenTaskbar::setOverlayIcon(this, icon.isNull() ? NULL : &icon);
}
