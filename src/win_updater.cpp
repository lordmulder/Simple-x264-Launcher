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

#include "win_updater.h"
#include "uic_win_updater.h"

#include "global.h"
#include "thread_updater.h"
#include "checksum.h"

#include <QMovie>
#include <QCloseEvent>
#include <QTimer>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QMap>

///////////////////////////////////////////////////////////////////////////////

#define UPDATE_TEXT(N, TEXT) ui->label_phase##N->setText((TEXT))
#define UPDATE_ICON(N, ICON) ui->icon_phase##N->setPixmap(QIcon(":/buttons/" ICON ".png").pixmap(16, 16))

#define SHOW_ANIMATION(FLAG) do  \
{ \
	ui->labelLoadingLeft->setVisible((FLAG)); \
	ui->labelLoadingCenter->setVisible((FLAG)); \
	ui->labelLoadingRight->setVisible((FLAG)); \
	ui->labelBuildNo->setVisible(!(FLAG)); \
	ui->labelInfo->setVisible(!(FLAG)); \
	ui->labelUrl->setVisible(!(FLAG)); \
	if((FLAG)) m_animator->start(); else m_animator->stop(); \
} \
while(0)


///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

UpdaterDialog::UpdaterDialog(QWidget *parent, const QString &binDir)
:
	QDialog(parent),
	ui(new Ui::UpdaterDialog()),
	m_binDir(binDir),
	m_status(UpdateCheckThread::UpdateStatus_NotStartedYet),
	m_thread(NULL),
	m_updaterProcess(NULL),
	m_success(false),
	m_firstShow(true)
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

	//Fix size
	setFixedSize(size());

	//Enable buttons
	connect(ui->buttonCancel, SIGNAL(clicked()), this, SLOT(close()));
	connect(ui->buttonDownload, SIGNAL(clicked()), this, SLOT(installUpdate()));
	connect(ui->buttonRetry, SIGNAL(clicked()), this, SLOT(checkForUpdates()));
	
	//Enable info label
	connect(ui->labelUrl, SIGNAL(linkActivated(QString)), this, SLOT(openUrl(QString)));
	
	//Init animation
	m_animator = new QMovie(":/images/loading.gif");
	ui->labelLoadingCenter->setMovie(m_animator);
	m_animator->start();

	//Init buttons
	ui->buttonCancel->setEnabled(false);
	ui->buttonRetry->hide();
	ui->buttonDownload->hide();

	//Hide labels
	ui->labelInfo->hide();
	ui->labelUrl->hide();
	ui->labelBuildNo->hide();
}

UpdaterDialog::~UpdaterDialog(void)
{
	if(m_thread)
	{
		if(!m_thread->wait(1000))
		{
			m_thread->terminate();
			m_thread->wait();
		}
	}

	if((!m_keysFile.isEmpty()) && QFile::exists(m_keysFile))
	{
		QFile::setPermissions(m_keysFile, QFile::ReadOwner | QFile::WriteOwner);
		QFile::remove(m_keysFile);
		m_keysFile.clear();
	}

	X264_DELETE(m_thread);
	X264_DELETE(m_animator);

	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
// Public Functions
///////////////////////////////////////////////////////////////////////////////

/*None yet*/

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

bool UpdaterDialog::event(QEvent *e)
{
	if((e->type() == QEvent::ActivationChange) && (m_updaterProcess != NULL))
	{
		x264_bring_process_to_front(m_updaterProcess);
	}
	return QDialog::event(e);
}

void UpdaterDialog::showEvent(QShowEvent *event)
{
	if(m_firstShow)
	{
		m_firstShow = false;
		QTimer::singleShot(16, this, SLOT(initUpdate()));
	}
}

void UpdaterDialog::closeEvent(QCloseEvent *e)
{
	if(!ui->buttonCancel->isEnabled())
	{
		e->ignore();
	}
}

void UpdaterDialog::keyPressEvent(QKeyEvent *event)
{
	if(event->key() == Qt::Key_F11)
	{
		QFile logFile(QString("%1/%2.log").arg(x264_temp_directory(), x264_rand_str()));
		if(logFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
		{
			logFile.write("\xEF\xBB\xBF");
			for(QStringList::ConstIterator iter = m_logFile.constBegin(); iter != m_logFile.constEnd(); iter++)
			{
				logFile.write(iter->toUtf8());
				logFile.write("\r\n");
			}
			logFile.close();
			QDesktopServices::openUrl(QUrl::fromLocalFile(logFile.fileName()));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void UpdaterDialog::initUpdate(void)
{
	//Check binary files
	QString wgetBin, gpgvBin;
	if(!checkBinaries(wgetBin, gpgvBin))
	{
		ui->buttonCancel->setEnabled(true);
		QMessageBox::critical(this, tr("File Error"), tr("At least one file required by web-update is missing or corrupted.<br>Please re-install this application and then try again!"));
		close();
		return;
	}
	
	//Create and setup thread
	if(!m_thread)
	{
		m_thread = new UpdateCheckThread(wgetBin, gpgvBin, m_keysFile, false);
		connect(m_thread, SIGNAL(statusChanged(int)), this, SLOT(threadStatusChanged(int)));
		connect(m_thread, SIGNAL(finished()), this, SLOT(threadFinished()));
		connect(m_thread, SIGNAL(terminated()), this, SLOT(threadFinished()));
		connect(m_thread, SIGNAL(messageLogged(QString)), this, SLOT(threadMessageLogged(QString)));
	}

	//Begin updater run
	QTimer::singleShot(16, this, SLOT(checkForUpdates()));
}

void UpdaterDialog::checkForUpdates(void)
{
	if((!m_thread) || m_thread->isRunning())
	{
		qWarning("Update in progress, cannot check for updates now!");
	}

	//Clear texts
	ui->retranslateUi(this);
	ui->labelBuildNo->setText(tr("Installed build is #%1  |  Latest build is #%2").arg(QString::number(x264_version_build()), tr("N/A")));

	//Init buttons
	ui->buttonCancel->setEnabled(false);
	ui->buttonRetry->hide();
	ui->buttonDownload->hide();

	//Hide labels
	ui->labelInfo->hide();
	ui->labelUrl->hide();

	//Update status
	threadStatusChanged(UpdateCheckThread::UpdateStatus_NotStartedYet);

	//Start animation
	SHOW_ANIMATION(true);

	//Update cursor
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	QApplication::setOverrideCursor(Qt::WaitCursor);

	//Clear log
	m_logFile.clear();

	//Start the updater thread
	QTimer::singleShot(250, m_thread, SLOT(start()));
}

void UpdaterDialog::threadStatusChanged(int status)
{
	switch(m_status = status)
	{
	case UpdateCheckThread::UpdateStatus_NotStartedYet:
		UPDATE_ICON(1, "clock");
		UPDATE_ICON(2, "clock");
		UPDATE_ICON(3, "clock");
		break;
	case UpdateCheckThread::UpdateStatus_CheckingConnection:
		UPDATE_ICON(1, "play");
		break;
	case UpdateCheckThread::UpdateStatus_FetchingUpdates:
		UPDATE_ICON(1, "shield_green");
		UPDATE_TEXT(1, tr("Internet connection is working."));
		UPDATE_ICON(2, "play");
		break;
	case UpdateCheckThread::UpdateStatus_ErrorNoConnection:
		UPDATE_ICON(1, "shield_error");
		UPDATE_TEXT(1, tr("Computer is currently offline!"));
		UPDATE_ICON(2, "shield_grey");
		UPDATE_ICON(3, "shield_grey");
		break;
	case UpdateCheckThread::UpdateStatus_ErrorConnectionTestFailed:
		UPDATE_ICON(1, "shield_error");
		UPDATE_TEXT(1, tr("Internet connectivity test failed!"));
		UPDATE_ICON(2, "shield_grey");
		UPDATE_ICON(3, "shield_grey");
		break;
	case UpdateCheckThread::UpdateStatus_ErrorFetchUpdateInfo:
		UPDATE_ICON(2, "shield_error");
		UPDATE_TEXT(2, tr("Failed to download the update information!"));
		UPDATE_ICON(3, "shield_grey");
		break;
	case UpdateCheckThread::UpdateStatus_CompletedUpdateAvailable:
	case UpdateCheckThread::UpdateStatus_CompletedNoUpdates:
	case UpdateCheckThread::UpdateStatus_CompletedNewVersionOlder:
		UPDATE_ICON(2, "shield_green");
		UPDATE_TEXT(2, tr("Update information received successfully."));
		UPDATE_ICON(3, "play");
		break;
	default:
		throw "Unknown status code!";
	}
}

void UpdaterDialog::threadFinished(void)
{
	m_success = m_thread->getSuccess();
	QTimer::singleShot((m_success ? 1000 : 0), this, SLOT(updateFinished()));
}

void UpdaterDialog::updateFinished(void)
{
	//Restore cursor
	QApplication::restoreOverrideCursor();

	//If update was successfull, process final updater state
	if(m_thread->getSuccess())
	{
		switch(m_status)
		{
		case UpdateCheckThread::UpdateStatus_CompletedUpdateAvailable:
			UPDATE_ICON(3, "shield_exclamation");
			UPDATE_TEXT(3, tr("A newer version is available!"));
			ui->buttonDownload->show();
			break;
		case UpdateCheckThread::UpdateStatus_CompletedNoUpdates:
			UPDATE_ICON(3, "shield_green");
			UPDATE_TEXT(3, tr("Your version is up-to-date."));
			break;
		case UpdateCheckThread::UpdateStatus_CompletedNewVersionOlder:
			UPDATE_ICON(3, "shield_blue");
			UPDATE_TEXT(3, tr("You are using a pre-release version!"));
			break;
		default:
			qWarning("Update thread succeeded with unexpected status code: %d", m_status);
		}
	}

	//Show update info or retry button
	switch(m_status)
	{
	case UpdateCheckThread::UpdateStatus_CompletedUpdateAvailable:
	case UpdateCheckThread::UpdateStatus_CompletedNoUpdates:
	case UpdateCheckThread::UpdateStatus_CompletedNewVersionOlder:
		SHOW_ANIMATION(false);
		ui->labelBuildNo->setText(tr("Installed build is #%1  |  Latest build is #%2").arg(QString::number(x264_version_build()), QString::number(m_thread->getUpdateInfo()->m_buildNo)));
		ui->labelUrl->setText(QString("<a href=\"%1\">%1</a>").arg(m_thread->getUpdateInfo()->m_downloadSite));
		break;
	case UpdateCheckThread::UpdateStatus_ErrorNoConnection:
	case UpdateCheckThread::UpdateStatus_ErrorConnectionTestFailed:
	case UpdateCheckThread::UpdateStatus_ErrorFetchUpdateInfo:
		m_animator->stop();
		ui->buttonRetry->show();
		break;
	default:
		qWarning("Update thread finished with unexpected status code: %d", m_status);
	}

	//Re-enbale cancel button
	ui->buttonCancel->setEnabled(true);
	
}

void UpdaterDialog::threadMessageLogged(const QString &message)
{
	m_logFile << message;
}

void UpdaterDialog::openUrl(const QString &url)
{
	qDebug("Open URL: %s", url.toLatin1().constData());
	QDesktopServices::openUrl(QUrl(url));
}

void UpdaterDialog::installUpdate(void)
{
	if(!((m_thread) && m_thread->getSuccess()))
	{
		qWarning("Cannot download/install update at this point!");
		return;
	}

	QApplication::setOverrideCursor(Qt::WaitCursor);
	ui->buttonDownload->hide();
	ui->buttonCancel->setEnabled(false);
	SHOW_ANIMATION(true);

	const UpdateInfo *updateInfo = m_thread->getUpdateInfo();

	QProcess process;
	QStringList args;
	QEventLoop loop;

	x264_init_process(process, x264_temp_directory(), false);

	connect(&process, SIGNAL(error(QProcess::ProcessError)), &loop, SLOT(quit()));
	connect(&process, SIGNAL(finished(int,QProcess::ExitStatus)), &loop, SLOT(quit()));

	args << QString("/Location=%1").arg(updateInfo->m_downloadAddress);
	args << QString("/Filename=%1").arg(updateInfo->m_downloadFilename);
	args << QString("/TicketID=%1").arg(updateInfo->m_downloadFilecode);
	args << QString("/ToFolder=%1").arg(QDir::toNativeSeparators(QDir(QApplication::applicationDirPath()).canonicalPath())); 
	args << QString("/ToExFile=%1.exe").arg(QFileInfo(QFileInfo(QApplication::applicationFilePath()).canonicalFilePath()).completeBaseName());
	args << QString("/AppTitle=Simple x264 Launcher (Build #%1)").arg(QString::number(updateInfo->m_buildNo));

	process.start(m_wupdFile, args);
	if(!process.waitForStarted())
	{
		QApplication::restoreOverrideCursor();
		SHOW_ANIMATION(false);
		QMessageBox::critical(this, tr("Update Failed"), tr("Sorry, failed to launch web-update program!"));
		ui->buttonDownload->show();
		ui->buttonCancel->setEnabled(true);
		return;
	}

	m_updaterProcess = x264_process_id(process);
	loop.exec(QEventLoop::ExcludeUserInputEvents);
	
	if(!process.waitForFinished())
	{
		process.kill();
		process.waitForFinished();
	}

	m_updaterProcess = NULL;
	QApplication::restoreOverrideCursor();
	ui->buttonDownload->show();
	ui->buttonCancel->setEnabled(true);
	SHOW_ANIMATION(false);

	if(process.exitCode() == 0)
	{
		done(READY_TO_INSTALL_UPDATE);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Private Functions
///////////////////////////////////////////////////////////////////////////////

bool UpdaterDialog::checkBinaries(QString &wgetBin, QString &gpgvBin)
{
	qDebug("[File Verification]");

	static struct
	{
		const char* name;
		const char* hash;
	}
	FILE_INFO[] =
	{
		{ "wget.exe", "7b522345239bcb95b5b0f7f50a883ba5957894a1feb769763e38ed789a8a0f63fead0155f54b9ffd0f1cdc5dfd855d207a6e7a8e4fd192589a8838ce646c504e" },
		{ "gpgv.exe", "e61d28e4c47b2422ceec7b8fc08f9c70f10a3056e3779a974026eb24fe09551eedc2e7f34fbe5ef8e844fab0dbe68b85c4ca69d63bf85d445f7cae152c17f589" },
		{ "gpgv.gpg", "58e0f0e462bbd0b5aa4f638801c1097da7da4b3eb38c8c88ad1db23705c0f11e174b083fa55fe76bd3ba196341c967833a6f3427d6f63ad8565900745535d8fa" },
		{ "wupd.exe", "9dd0ae5f386aaf5df9e261a53b81569df51bccf2f9290ed42f04a5b52b4a4e1118f2c9ce3a9b2492fd5ae597600411dfa47bd7c96e2fd7bee205d603324452a2" },
		{ NULL, NULL }
	};

	QMap<QString, QString> binaries;

	m_keysFile.clear();
	m_wupdFile.clear();
	wgetBin.clear();
	gpgvBin.clear();

	bool okay = true;

	for(size_t i = 0; FILE_INFO[i].name; i++)
	{
		const QString binPath = QString("%1/common/%2").arg(m_binDir, QString::fromLatin1(FILE_INFO[i].name));
		if(okay = okay && checkFileHash(binPath, FILE_INFO[i].hash))
		{
			binaries.insert(FILE_INFO[i].name, binPath);
		}
	}

	if(okay)
	{
		wgetBin = binaries.value("wget.exe");
		gpgvBin = binaries.value("gpgv.exe");

		m_wupdFile = binaries.value("wupd.exe");
		m_keysFile = QString("%1/%2.gpg").arg(x264_temp_directory(), x264_rand_str());

		if(okay = QFile::copy(binaries.value("gpgv.gpg"), m_keysFile))
		{
			QFile::setPermissions(m_keysFile, QFile::ReadOwner);
		}
		qDebug("%s\n", okay ? "Completed." : "Failed to copy GPG file!");
	}

	return okay;
}

bool UpdaterDialog::checkFileHash(const QString &filePath, const char *expectedHash)
{
	qDebug("Checking file: %s", filePath.toUtf8().constData());
	QBlake2Checksum checksum2;
	QFile file(filePath);
	if(file.open(QIODevice::ReadOnly))
	{
		checksum2.update(file);
		const QByteArray fileHash = checksum2.finalize();
		if((strlen(expectedHash) != fileHash.size()) || (memcmp(fileHash.constData(), expectedHash, fileHash.size()) != 0))
		{
			qWarning("\nFile appears to be corrupted:\n%s\n", filePath.toUtf8().constData());
			qWarning("Expected Hash: %s\nDetected Hash: %s\n", expectedHash, fileHash.constData());
			return false;
		}
		return true;
	}
	else
	{
		qWarning("Failed to open file:\n%s\n", filePath.toUtf8().constData());
		return false;
	}
}
