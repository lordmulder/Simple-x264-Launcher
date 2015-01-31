///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2015 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "win_editor.h"
#include "uic_win_editor.h"

#include "global.h"

#include <QProcess>
#include <QScrollBar>
#include <QTimer>

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

EditorDialog::EditorDialog(QWidget *parent)
:
	QDialog(parent),
	ui(new Ui::EditorDialog())
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

	//Fix size
	setMinimumSize(size());
}

EditorDialog::~EditorDialog(void)
{
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
// Public Functions
///////////////////////////////////////////////////////////////////////////////

QString EditorDialog::getEditText(void)
{
	return ui->plainTextEdit->toPlainText().simplified();
}
	
void EditorDialog::setEditText(const QString &text)
{
	ui->plainTextEdit->clear();
	ui->plainTextEdit->appendPlainText(text.simplified());
}

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

/*None*/

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

/*None*/

