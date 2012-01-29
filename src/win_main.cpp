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
#include "win_addJob.h"

#include <QDate>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(bool x64supported)
:
	m_x64supported(x64supported)
{
	//Init the dialog, from the .ui file
	setupUi(this);
	setWindowFlags(windowFlags() ^ Qt::WindowMaximizeButtonHint);

	//Register meta types
	qRegisterMetaType<QUuid>("QUuid");
	qRegisterMetaType<EncodeThread::JobStatus>("EncodeThread::JobStatus");

	//Freeze minimum size
	setMinimumSize(size());

	//Update title
	setWindowTitle(QString("%1 [%2]").arg(windowTitle(), x264_version_date().toString(Qt::ISODate)));
	if(m_x64supported) setWindowTitle(QString("%1 (x64)").arg(windowTitle()));

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

	//Enable buttons
	connect(buttonAddJob, SIGNAL(clicked()), this, SLOT(addButtonPressed()));
	connect(buttonStartJob, SIGNAL(clicked()), this, SLOT(startButtonPressed()));
	connect(buttonAbortJob, SIGNAL(clicked()), this, SLOT(abortButtonPressed()));

	//Enable menu
	connect(actionAbout, SIGNAL(triggered()), this, SLOT(showAbout()));
}

MainWindow::~MainWindow(void)
{
	X264_DELETE(m_jobList);
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void MainWindow::addButtonPressed(void)
{
	AddJobDialog *addDialog = new AddJobDialog(this);
	int result = addDialog->exec();
	
	if(result == QDialog::Accepted)
	{
		EncodeThread *thrd = new EncodeThread();
		QModelIndex newIndex = m_jobList->insertJob(thrd);
		jobsView->selectRow(newIndex.row());
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
	const char *url = "http://mulder.brhack.net/";

	text += QString().sprintf("<nobr><tt>Simple x264 Launcher v%u.%02u - use 64-Bit x264 with 32-Bit Avisynth<br>", x264_version_major(), x264_version_minor());
	text += QString().sprintf("Copyright (c) 2004-%04d LoRd_MuldeR &lt;mulder2@gmx.de&gt;. Some rights reserved.<br>", qMax(x264_version_date().year(),QDate::currentDate().year()));
	text += QString().sprintf("Built on %s at %s with %s for Win-%s.<br><br>", x264_version_date().toString(Qt::ISODate).toLatin1().constData(), x264_version_time(), x264_version_compiler(), x264_version_arch());
	text += QString().sprintf("This program is free software: you can redistribute it and/or modify<br>");
	text += QString().sprintf("it under the terms of the GNU General Public License &lt;http://www.gnu.org/&gt;.<br>");
	text += QString().sprintf("Note that this program is distributed with ABSOLUTELY NO WARRANTY.<br><br>");
	text += QString().sprintf("Please check the web-site at <a href=\"%s\">%s</a> for updates !!!<br></tt></nobr>", url, url);

	QMessageBox::information(this, tr("About..."), text.replace("-", "&minus;"));
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

///////////////////////////////////////////////////////////////////////////////
// Event functions
///////////////////////////////////////////////////////////////////////////////

void MainWindow::closeEvent(QCloseEvent *e)
{
	const int rows = m_jobList->rowCount(QModelIndex());
	
	for(int i = 0; i < rows; i++)
	{
		EncodeThread::JobStatus status = m_jobList->getJobStatus(m_jobList->index(i, 0, QModelIndex()));
		if(status != EncodeThread::JobStatus_Completed && status != EncodeThread::JobStatus_Aborted && status != EncodeThread::JobStatus_Failed)
		{
			e->ignore();
			MessageBeep(MB_ICONWARNING);
			break;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

void MainWindow::updateButtons(EncodeThread::JobStatus status)
{
	qDebug("MainWindow::updateButtons(void)");

	buttonStartJob->setEnabled(status == EncodeThread::JobStatus_Enqueued);
	buttonAbortJob->setEnabled(status == EncodeThread::JobStatus_Indexing || status == EncodeThread::JobStatus_Running ||
		status == EncodeThread::JobStatus_Running_Pass1 || status == EncodeThread::JobStatus_Running_Pass2 );
}
