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

#include "win_help.h"
#include "global.h"

#include <QProcess>
#include <QScrollBar>
#include <QTimer>

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

HelpDialog::HelpDialog(QWidget *parent)
:
	QDialog(parent),
	m_appDir(QApplication::applicationDirPath()),
	m_process(new QProcess())
{
	//Init the dialog, from the .ui file
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

	//Fix size
	setMinimumSize(size());

	//Prepare process
	m_process->setReadChannelMode(QProcess::MergedChannels);
	m_process->setReadChannel(QProcess::StandardOutput);
	connect(m_process, SIGNAL(readyRead()), this, SLOT(readyRead()));
	connect(m_process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(finished()));

	m_startAgain = true;
}

HelpDialog::~HelpDialog(void)
{
	delete m_process;
}

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

void HelpDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);

	m_startAgain = true;
	m_process->start(QString("%1/toolset/x264.exe").arg(m_appDir), QStringList() << "--version");

	if(!m_process->waitForStarted())
	{
		plainTextEdit->appendPlainText(tr("Failed to create x264 process :-("));
	}
}

void HelpDialog::closeEvent(QCloseEvent *e)
{
	if(m_process->state() != QProcess::NotRunning)
	{
		e->ignore();
		MessageBeep(MB_ICONWARNING);
		return;
	}

	QDialog::closeEvent(e);
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void HelpDialog::readyRead(void)
{
	while(m_process->canReadLine())
	{
		QString line = QString::fromLatin1(m_process->readLine());
		while(line.endsWith('\r') || line.endsWith('\n'))
		{
			line = line.left(line.length() - 1);
		}
		plainTextEdit->appendPlainText(line);
	}
}

void HelpDialog::finished(void)
{
	if(m_startAgain)
	{
		m_startAgain = false;
		m_process->start(QString("%1/toolset/x264.exe").arg(m_appDir), QStringList() << "--fullhelp");
		plainTextEdit->appendPlainText("\n--------\n");

		if(!m_process->waitForStarted())
		{
			plainTextEdit->appendPlainText(tr("Failed to create x264 process :-("));
		}
	}
	else
	{
		plainTextEdit->verticalScrollBar()->setSliderPosition(0);
	}
}
