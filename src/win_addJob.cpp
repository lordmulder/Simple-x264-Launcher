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

#include "win_addJob.h"

#include "global.h"

#include <QDate>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

AddJobDialog::AddJobDialog(QWidget *parent)
:
	QDialog(parent)
{
	//Init the dialog, from the .ui file
	setupUi(this);
	
	//Fix dialog size
	resize(width(), minimumHeight());
	setMinimumSize(size());
	setMaximumHeight(height());

	//Activate combobox
	connect(cbxRateControlMode, SIGNAL(currentIndexChanged(int)), this, SLOT(modeIndexChanged(int)));
}

AddJobDialog::~AddJobDialog(void)
{
}

void AddJobDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	modeIndexChanged(cbxRateControlMode->currentIndex());
}

void AddJobDialog::modeIndexChanged(int index)
{
	spinQuantizer->setEnabled(index == 0 || index == 1);
	spinBitrate->setEnabled(index == 2 || index == 3);
}
