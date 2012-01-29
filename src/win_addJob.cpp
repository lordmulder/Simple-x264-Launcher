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
#include "model_options.h"

#include <QDate>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QValidator>

///////////////////////////////////////////////////////////////////////////////
// Validator
///////////////////////////////////////////////////////////////////////////////

class StringValidator : public QValidator
{
	virtual State validate(QString &input, int &pos) const
	{
		bool invalid = input.simplified().compare(input) && input.simplified().append(" ").compare(input) &&
			input.simplified().prepend(" ").compare(input) && input.simplified().append(" ").prepend(" ").compare(input);

		if(!invalid)
		{
			invalid = invalid || input.contains("--fps");
			invalid = invalid || input.contains("--frames");
			invalid = invalid || input.contains("--preset");
			invalid = invalid || input.contains("--tune");
			invalid = invalid || input.contains("--profile");
		}

		if(invalid) MessageBeep(MB_ICONWARNING);
		return invalid ? QValidator::Invalid : QValidator::Acceptable;
	}

	virtual void fixup(QString &input) const
	{
		input = input.simplified();
	}
};

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

AddJobDialog::AddJobDialog(QWidget *parent, OptionsModel *options)
:
	QDialog(parent),
	m_options(options)
{
	//Init the dialog, from the .ui file
	setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));
	
	//Fix dialog size
	resize(width(), minimumHeight());
	setMinimumSize(size());
	setMaximumHeight(height());

	//Activate combobox
	connect(cbxRateControlMode, SIGNAL(currentIndexChanged(int)), this, SLOT(modeIndexChanged(int)));

	//Activate buttons
	connect(buttonBrowseSource, SIGNAL(clicked()), this, SLOT(browseButtonClicked()));
	connect(buttonBrowseOutput, SIGNAL(clicked()), this, SLOT(browseButtonClicked()));

	//Setup validator
	cbxCustomParams->setValidator(new StringValidator());
	cbxCustomParams->addItem("--bluray-compat --vbv-maxrate 40000 --vbv-bufsize 30000 --level 4.1 --keyint 25 --open-gop --slices 4");
	cbxCustomParams->clearEditText();
}

AddJobDialog::~AddJobDialog(void)
{
}

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

void AddJobDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	restoreOptions(m_options);
	modeIndexChanged(cbxRateControlMode->currentIndex());
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void AddJobDialog::modeIndexChanged(int index)
{
	spinQuantizer->setEnabled(index == 0 || index == 1);
	spinBitrate->setEnabled(index == 2 || index == 3);
}

void AddJobDialog::accept(void)
{
	if(editSource->text().trimmed().isEmpty())
	{
		QMessageBox::warning(this, tr("Not Found!"), tr("Please select a valid source file first!"));
		return;
	}
	
	QFileInfo sourceFile = QFileInfo(editSource->text());
	if(!(sourceFile.exists() && sourceFile.isFile()))
	{
		QMessageBox::warning(this, tr("Not Found!"), tr("The selected source file could not be found!"));
		return;
	}

	QFileInfo outputDir = QFileInfo(QFileInfo(editOutput->text()).path());
	if(!(outputDir.exists() && outputDir.isDir() && outputDir.isWritable()))
	{
		QMessageBox::warning(this, tr("Not Writable!"), tr("Output directory does not exist or is not writable!"));
		return;
	}

	QFileInfo outputFile = QFileInfo(editOutput->text());
	if(outputFile.exists() && outputFile.isFile())
	{
		if(QMessageBox::question(this, tr("Already Exists!"), tr("Output file already exists! Overwrite?"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		{
			return;
		}
	}
	if(outputFile.exists() && (!outputFile.isFile()))
	{
		QMessageBox::warning(this, tr("Not a File!"), tr("Selected output files does not appear to be a file!"));
		return;
	}

	saveOptions(m_options);
	QDialog::accept();
}

void AddJobDialog::browseButtonClicked(void)
{
	QString initialDir = QDesktopServices::storageLocation(QDesktopServices::MoviesLocation);

	if(QObject::sender() == buttonBrowseSource)
	{
		QString filters;
		filters += tr("Avisynth Scripts (*.avs)").append(";;");
		filters += tr("Matroska Files (*.mkv)").append(";;");
		filters += tr("MPEG-4 Part 14 Container (*.mp4)").append(";;");
		filters += tr("Audio Video Interleaved (*.avi)").append(";;");
		filters += tr("Flash Video (*.flv)").append(";;");

		QString filePath = QFileDialog::getOpenFileName(this, tr("Open Source File"), initialDir, filters);

		if(!(filePath.isNull() || filePath.isEmpty()))
		{
			editSource->setText(filePath);

			QString path = QFileInfo(filePath).path();
			QString name = QFileInfo(filePath).completeBaseName();
			
			QString outPath = QString("%1/%2.mkv").arg(path, name);

			if(QFileInfo(outPath).exists())
			{
				int i = 2;
				while(QFileInfo(outPath).exists())
				{
					outPath = QString("%1/%2 (%3).mkv").arg(path, name, QString::number(i++));
				}
			}

			editOutput->setText(outPath);
		}
	}
	else if(QObject::sender() == buttonBrowseOutput)
	{
		QString filters;
		filters += tr("Matroska Files (*.mkv)").append(";;");
		filters += tr("MPEG-4 Part 14 Container (*.mp4)").append(";;");
		filters += tr("H.264 Elementary Stream (*.264)").append(";;");

		QString filePath = QFileDialog::getSaveFileName(this, tr("Choose Output File"), initialDir, filters);

		if(!(filePath.isNull() || filePath.isEmpty()))
		{
			editOutput->setText(filePath);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

void AddJobDialog::updateComboBox(QComboBox *cbox, const QString &text)
{
	for(int i = 0; i < cbox->model()->rowCount(); i++)
	{
		if(cbox->model()->data(cbox->model()->index(i, 0, QModelIndex())).toString().compare(text, Qt::CaseInsensitive) == 0)
		{
			cbox->setCurrentIndex(i);
			break;
		}
	}
}

void AddJobDialog::restoreOptions(OptionsModel *options)
{
	cbxRateControlMode->setCurrentIndex(options->rcMode());
	spinQuantizer->setValue(options->quantizer());
	spinBitrate->setValue(options->bitrate());
	updateComboBox(cbxPreset, options->preset());
	updateComboBox(cbxTuning, options->tune());
	updateComboBox(cbxProfile, options->profile());
	cbxCustomParams->setEditText(options->custom());
}

void AddJobDialog::saveOptions(OptionsModel *options)
{
	options->setRCMode(static_cast<OptionsModel::RCMode>(cbxRateControlMode->currentIndex()));
	options->setQuantizer(spinQuantizer->value());
	options->setBitrate(spinBitrate->value());
	options->setPreset(cbxPreset->model()->data(cbxPreset->model()->index(cbxPreset->currentIndex(), 0)).toString());
	options->setTune(cbxTuning->model()->data(cbxTuning->model()->index(cbxTuning->currentIndex(), 0)).toString());
	options->setProfile(cbxProfile->model()->data(cbxProfile->model()->index(cbxProfile->currentIndex(), 0)).toString());
	options->setCustom(cbxCustomParams->currentText());
}
