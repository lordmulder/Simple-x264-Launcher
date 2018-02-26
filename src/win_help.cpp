///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2018 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include "UIC_win_help.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_sysinfo.h"
#include "model_preferences.h"
#include "encoder_factory.h"
#include "source_factory.h"

//MUtils
#include <MUtils/Sound.h>

//Qt
#include <QProcess>
#include <QScrollBar>
#include <QTimer>

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

HelpDialog::HelpDialog(QWidget *parent, bool avs2yuv, const SysinfoModel *const sysinfo, const OptionsModel *const options, const PreferencesModel *const preferences)
:
	QDialog(parent),
	m_avs2yuv(avs2yuv),
	m_sysinfo(sysinfo),
	m_preferences(preferences),
	m_options(options),
	m_process(new QProcess()),
	ui(new Ui::HelpDialog())
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
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
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

void HelpDialog::showEvent(QShowEvent *event)
{
	ui->logo_x264->setHidden(m_avs2yuv);
	ui->logo_avisynth->setVisible(m_avs2yuv);
	
	QDialog::showEvent(event);
	
	m_startAgain = true;

	if(!m_avs2yuv)
	{
		m_process->start(EncoderFactory::getEncoderInfo(m_options->encType()).getBinaryPath(m_sysinfo, m_options->encArch(), m_options->encVariant()), QStringList() << "--version");
	}
	else
	{
		m_process->start(SourceFactory::getSourceInfo(SourceFactory::SourceType_AVS).getBinaryPath(m_sysinfo, m_preferences->getPrefer64BitSource() && m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64)), QStringList());
	}

	if(!m_process->waitForStarted())
	{
		ui->plainTextEdit->appendPlainText(tr("Failed to create x264 process :-("));
	}
}

void HelpDialog::closeEvent(QCloseEvent *e)
{
	if(m_process->state() != QProcess::NotRunning)
	{
		e->ignore();
		MUtils::Sound::beep(MUtils::Sound::BEEP_WRN);
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
		ui->plainTextEdit->appendPlainText(line);
	}
}

void HelpDialog::finished(void)
{
	if(m_startAgain)
	{
		m_startAgain = false;
		if(!m_avs2yuv)
		{
			const AbstractEncoderInfo &encInfo = EncoderFactory::getEncoderInfo(m_options->encType());
			m_process->start(encInfo.getBinaryPath(m_sysinfo, m_options->encArch(), m_options->encVariant()), QStringList() << encInfo.getHelpCommand());
			ui->plainTextEdit->appendPlainText("\n--------\n");

			if(!m_process->waitForStarted())
			{
				ui->plainTextEdit->appendPlainText(tr("Failed to create x264 process :-("));
			}
		}
	}
	else
	{
		ui->plainTextEdit->verticalScrollBar()->setSliderPosition(0);
	}
}
