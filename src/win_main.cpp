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

#include "global.h"
#include "model_jobList.h"
#include "model_options.h"
#include "win_addJob.h"

#include <QDate>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QLibrary>

const char *home_url = "http://mulder.brhack.net/";

#define PRE_RELEASE (1)

#define SET_FONT_BOLD(WIDGET,BOLD) { QFont _font = WIDGET->font(); _font.setBold(BOLD); WIDGET->setFont(_font); }
#define SET_TEXT_COLOR(WIDGET,COLOR) { QPalette _palette = WIDGET->palette(); _palette.setColor(QPalette::WindowText, (COLOR)); _palette.setColor(QPalette::Text, (COLOR)); WIDGET->setPalette(_palette); }

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(bool x64supported)
:
	m_x64supported(x64supported),
	m_appDir(QApplication::applicationDirPath()),
	m_firstShow(true)
{
	//Init the dialog, from the .ui file
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowMaximizeButtonHint));

	//Register meta types
	qRegisterMetaType<QUuid>("QUuid");
	qRegisterMetaType<EncodeThread::JobStatus>("EncodeThread::JobStatus");

	//Freeze minimum size
	setMinimumSize(size());
	splitter->setSizes(QList<int>() << 16 << 196);

	//Update title
	labelBuildDate->setText(tr("Built on %1 at %2").arg(x264_version_date().toString(Qt::ISODate), QString::fromLatin1(x264_version_time())));
	setWindowTitle(QString("%1 (%2 Mode)").arg(windowTitle(), m_x64supported ? "64-Bit" : "32-Bit"));
	if(PRE_RELEASE) setWindowTitle(QString("%1 | PRE-RELEASE VERSION").arg(windowTitle()));

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
	logView->addAction(actionClipboard);
	connect(actionClipboard, SIGNAL(triggered(bool)), this, SLOT(copyLogToClipboard(bool)));
	jobsView->addActions(menuJob->actions());

	//Enable buttons
	connect(buttonAddJob, SIGNAL(clicked()), this, SLOT(addButtonPressed()));
	connect(buttonStartJob, SIGNAL(clicked()), this, SLOT(startButtonPressed()));
	connect(buttonAbortJob, SIGNAL(clicked()), this, SLOT(abortButtonPressed()));

	//Enable menu
	connect(actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
	connect(actionWebMulder, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebX264, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebKomisar, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebJarod, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebWiki, SIGNAL(triggered()), this, SLOT(showWebLink()));
	connect(actionWebBluRay, SIGNAL(triggered()), this, SLOT(showWebLink()));

	//Create floating label
	m_label = new QLabel(jobsView);
	m_label->setText(tr("No job created yet. Please click the 'Add New Job' button!"));
	m_label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	SET_TEXT_COLOR(m_label, Qt::darkGray);
	SET_FONT_BOLD(m_label, true);
	m_label->setVisible(true);
	m_label->setContextMenuPolicy(Qt::ActionsContextMenu);
	m_label->addActions(jobsView->actions());
	connect(splitter, SIGNAL(splitterMoved(int, int)), this, SLOT(updateLabel()));
	updateLabel();

	//Create options object
	m_options = new OptionsModel();
}

MainWindow::~MainWindow(void)
{
	X264_DELETE(m_jobList);
	X264_DELETE(m_options);
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

void MainWindow::addButtonPressed(void)
{
	AddJobDialog *addDialog = new AddJobDialog(this, m_options);
	addDialog->setRunImmediately(!havePendingJobs());
	int result = addDialog->exec();
	
	if(result == QDialog::Accepted)
	{
		
		EncodeThread *thrd = new EncodeThread
		(
			addDialog->sourceFile(),
			addDialog->outputFile(),
			m_options,
			QString("%1/toolset").arg(m_appDir),
			m_x64supported
		);

		QModelIndex newIndex = m_jobList->insertJob(thrd);

		if(addDialog->runImmediately())
		{
			jobsView->selectRow(newIndex.row());
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
			m_jobList->startJob(newIndex);
		}

		m_label->setVisible(false);
	}

	X264_DELETE(addDialog);
}

void MainWindow::startButtonPressed(void)
{
	m_jobList->startJob(jobsView->currentIndex());
}

void MainWindow::abortButtonPressed(void)
{
	m_jobList->abortJob(jobsView->currentIndex());
}

void MainWindow::jobSelected(const QModelIndex & current, const QModelIndex & previous)
{
	qDebug("Job selected: %d", current.row());
	
	if(logView->model())
	{
		disconnect(logView->model(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(jobLogExtended(QModelIndex, int, int)));
	}
	
	logView->setModel(m_jobList->getLogFile(current));
	connect(logView->model(), SIGNAL(rowsInserted(QModelIndex, int, int)), this, SLOT(jobLogExtended(QModelIndex, int, int)));
	QTimer::singleShot(0, logView, SLOT(scrollToBottom()));
	
	progressBar->setValue(m_jobList->getJobProgress(current));
	editDetails->setText(m_jobList->data(m_jobList->index(current.row(), 3, QModelIndex()), Qt::DisplayRole).toString());
	updateButtons(m_jobList->getJobStatus(current));

	progressBar->repaint();
}

void MainWindow::jobChangedData(const QModelIndex &topLeft, const  QModelIndex &bottomRight)
{
	int selected = jobsView->currentIndex().row();
	
	if(topLeft.column() <= 1 && bottomRight.column() >= 1)
	{
		for(int i = topLeft.row(); i <= bottomRight.row(); i++)
		{
			EncodeThread::JobStatus status =  m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
			if(i == selected)
			{
				qDebug("Current job changed status!");
				updateButtons(status);
			}
			if(status == EncodeThread::JobStatus_Completed)
			{
				QTimer::singleShot(0, this, SLOT(launchNextJob()));
			}
		}
	}
	else if(topLeft.column() <= 2 && bottomRight.column() >= 2)
	{
		for(int i = topLeft.row(); i <= bottomRight.row(); i++)
		{
			if(i == selected)
			{
				progressBar->setValue(m_jobList->getJobProgress(m_jobList->index(i, 0, QModelIndex())));
				break;
			}
		}
	}
	else if(topLeft.column() <= 3 && bottomRight.column() >= 3)
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

void MainWindow::jobLogExtended(const QModelIndex & parent, int start, int end)
{
	QTimer::singleShot(0, logView, SLOT(scrollToBottom()));
}

void MainWindow::showAbout(void)
{
	QString text;

	text += QString().sprintf("<nobr><tt>Simple x264 Launcher v%u.%02u - use 64-Bit x264 with 32-Bit Avisynth<br>", x264_version_major(), x264_version_minor());
	text += QString().sprintf("Copyright (c) 2004-%04d LoRd_MuldeR &lt;mulder2@gmx.de&gt;. Some rights reserved.<br>", qMax(x264_version_date().year(),QDate::currentDate().year()));
	text += QString().sprintf("Built on %s at %s with %s for Win-%s.<br><br>", x264_version_date().toString(Qt::ISODate).toLatin1().constData(), x264_version_time(), x264_version_compiler(), x264_version_arch());
	text += QString().sprintf("This program is free software: you can redistribute it and/or modify<br>");
	text += QString().sprintf("it under the terms of the GNU General Public License &lt;http://www.gnu.org/&gt;.<br>");
	text += QString().sprintf("Note that this program is distributed with ABSOLUTELY NO WARRANTY.<br><br>");
	text += QString().sprintf("Please check the web-site at <a href=\"%s\">%s</a> for updates !!!<br></tt></nobr>", home_url, home_url);

	QMessageBox::information(this, tr("About..."), text.replace("-", "&minus;"));
}

void MainWindow::showWebLink(void)
{
	if(QObject::sender() == actionWebMulder) QDesktopServices::openUrl(QUrl(home_url));
	if(QObject::sender() == actionWebX264) QDesktopServices::openUrl(QUrl("http://www.x264.com/"));
	if(QObject::sender() == actionWebKomisar) QDesktopServices::openUrl(QUrl("http://komisar.gin.by/"));
	if(QObject::sender() == actionWebJarod) QDesktopServices::openUrl(QUrl("http://www.x264.nl/"));
	if(QObject::sender() == actionWebWiki) QDesktopServices::openUrl(QUrl("http://mewiki.project357.com/wiki/X264_Settings"));
	if(QObject::sender() == actionWebBluRay) QDesktopServices::openUrl(QUrl("http://www.x264bluray.com/"));
}

void MainWindow::launchNextJob(void)
{
	const int rows = m_jobList->rowCount(QModelIndex());

	for(int i = 0; i < rows; i++)
	{
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status == EncodeThread::JobStatus_Running || status == EncodeThread::JobStatus_Running_Pass1 || status == EncodeThread::JobStatus_Running_Pass2)
		{
			qWarning("Still have a job running, won't launch next yet!");
			return;
		}
	}

	for(int i = 0; i < rows; i++)
	{
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status == EncodeThread::JobStatus_Enqueued)
		{
			m_jobList->startJob(m_jobList->index(i, 0, QModelIndex()));
			return;
		}
	}

	qWarning("No enqueued jobs left!");
}

void MainWindow::init(void)
{
	static const char *binFiles = "x264.exe:x264_x64.exe:avs2yuv.exe";
	QStringList binaries = QString::fromLatin1(binFiles).split(":", QString::SkipEmptyParts);

	updateLabel();

	//Check all binaries
	while(!binaries.isEmpty())
	{
		QString current = binaries.takeFirst();
		QFile *file = new QFile(QString("%1/toolset/%2").arg(m_appDir, current));
		if(file->open(QIODevice::ReadOnly))
		{
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

	//Pre-release popup
	if(PRE_RELEASE)
	{
		qsrand(time(NULL)); int rnd = qrand() % 3;
		int val = QMessageBox::information(this, tr("Pre-Release Version"), tr("Note: This is a pre-release version. Please do NOT use for production!<br>Click the button #%1 in order to continue...<br><br>(There will be no such message box in the final version of this application)").arg(QString::number(rnd + 1)), tr("(1)"), tr("(2)"), tr("(3)"), qrand() % 3);
		if(rnd != val) { close(); qApp->exit(-1); return; }
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
		int val = QMessageBox::warning(this, tr("Avisynth Missing"), tr("<nobr>It appears that Avisynth is not currently installed on your computer.<br>Thus Avisynth input will not be working at all!<br><br>Please download and install Avisynth:<br><a href=\"http://sourceforge.net/projects/avisynth2/files/AviSynth%202.5/\">http://sourceforge.net/projects/avisynth2/files/AviSynth 2.5/</a></nobr>").replace("-", "&minus;"), tr("Quit"), tr("Ignore"));
		if(val != 1) { close(); qApp->exit(-1); return; }
	}
}

void MainWindow::updateLabel(void)
{
	m_label->setGeometry(0, 0, jobsView->width(), jobsView->height());
}

void MainWindow::copyLogToClipboard(bool checked)
{
	qDebug("copyLogToClipboard");
	
	if(LogFileModel *log = dynamic_cast<LogFileModel*>(logView->model()))
	{
		log->copyToClipboard();
		MessageBeep(MB_ICONINFORMATION);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Event functions
///////////////////////////////////////////////////////////////////////////////

void MainWindow::showEvent(QShowEvent *e)
{
	QMainWindow::showEvent(e);

	if(m_firstShow)
	{
		m_firstShow = false;
		QTimer::singleShot(0, this, SLOT(init()));
	}
}

void MainWindow::closeEvent(QCloseEvent *e)
{
	if(havePendingJobs())
	{
		e->ignore();
		MessageBeep(MB_ICONWARNING);
		return;
	}

	QMainWindow::closeEvent(e);
}

void MainWindow::resizeEvent(QResizeEvent *e)
{
	QMainWindow::resizeEvent(e);
	updateLabel();
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

bool MainWindow::havePendingJobs(void)
{
	const int rows = m_jobList->rowCount(QModelIndex());

	for(int i = 0; i < rows; i++)
	{
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status != EncodeThread::JobStatus_Completed && status != EncodeThread::JobStatus_Aborted && status != EncodeThread::JobStatus_Failed)
		{
			return true;
		}
	}

	return false;
}

void MainWindow::updateButtons(EncodeThread::JobStatus status)
{
	qDebug("MainWindow::updateButtons(void)");

	buttonStartJob->setEnabled(status == EncodeThread::JobStatus_Enqueued);
	buttonAbortJob->setEnabled(status == EncodeThread::JobStatus_Indexing || status == EncodeThread::JobStatus_Running ||
		status == EncodeThread::JobStatus_Running_Pass1 || status == EncodeThread::JobStatus_Running_Pass2 );

	actionJob_Start->setEnabled(buttonStartJob->isEnabled());
	actionJob_Abort->setEnabled(buttonAbortJob->isEnabled());
}
