///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "win_about.h"
#include "UIC_win_about.h"

//Internal
#include "global.h"

//MUtils
#include <MUtils/Version.h>

//Qt
#include <QProcess>
#include <QScrollBar>
#include <QDate>
#include <QTimer>
#include <QMessageBox>

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

AboutDialog::AboutDialog(QWidget *parent)
:
	QDialog(parent),
	ui(new Ui::AboutDialog())
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
	setWindowFlags(windowFlags() | Qt::Tool);

	//Fill in the template text
	ui->labelAbout->setText(
		ui->labelAbout->text().arg(
			QString().sprintf("%u.%02u.%u", x264_version_major(),
			x264_version_minor(),
			x264_version_build()),
			QString::number(qMax(MUtils::Version::app_build_date().year(),QDate::currentDate().year())),
			MUtils::Version::app_build_date().toString(Qt::ISODate),
			MUtils::Version::app_build_time().toString(Qt::ISODate),
			MUtils::Version::compiler_version(),
			MUtils::Version::compiler_arch(),
			QString::fromLatin1(QT_VERSION_STR)
		)
	);

	//Enable hover
	ui->labelGPL->setAttribute(Qt::WA_Hover, true);
	((QWidget*)ui->labelGPL->parent())->setAttribute(Qt::WA_Hover, true);
	((QWidget*)ui->labelGPL->parent()->parent())->setAttribute(Qt::WA_Hover, true);

	//Switch to first tab
	ui->tabWidget->setCurrentIndex(ui->tabWidget->indexOf(ui->tabAbout));

	//Connect button
	connect(ui->aboutQtButton, SIGNAL(clicked()), this, SLOT(showAboutQt()));
}

AboutDialog::~AboutDialog(void)
{
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
// Public Functions
///////////////////////////////////////////////////////////////////////////////

/*None*/

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

void AboutDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);

	//Fix dialog size - need to do this in Show event
	const QSize hint = sizeHint();
	setFixedSize(hint.isValid() ? hint : size());
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void AboutDialog::showAboutQt(void)
{
	QMessageBox::aboutQt(this);
}
