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

#include "win_updater.h"
#include "UIC_win_updater.h"

//Internal
#include "global.h"
#include "model_sysinfo.h"

//MUtils
#include <MUtils/UpdateChecker.h>
#include <MUtils/Hash_Blake2.h>
#include <MUtils/GUI.h>
#include <MUtils/OSSupport.h>
#include <MUtils/Exception.h>

//Qt
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

const UpdaterDialog::binary_t UpdaterDialog::BINARIES[] =
{
	{ "wget.exe", "7b522345239bcb95b5b0f7f50a883ba5957894a1feb769763e38ed789a8a0f63fead0155f54b9ffd0f1cdc5dfd855d207a6e7a8e4fd192589a8838ce646c504e", 1 },
	{ "netc.exe", "c199ea12d761fa3191006da250f8f600ad426265fdf4a43e551cdf04a451a105692efd3ef82ac621c0799394aa21ac65bfbb4bab90c3fbb1f557e93f490fcb75", 1 },
	{ "gpgv.exe", "18c5456cbb9ebf5cb9012a939b199d9eaa71c92a39f574f1e032babad0bbd9e72a064af96ca9d3d01f2892b064ec239fd61f27bac2eb9a64f7b2ece7beea3158", 1 },
	{ "gpgv.gpg", "745c7a9c040196d9d322b1580e0046ff26ec13238cfd04325ceb3d4c8948294c593c027f895dc8ec427295175003e75d34f083019b706b0f4f06f81cce8df47d", 0 },
	{ "wupd.exe", "28a5fd1515e27180a4659a9c1eee10e7483d25c556a6a142bce88176d0ac287105c0bd315fa5ece0a1a145fa64465378997a484f7a9ffffb28743c80f2c12ccd", 1 },
	{ NULL, NULL, 0 }
};

#define UPDATE_TEXT(N, TEXT) ui->label_phase##N->setText((TEXT))
#define UPDATE_ICON(N, ICON) ui->icon_phase##N->setPixmap(QIcon(":/buttons/" ICON ".png").pixmap(16, 16))

#define SHOW_ANIMATION(FLAG) do  \
{ \
	ui->frameAnimation->setVisible((FLAG)); \
	ui->labelInfo->setVisible(!(FLAG)); \
	ui->labelUrl->setVisible(!(FLAG)); \
	ui->labelBuildNo->setVisible(!(FLAG)); \
	if((FLAG)) m_animator->start(); else m_animator->stop(); \
} \
while(0)


///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

UpdaterDialog::UpdaterDialog(QWidget *parent, const SysinfoModel *sysinfo, const char *const updateUrl)
:
	QDialog(parent),
	ui(new Ui::UpdaterDialog()),
	m_sysinfo(sysinfo),
	m_updateUrl(updateUrl),
	m_status(MUtils::UpdateChecker::UpdateStatus_NotStartedYet),
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
	m_animator.reset(new QMovie(":/images/loading.gif"));
	ui->labelLoadingCenter->setMovie(m_animator.data());

	//Init buttons
	ui->buttonCancel->setEnabled(false);
	ui->buttonRetry->hide();
	ui->buttonDownload->hide();

	//Start animation
	SHOW_ANIMATION(true);
}

UpdaterDialog::~UpdaterDialog(void)
{
	if(!m_thread.isNull())
	{
		if(!m_thread->wait(5000))
		{
			m_thread->terminate();
			m_thread->wait();
		}
	}

	cleanFiles();
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
		MUtils::GUI::bring_to_front(m_updaterProcess);
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
		QFile logFile(QString("%1/%2.log").arg(MUtils::temp_folder(), MUtils::rand_str()));
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
	//Clean up files from previous attempt
	if(!m_binaries.isEmpty())
	{
		cleanFiles();
	}

	//Check binary files
	if(!checkBinaries())
	{
		ui->buttonCancel->setEnabled(true);
		const QString message = QString("%1<br><br><nobr><a href=\"%2\">%3</a></nobr><br>").arg(tr("At least one file required by the web-update tool is missing or corrupted.<br>Please re-install this application and then try again!"), QString::fromLatin1(m_updateUrl), QString::fromLatin1(m_updateUrl).replace("-", "&minus;"));
		if(QMessageBox::critical(this, tr("File Error"), message, tr("Download Latest Version"), tr("Discard")) == 0)
		{
			QDesktopServices::openUrl(QUrl(QString::fromLatin1(m_updateUrl)));
		}
		close(); return;
	}

	//Make sure user does have admin access
	if(!MUtils::OS::user_is_admin())
	{
		qWarning("User is not in the \"admin\" group, cannot update!");
		QString message;
		message += QString("<nobr>%1</nobr><br>").arg(tr("Sorry, but only users in the \"Administrators\" group can install updates."));
		message += QString("<nobr>%1</nobr>").arg(tr("Please start application from an administrator account and try again!"));
		if(QMessageBox::critical(this, this->windowTitle(), message, tr("Discard"), tr("Ignore")) != 1)
		{
			ui->buttonCancel->setEnabled(true);
			close(); return;
		}
	}
	
	//Create and setup thread
	if(!m_thread)
	{
		m_thread.reset(new MUtils::UpdateChecker(m_binaries.value("wget.exe"), m_binaries.value("netc.exe"), m_binaries.value("gpgv.exe"), m_binaries.value("gpgv.gpg"), "Simple x264 Launcher", x264_version_build(), false));
		connect(m_thread.data(), SIGNAL(statusChanged(int)), this, SLOT(threadStatusChanged(int)));
		connect(m_thread.data(), SIGNAL(finished()), this, SLOT(threadFinished()));
		connect(m_thread.data(), SIGNAL(terminated()), this, SLOT(threadFinished()));
		connect(m_thread.data(), SIGNAL(messageLogged(QString)), this, SLOT(threadMessageLogged(QString)));
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
	threadStatusChanged(MUtils::UpdateChecker::UpdateStatus_NotStartedYet);

	//Start animation
	SHOW_ANIMATION(true);

	//Update cursor
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	QApplication::setOverrideCursor(Qt::WaitCursor);

	//Clear log
	m_logFile.clear();

	//Start the updater thread
	QTimer::singleShot(250, m_thread.data(), SLOT(start()));
}

void UpdaterDialog::threadStatusChanged(int status)
{
	switch(m_status = status)
	{
	case MUtils::UpdateChecker::UpdateStatus_NotStartedYet:
		UPDATE_ICON(1, "clock");
		UPDATE_ICON(2, "clock");
		UPDATE_ICON(3, "clock");
		break;
	case MUtils::UpdateChecker::UpdateStatus_CheckingConnection:
		UPDATE_ICON(1, "play");
		break;
	case MUtils::UpdateChecker::UpdateStatus_FetchingUpdates:
		UPDATE_ICON(1, "shield_green");
		UPDATE_TEXT(1, tr("Internet connection is working."));
		UPDATE_ICON(2, "play");
		break;
	case MUtils::UpdateChecker::UpdateStatus_ErrorNoConnection:
		UPDATE_ICON(1, "shield_error");
		UPDATE_TEXT(1, tr("Computer is currently offline!"));
		UPDATE_ICON(2, "shield_grey");
		UPDATE_ICON(3, "shield_grey");
		break;
	case MUtils::UpdateChecker::UpdateStatus_ErrorConnectionTestFailed:
		UPDATE_ICON(1, "shield_error");
		UPDATE_TEXT(1, tr("Internet connectivity test failed!"));
		UPDATE_ICON(2, "shield_grey");
		UPDATE_ICON(3, "shield_grey");
		break;
	case MUtils::UpdateChecker::UpdateStatus_ErrorFetchUpdateInfo:
		UPDATE_ICON(2, "shield_error");
		UPDATE_TEXT(2, tr("Failed to download the update information!"));
		UPDATE_ICON(3, "shield_grey");
		break;
	case MUtils::UpdateChecker::UpdateStatus_CompletedUpdateAvailable:
	case MUtils::UpdateChecker::UpdateStatus_CompletedNoUpdates:
	case MUtils::UpdateChecker::UpdateStatus_CompletedNewVersionOlder:
		UPDATE_ICON(2, "shield_green");
		UPDATE_TEXT(2, tr("Update information received successfully."));
		UPDATE_ICON(3, "play");
		break;
	default:
		MUTILS_THROW("Unknown status code!");
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
		case MUtils::UpdateChecker::UpdateStatus_CompletedUpdateAvailable:
			UPDATE_ICON(3, "shield_exclamation");
			UPDATE_TEXT(3, tr("A newer version is available!"));
			ui->buttonDownload->show();
			break;
		case MUtils::UpdateChecker::UpdateStatus_CompletedNoUpdates:
			UPDATE_ICON(3, "shield_green");
			UPDATE_TEXT(3, tr("Your version is up-to-date."));
			break;
		case MUtils::UpdateChecker::UpdateStatus_CompletedNewVersionOlder:
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
	case MUtils::UpdateChecker::UpdateStatus_CompletedUpdateAvailable:
	case MUtils::UpdateChecker::UpdateStatus_CompletedNoUpdates:
	case MUtils::UpdateChecker::UpdateStatus_CompletedNewVersionOlder:
		SHOW_ANIMATION(false);
		ui->labelBuildNo->setText(tr("Installed build is #%1  |  Latest build is #%2").arg(QString::number(x264_version_build()), QString::number(m_thread->getUpdateInfo()->getBuildNo())));
		ui->labelUrl->setText(QString("<a href=\"%1\">%1</a>").arg(m_thread->getUpdateInfo()->getDownloadSite()));
		break;
	case MUtils::UpdateChecker::UpdateStatus_ErrorNoConnection:
	case MUtils::UpdateChecker::UpdateStatus_ErrorConnectionTestFailed:
	case MUtils::UpdateChecker::UpdateStatus_ErrorFetchUpdateInfo:
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

	const MUtils::UpdateCheckerInfo *updateInfo = m_thread->getUpdateInfo();

	QProcess process;
	QStringList args;
	QEventLoop loop;

	MUtils::init_process(process, MUtils::temp_folder(), false);

	connect(&process, SIGNAL(error(QProcess::ProcessError)), &loop, SLOT(quit()));
	connect(&process, SIGNAL(finished(int,QProcess::ExitStatus)), &loop, SLOT(quit()));

	args << QString("/Location=%1").arg(updateInfo->getDownloadAddress());
	args << QString("/Filename=%1").arg(updateInfo->getDownloadFilename());
	args << QString("/TicketID=%1").arg(updateInfo->getDownloadFilecode());
	args << QString("/CheckSum=%1").arg(updateInfo->getDownloadChecksum());
	args << QString("/ToFolder=%1").arg(QDir::toNativeSeparators(QDir(QApplication::applicationDirPath()).canonicalPath()));
	args << QString("/ToExFile=%1.exe").arg(QFileInfo(QFileInfo(QApplication::applicationFilePath()).canonicalFilePath()).completeBaseName());
	args << QString("/AppTitle=Simple x264 Launcher (Build #%1)").arg(QString::number(updateInfo->getBuildNo()));

	process.start(m_binaries.value("wupd.exe"), args);
	if(!process.waitForStarted())
	{
		QApplication::restoreOverrideCursor();
		SHOW_ANIMATION(false);
		QMessageBox::critical(this, tr("Update Failed"), tr("Sorry, failed to launch web-update program!"));
		ui->buttonDownload->show();
		ui->buttonCancel->setEnabled(true);
		return;
	}

	m_updaterProcess = MUtils::OS::process_id(&process);
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

bool UpdaterDialog::checkBinaries(void)
{
	qDebug("[File Verification]");
	m_binaries.clear();

	//Validate hashes first
	const QString tempPath = MUtils::temp_folder();
	for(size_t i = 0; BINARIES[i].name; i++)
	{
		const QString orgName = QString::fromLatin1(BINARIES[i].name);
		const QString binPath = QString("%1/toolset/common/%2").arg(m_sysinfo->getAppPath(), orgName);
		const QString outPath = QString("%1/%2_%3.%4").arg(tempPath, QFileInfo(orgName).baseName(), MUtils::rand_str(), QFileInfo(orgName).suffix());
		if(!checkFileHash(binPath, BINARIES[i].hash))
		{
			qWarning("Verification of '%s' has failed!", MUTILS_UTF8(orgName));
			return false;
		}
		if(!QFile::copy(binPath, outPath))
		{
			qWarning("Copying of '%s' has failed!", MUTILS_UTF8(orgName));
			return false;
		}
		QFile::setPermissions(outPath, QFile::ReadOwner);
		m_binaries.insert(BINARIES[i].name, outPath);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	}

	return true;
}

bool UpdaterDialog::checkFileHash(const QString &filePath, const char *expectedHash)
{
	qDebug("Checking file: %s", filePath.toUtf8().constData());
	MUtils::Hash::Blake2 checksum2;
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

void UpdaterDialog::cleanFiles(void)
{
	const QStringList keys = m_binaries.keys();
	foreach(const QString &key, keys)
	{
		const QString fileName = m_binaries.value(key);
		QFile::setPermissions(fileName, QFile::ReadOwner | QFile::WriteOwner);
		if(!QFile::remove(fileName))
		{
			qWarning("Failed to remove file: %s", MUTILS_UTF8(fileName));
		}
		m_binaries.remove(key);
	}
}
