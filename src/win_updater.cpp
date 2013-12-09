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
#include "checksum.h"

#include <QMovie>
#include <QCloseEvent>
#include <QTimer>
#include <QMessageBox>

///////////////////////////////////////////////////////////////////////////////

#define UPDATE_TEXT(N, TEXT) ui->label_phase##N->setText((TEXT))
#define UPDATE_ICON(N, ICON) ui->icon_phase##N->setPixmap(QIcon(":/buttons/" ICON ".png").pixmap(16, 16))

#define SHOW_ANIMATION(FLAG) do  \
{ \
	ui->labelLoadingLeft->setVisible((FLAG)); \
	ui->labelLoadingCenter->setVisible((FLAG)); \
	ui->labelLoadingRight->setVisible((FLAG)); \
	ui->labelInfo->setVisible(!(FLAG)); \
	ui->labelUrl->setVisible(!(FLAG)); \
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
	m_state(0),
	m_firstShow(true)
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

	//Fix size
	setFixedSize(size());

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
}

UpdaterDialog::~UpdaterDialog(void)
{
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

void UpdaterDialog::showEvent(QShowEvent *event)
{
	if(m_firstShow)
	{
		m_firstShow = false;
		QTimer::singleShot(0, this, SLOT(initUpdate()));
	}
}

void UpdaterDialog::closeEvent(QCloseEvent *e)
{
	if(!ui->buttonCancel->isEnabled())
	{
		e->ignore();
	}
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void UpdaterDialog::initUpdate(void)
{
	//Restet text
	ui->retranslateUi(this);

	//Init buttons
	ui->buttonCancel->setEnabled(false);
	ui->buttonRetry->hide();
	ui->buttonDownload->hide();

	//Hide labels
	ui->labelInfo->hide();
	ui->labelUrl->hide();

	//Reset icons
	UPDATE_ICON(1, "clock");
	UPDATE_ICON(2, "clock");
	UPDATE_ICON(3, "clock");

	//Show animation
	SHOW_ANIMATION(true);

	//Check binary files
	if(!checkBinaries())
	{
		ui->buttonCancel->setEnabled(true);
		QMessageBox::critical(this, tr("File Error"), tr("At least one file required by web-update is missing or corrupted.<br>Please re-install this application and then try again!"));
		close();
		return;
	}

	//Begin updater test run
	m_state = 0;
	QTimer::singleShot(333, this, SLOT(updateState()));
}

void UpdaterDialog::updateState(void)
{
	switch(m_state++)
	{
	case 0:
		UPDATE_ICON(1, "play");
		QTimer::singleShot(6666, this, SLOT(updateState()));
		break;
	case 1:
		UPDATE_ICON(1, "shield_green");
		UPDATE_TEXT(1, tr("Internet connection is working."));
		UPDATE_ICON(2, "play");
		QTimer::singleShot(6666, this, SLOT(updateState()));
		break;
	case 2:
		UPDATE_ICON(2, "shield_green");
		UPDATE_TEXT(2, tr("Update-information was received successfully."));
		UPDATE_ICON(3, "play");
		QTimer::singleShot(6666, this, SLOT(updateState()));
		break;
	case 3:
		UPDATE_ICON(3, "shield_exclamation");
		UPDATE_TEXT(3, tr("A newer version is available!"));
		QTimer::singleShot(6666, this, SLOT(updateState()));
		SHOW_ANIMATION(false);
		ui->buttonCancel->setEnabled(true);
		ui->buttonDownload->show();
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Private Functions
///////////////////////////////////////////////////////////////////////////////

bool UpdaterDialog::checkBinaries(void)
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
		{ NULL, NULL }
	};

	bool okay = true;
	for(size_t i = 0; FILE_INFO[i].name; i++)
	{
		okay = okay && checkFileHash(QString("%1/common/%2").arg(m_binDir, QString::fromLatin1(FILE_INFO[i].name)), FILE_INFO[i].hash);
	}

	if(okay)
	{
		qDebug("Completed.\n");
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
