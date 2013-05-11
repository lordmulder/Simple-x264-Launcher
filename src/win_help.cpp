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

#include "win_help.h"
#include "global.h"

#include <QProcess>
#include <QScrollBar>
#include <QTimer>

#define X264_BINARY(BIN_DIR, IS_10BIT, IS_X64) QString("%1/x264_%2_%3.exe").arg((BIN_DIR), ((IS_10BIT) ? "10bit" : "8bit"), ((IS_X64) ? "x64" : "x86"))

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

HelpDialog::HelpDialog(QWidget *parent, bool avs2yuv, bool x64supported, bool use10BitEncoding)
:
	QDialog(parent),
	m_appDir(QApplication::applicationDirPath() + "/toolset"),
	m_avs2yuv(avs2yuv),
	m_x64supported(x64supported),
	m_use10BitEncoding(use10BitEncoding),
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
	logo_x264->setHidden(m_avs2yuv);
	logo_avisynth->setVisible(m_avs2yuv);
	
	QDialog::showEvent(event);
	
	m_startAgain = true;

	if(!m_avs2yuv)
	{
		m_process->start(X264_BINARY(m_appDir, m_use10BitEncoding, m_x64supported), QStringList() << "--version");
	}
	else
	{
		m_process->start(QString("%1/%2.exe").arg(m_appDir, m_x64supported ? "avs2yuv_x64" : "avs2yuv_x86"), QStringList());
	}

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
		if(!m_avs2yuv)
		{
			m_process->start(X264_BINARY(m_appDir, m_use10BitEncoding, m_x64supported), QStringList() << "--fullhelp");
			plainTextEdit->appendPlainText("\n--------\n");

			if(!m_process->waitForStarted())
			{
				plainTextEdit->appendPlainText(tr("Failed to create x264 process :-("));
			}
		}
	}
	else
	{
		plainTextEdit->verticalScrollBar()->setSliderPosition(0);
	}
}
