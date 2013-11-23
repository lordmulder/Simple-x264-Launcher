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

UpdaterDialog::UpdaterDialog(QWidget *parent)
:
	QDialog(parent),
	ui(new Ui::UpdaterDialog()),
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

	//TEST
	QBlake2Checksum checksum;
	checksum.update("The quick brown fox jumps over the lazy dog");
	qWarning("Result: %s\n", checksum.finalize().constData());

	//TEST
	QBlake2Checksum checksum2;
	QFile file("");
	if(file.open(QIODevice::ReadOnly))
	{
		checksum2.update(file);
		qWarning("Result: %s\n", checksum2.finalize().constData());
	}
}

UpdaterDialog::~UpdaterDialog(void)
{
	X264_DELETE(m_animator);
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
// Public Functions
///////////////////////////////////////////////////////////////////////////////

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
