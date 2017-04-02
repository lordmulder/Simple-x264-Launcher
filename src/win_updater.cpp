///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2017 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <MUtils/Hash.h>
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
#include <QElapsedTimer>

///////////////////////////////////////////////////////////////////////////////

static const char *const DIGEST_KEY = "~Dv/bW3/7t>6?RXVwkaZk-hmS0#O4JS/5YQAO>\\8hvr0B~7[n!X~KMYruemu:MDq";

const UpdaterDialog::binary_t UpdaterDialog::BINARIES[] =
{
	{ "wget.exe", "35d70bf8a1799956b5de3975ff99088a4444a2d17202059afb63949b297e2cc81e5e49e2b95df1c4e26b49ab7430399c293bf805a0b250d686c6f4dd994a0764", 1 },
	{ "netc.exe", "94df9fd3212f72cdfb3c17d612db3ea2748c80b5392902500bc7f7fa4bf4f3dccb2cc049c3edc067f6ed33bf1986800e8a501d34f58a254f2eb38fe6c33e2ade", 1 },
	{ "gpgv.exe", "a8d4d1702e5fb1eee5a2c22fdaf255816a9199ae48142aeec1c8ce16bbcf61d6d634f1e769e62d05cf52c204ba2611f09c9bb661bc6688b937749d478af3e47d", 1 },
	{ "gpgv.gpg", "1a2f528e551b9abfb064f08674fdd421d3abe403469ddfee2beafd007775a6c684212a6274dc2b41a0b20dd5c2200021c91320e737f7a90b2ac5a40a6221d93f", 0 },
	{ "wupd.exe", "c7fe72259ae781889a18f688321275e3bae39d75fb96c9c650446e177cb3af3d3ea84db2c1590e44bc2440b2ea79f9684e3a14e47e57e6083ec6f98c5bf72a73", 1 },
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

static inline QString getBin(const QMap<QString, QSharedPointer<QFile>> &binaries, const QString &nameName)
{
	const QSharedPointer<QFile> file = binaries.value(nameName);
	return file.isNull() ? QString() : file->fileName();
}

static void qFileDeleter(QFile *const file)
{
	if(file)
	{
		file->close();
		delete file;
	}
}

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
	ui->labelCancel->hide();

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
	delete ui;
}

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
	switch (event->key())
	{
	case Qt::Key_Escape:
		if ((!m_thread.isNull()) && m_thread->isRunning())
		{
			if (m_status >= MUtils::UpdateChecker::UpdateStatus_FetchingUpdates)
			{
				UPDATE_TEXT(2, tr("Cancellation requested..."));
			}
			else
			{
				UPDATE_TEXT(1, tr("Cancellation requested..."));
			}
			m_thread->cancel();
		}
		break;
	case Qt::Key_F11:
		{
			const QString logFilePath = MUtils::make_temp_file(MUtils::temp_folder(), "txt", true);
			if (!logFilePath.isEmpty())
			{
				qWarning("Write log to: '%s'", MUTILS_UTF8(logFilePath));
				QFile logFile(logFilePath);
				if (logFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
				{
					logFile.write("\xEF\xBB\xBF");
					for (QStringList::ConstIterator iter = m_logFile.constBegin(); iter != m_logFile.constEnd(); iter++)
					{
						logFile.write(iter->toUtf8());
						logFile.write("\r\n");
					}
					logFile.close();
					QDesktopServices::openUrl(QUrl::fromLocalFile(logFile.fileName()));
				}
			}
		}
		break;
	default:
		QDialog::keyPressEvent(event);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void UpdaterDialog::initUpdate(void)
{
	//Check binary files
	if(!checkBinaries())
	{
		ui->buttonCancel->setEnabled(true);
		const QString message = QString("%1<br><br><nobr><a href=\"%2\">%3</a></nobr><br>").arg(tr("At least one file required by the web-update tool is missing or corrupted.<br>Please re-install this application and then try again!"), QString::fromLatin1(m_updateUrl), QString::fromLatin1(m_updateUrl).replace("-", "&minus;"));
		if(QMessageBox::critical(this, tr("File Error"), message, tr("Download Latest Version"), tr("Discard")) == 0)
		{
			QDesktopServices::openUrl(QUrl(QString::fromLatin1(m_updateUrl)));
		}
		close();
		return;
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
			close();
			return;
		}
	}
	
	//Create and setup thread
	if(!m_thread)
	{
		m_thread.reset(new MUtils::UpdateChecker(getBin(m_binaries, "wget.exe"), getBin(m_binaries, "netc.exe"), getBin(m_binaries, "gpgv.exe"), getBin(m_binaries, "gpgv.gpg"), "Simple x264 Launcher", x264_version_build(), false));
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
	ui->labelCancel->show();

	//Update status
	threadStatusChanged(MUtils::UpdateChecker::UpdateStatus_NotStartedYet);

	//Start animation
	SHOW_ANIMATION(true);

	//Update cursor
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	QApplication::setOverrideCursor(Qt::WaitCursor);

	//Clear log
	m_logFile.clear();

	//Init timer
	m_elapsed.reset(new QElapsedTimer());
	m_elapsed->start();

	//Start the updater thread
	QTimer::singleShot(125, m_thread.data(), SLOT(start()));
}

void UpdaterDialog::threadStatusChanged(int status)
{
	const int prevStatus = m_status;
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
	case MUtils::UpdateChecker::UpdateStatus_CancelledByUser:
		if (prevStatus >= MUtils::UpdateChecker::UpdateStatus_FetchingUpdates)
		{
			UPDATE_ICON(2, "shield_error");
			UPDATE_TEXT(2, tr("Operation was cancelled by the user!"));
			UPDATE_ICON(3, "shield_grey");
		}
		else
		{
			UPDATE_ICON(1, "shield_error");
			UPDATE_TEXT(1, tr("Operation was cancelled by the user!"));
			UPDATE_ICON(2, "shield_grey");
			UPDATE_ICON(3, "shield_grey");
		}
		break;
	default:
		MUTILS_THROW("Unknown status code!");
	}
}

void UpdaterDialog::threadFinished(void)
{
	m_success = m_thread->getSuccess();
	ui->labelCancel->hide();
	QTimer::singleShot((m_success ? 500 : 0), this, SLOT(updateFinished()));
}

void UpdaterDialog::updateFinished(void)
{
	//Query the timer, if available
	if (!m_elapsed.isNull())
	{
		const quint64 elapsed = m_elapsed->restart();
		qDebug("Update check completed after %.2f seconds.", double(elapsed) / 1000.0);
	}

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
	case MUtils::UpdateChecker::UpdateStatus_CancelledByUser:
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

	process.start(getBin(m_binaries, "wupd.exe"), args);
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
	for(size_t i = 0; BINARIES[i].name; i++)
	{
		const QString name = QString::fromLatin1(BINARIES[i].name);
		if (!m_binaries.contains(name))
		{
			QScopedPointer<QFile> binary(new QFile(QString("%1/toolset/common/%2").arg(m_sysinfo->getAppPath(), name)));
			if (binary->open(QIODevice::ReadOnly))
			{
				if (checkFileHash(binary->fileName(), BINARIES[i].hash))
				{
					QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
					m_binaries.insert(name, QSharedPointer<QFile>(binary.take(), qFileDeleter));
				}
				else
				{
					qWarning("Verification of '%s' has failed!", MUTILS_UTF8(name));
					binary->close();
					return false;
				}
			}
			else
			{
				qWarning("File '%s' could not be opened!", MUTILS_UTF8(name));
				return false;
			}
		}
	}
	qDebug("File check completed.\n");
	return true;
}

bool UpdaterDialog::checkFileHash(const QString &filePath, const char *expectedHash)
{
	qDebug("Checking file: %s", MUTILS_UTF8(filePath));
	QScopedPointer<MUtils::Hash::Hash> checksum(MUtils::Hash::create(MUtils::Hash::HASH_BLAKE2_512, DIGEST_KEY));
	QFile file(filePath);
	if(file.open(QIODevice::ReadOnly))
	{
		checksum->update(file);
		const QByteArray fileHash = checksum->digest();
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
