///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2021 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include "UIC_win_addJob.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_preferences.h"
#include "model_sysinfo.h"
#include "model_recently.h"
#include "encoder_factory.h"
#include "mediainfo.h"
#include "string_validator.h"
#include "win_help.h"
#include "win_editor.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/OSSupport.h>
#include <MUtils/Exception.h>

//Qt
#include <QDate>
#include <QTimer>
#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QValidator>
#include <QDir>
#include <QInputDialog>
#include <QSettings>
#include <QUrl>
#include <QAction>
#include <QClipboard>
#include <QToolTip>

#include <memory>

#define ARRAY_SIZE(ARRAY) (sizeof((ARRAY))/sizeof((ARRAY[0])))
#define VALID_DIR(PATH) ((!(PATH).isEmpty()) && QFileInfo(PATH).exists() && QFileInfo(PATH).isDir())
#define SHFL(X) ((*reinterpret_cast<int*>(&(X))) <<= 1)

#define REMOVE_USAFED_ITEM \
{ \
	for(int i = 0; i < ui->cbxTemplate->count(); i++) \
	{ \
		const OptionsModel* temp = reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(i).value<const void*>()); \
		if(temp == NULL) \
		{ \
			ui->cbxTemplate->blockSignals(true); \
			ui->cbxTemplate->removeItem(i); \
			ui->cbxTemplate->blockSignals(false); \
			break; \
		} \
	} \
}

#define ADD_CONTEXTMENU_ACTION(WIDGET, ICON, TEXT, SLOTNAME) \
{ \
	QAction *_action = new QAction((ICON), (TEXT), this); \
	_action->setData(QVariant::fromValue<void*>(WIDGET)); \
	WIDGET->addAction(_action); \
	connect(_action, SIGNAL(triggered(bool)), this, SLOT(SLOTNAME())); \
}

#define ADD_CONTEXTMENU_SEPARATOR(WIDGET) \
{ \
	QAction *_action = new QAction(this); \
	_action->setSeparator(true); \
	WIDGET->addAction(_action); \
} 

Q_DECLARE_METATYPE(const void*)

///////////////////////////////////////////////////////////////////////////////
// Disable Monitoring RAII
///////////////////////////////////////////////////////////////////////////////

class DisableHelperRAII
{
public:
	DisableHelperRAII(bool *const flag) : m_flag(flag)
	{
		*m_flag = false;
	}

	~DisableHelperRAII(void)
	{
		*m_flag = true;
	}

private:
	bool *const m_flag;
};

///////////////////////////////////////////////////////////////////////////////
// Constructor & Destructor
///////////////////////////////////////////////////////////////////////////////

AddJobDialog::AddJobDialog(QWidget *parent, OptionsModel *const options, RecentlyUsed *const recentlyUsed, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences)
:
	QDialog(parent),
	m_options(options),
	m_recentlyUsed(recentlyUsed),
	m_sysinfo(sysinfo),
	m_preferences(preferences),
	m_defaults(new OptionsModel(sysinfo)),
	ui(new Ui::AddJobDialog()),
	m_monitorConfigChanges(false)
{
	//Init the dialog, from the .ui file
	ui->setupUi(this);
	setWindowFlags(windowFlags() & (~Qt::WindowContextHelpButtonHint));

	//Fix dialog size
	ui->buttonSaveTemplate->setMaximumHeight(20);
	ui->buttonDeleteTemplate->setMaximumHeight(20);
	resize(width(), minimumHeight());
	setMinimumSize(size());
	setMaximumHeight(height());

	//Init encoder combobox
	ui->cbxEncoderType->addItem(tr("x264 (AVC)"),  OptionsModel::EncType_X264);
	ui->cbxEncoderType->addItem(tr("x265 (HEVC)"), OptionsModel::EncType_X265);
	ui->cbxEncoderType->addItem(tr("NVEncC"),      OptionsModel::EncType_NVEnc);

	//Init combobox items
	ui->cbxTuning ->addItem(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED));
	ui->cbxProfile->addItem(QString::fromLatin1(OptionsModel::PROFILE_UNRESTRICTED));

	//Hide optional controls
	ui->checkBoxApplyToAll->setVisible(false);

	//Monitor combobox changes
	connect(ui->cbxEncoderType,     SIGNAL(currentIndexChanged(int)), this, SLOT(encoderIndexChanged(int)));
	connect(ui->cbxEncoderVariant,  SIGNAL(currentIndexChanged(int)), this, SLOT(variantIndexChanged(int)));
	connect(ui->cbxRateControlMode, SIGNAL(currentIndexChanged(int)), this, SLOT(modeIndexChanged(int)));

	//Activate buttons
	connect(ui->buttonBrowseSource,   SIGNAL(clicked()), this, SLOT(browseButtonClicked()));
	connect(ui->buttonBrowseOutput,   SIGNAL(clicked()), this, SLOT(browseButtonClicked()));
	connect(ui->buttonSaveTemplate,   SIGNAL(clicked()), this, SLOT(saveTemplateButtonClicked()));
	connect(ui->buttonDeleteTemplate, SIGNAL(clicked()), this, SLOT(deleteTemplateButtonClicked()));

	//Setup validator
	ui->editCustomX264Params->installEventFilter(this);
	ui->editCustomX264Params->setValidator(new StringValidatorEncoder(ui->labelNotificationX264, ui->iconNotificationX264));
	ui->editCustomX264Params->clear();
	ui->editCustomAvs2YUVParams->installEventFilter(this);
	ui->editCustomAvs2YUVParams->setValidator(new StringValidatorSource(ui->labelNotificationAvs2YUV, ui->iconNotificationAvs2YUV));
	ui->editCustomAvs2YUVParams->clear();

	//Install event filter
	ui->labelHelpScreenX264->installEventFilter(this);
	ui->labelHelpScreenAvs2YUV->installEventFilter(this);

	//Monitor for options changes
	connect(ui->cbxEncoderType, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->cbxEncoderArch, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->cbxEncoderVariant, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->cbxRateControlMode, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->spinQuantizer, SIGNAL(valueChanged(double)), this, SLOT(configurationChanged()));
	connect(ui->spinBitrate, SIGNAL(valueChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->cbxPreset, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->cbxTuning, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->cbxProfile, SIGNAL(currentIndexChanged(int)), this, SLOT(configurationChanged()));
	connect(ui->editCustomX264Params, SIGNAL(textChanged(QString)), this, SLOT(configurationChanged()));
	connect(ui->editCustomAvs2YUVParams, SIGNAL(textChanged(QString)), this, SLOT(configurationChanged()));

	//Create context menus
	ADD_CONTEXTMENU_ACTION(ui->editCustomX264Params, QIcon(":/buttons/page_edit.png"), tr("Open the Text-Editor"), editorActionTriggered);
	ADD_CONTEXTMENU_ACTION(ui->editCustomAvs2YUVParams, QIcon(":/buttons/page_edit.png"), tr("Open the Text-Editor"), editorActionTriggered);
	ADD_CONTEXTMENU_SEPARATOR(ui->editCustomX264Params);
	ADD_CONTEXTMENU_SEPARATOR(ui->editCustomAvs2YUVParams);
	ADD_CONTEXTMENU_ACTION(ui->editCustomX264Params, QIcon(":/buttons/page_copy.png"), tr("Copy to Clipboard"), copyActionTriggered);
	ADD_CONTEXTMENU_ACTION(ui->editCustomAvs2YUVParams, QIcon(":/buttons/page_copy.png"), tr("Copy to Clipboard"), copyActionTriggered);
	ADD_CONTEXTMENU_ACTION(ui->editCustomX264Params, QIcon(":/buttons/page_paste.png"), tr("Paste from Clipboard"), pasteActionTriggered);
	ADD_CONTEXTMENU_ACTION(ui->editCustomAvs2YUVParams, QIcon(":/buttons/page_paste.png"), tr("Paste from Clipboard"), pasteActionTriggered);

	//Setup template selector
	loadTemplateList();
	connect(ui->cbxTemplate, SIGNAL(currentIndexChanged(int)), this, SLOT(templateSelected()));

	//Force initial UI update
	encoderIndexChanged(ui->cbxEncoderType->currentIndex());
	m_monitorConfigChanges = true;
}

AddJobDialog::~AddJobDialog(void)
{
	//Free templates
	for(int i = 0; i < ui->cbxTemplate->model()->rowCount(); i++)
	{
		if(ui->cbxTemplate->itemText(i).startsWith("<") || ui->cbxTemplate->itemText(i).endsWith(">"))
		{
			continue;
		}
		const OptionsModel *item = reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(i).value<const void*>());
		ui->cbxTemplate->setItemData(i, QVariant::fromValue<const void*>(NULL));
		MUTILS_DELETE(item);
	}

	//Free validators
	if(const QValidator *tmp = ui->editCustomX264Params->validator())
	{
		ui->editCustomX264Params->setValidator(NULL);
		MUTILS_DELETE(tmp);
	}
	if(const QValidator *tmp = ui->editCustomAvs2YUVParams->validator())
	{
		ui->editCustomAvs2YUVParams->setValidator(NULL);
		MUTILS_DELETE(tmp);
	}

	MUTILS_DELETE(m_defaults);
	delete ui;
}

///////////////////////////////////////////////////////////////////////////////
// Events
///////////////////////////////////////////////////////////////////////////////

void AddJobDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	templateSelected();

	if((!ui->editSource->text().isEmpty()) && ui->editOutput->text().isEmpty())
	{
		QString outPath = generateOutputFileName(QDir::fromNativeSeparators(ui->editSource->text()), m_recentlyUsed->outputDirectory(), m_recentlyUsed->filterIndex(), m_preferences->getSaveToSourcePath());
		ui->editOutput->setText(QDir::toNativeSeparators(outPath));
		ui->buttonAccept->setFocus();
	}

	ui->labelNotificationX264->hide();
	ui->iconNotificationX264->hide();
	ui->labelNotificationAvs2YUV->hide();
	ui->iconNotificationAvs2YUV->hide();

	//Enable drag&drop support for this window, required for Qt v4.8.4+
	setAcceptDrops(true);
}

bool AddJobDialog::eventFilter(QObject *o, QEvent *e)
{
	if((o == ui->labelHelpScreenX264) && (e->type() == QEvent::MouseButtonPress))
	{
		OptionsModel options(m_sysinfo); saveOptions(&options);
		QScopedPointer<HelpDialog> helpScreen(new HelpDialog(this, false, m_sysinfo, &options, m_preferences));
		helpScreen->exec();
	}
	else if((o == ui->labelHelpScreenAvs2YUV) && (e->type() == QEvent::MouseButtonPress))
	{
		OptionsModel options(m_sysinfo); saveOptions(&options);
		QScopedPointer<HelpDialog> helpScreen(new HelpDialog(this, true, m_sysinfo, &options, m_preferences));
		helpScreen->exec();
	}
	else if((o == ui->editCustomX264Params) && (e->type() == QEvent::FocusOut))
	{
		ui->editCustomX264Params->setText(ui->editCustomX264Params->text().simplified());
	}
	else if((o == ui->editCustomAvs2YUVParams) && (e->type() == QEvent::FocusOut))
	{
		ui->editCustomAvs2YUVParams->setText(ui->editCustomAvs2YUVParams->text().simplified());
	}
	return false;
}

void AddJobDialog::dragEnterEvent(QDragEnterEvent *event)
{
	bool accept[2] = {false, false};

	foreach(const QString &fmt, event->mimeData()->formats())
	{
		accept[0] = accept[0] || fmt.contains("text/uri-list", Qt::CaseInsensitive);
		accept[1] = accept[1] || fmt.contains("FileNameW", Qt::CaseInsensitive);
	}

	if(accept[0] && accept[1])
	{
		event->acceptProposedAction();
	}
}

void AddJobDialog::dropEvent(QDropEvent *event)
{
	QString droppedFile;
	QList<QUrl> urls = event->mimeData()->urls();

	if(urls.count() > 1)
	{
		QDragEnterEvent dragEvent(event->pos(), event->proposedAction(), event->mimeData(), Qt::NoButton, Qt::NoModifier);
		if(qApp->notify(parent(), &dragEvent))
		{
			qApp->notify(parent(), event);
			reject(); return;
		}
	}

	while((!urls.isEmpty()) && droppedFile.isEmpty())
	{
		QUrl currentUrl = urls.takeFirst();
		QFileInfo file(currentUrl.toLocalFile());
		if(file.exists() && file.isFile())
		{
			qDebug("AddJobDialog::dropEvent: %s", file.canonicalFilePath().toUtf8().constData());
			droppedFile = file.canonicalFilePath();
		}
	}
	
	if(!droppedFile.isEmpty())
	{
		const QString outFileName = generateOutputFileName(droppedFile, currentOutputPath(), currentOutputIndx(), m_preferences->getSaveToSourcePath());
		ui->editSource->setText(QDir::toNativeSeparators(droppedFile));
		ui->editOutput->setText(QDir::toNativeSeparators(outFileName));
	}
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void AddJobDialog::encoderIndexChanged(int index)
{
	const OptionsModel::EncType encType = static_cast<OptionsModel::EncType>(ui->cbxEncoderType->itemData(ui->cbxEncoderType->currentIndex()).toInt());
	const AbstractEncoderInfo &encoderInfo = EncoderFactory::getEncoderInfo(encType);

	//Update encoder architectures
	const QList<AbstractEncoderInfo::ArchId> archs = encoderInfo.getArchitectures();
	ui->cbxEncoderArch->clear();
	for (quint32 archIdx = 0; archIdx < quint32(archs.count()); ++archIdx)
	{
		ui->cbxEncoderArch->addItem(archs[archIdx].first, archIdx);
	}

	//Update encoder variants
	const QStringList variants = encoderInfo.getVariants();
	ui->cbxEncoderVariant->clear();
	for(quint32 varntIdx = 0; varntIdx < quint32(variants.count()); ++varntIdx)
	{
		ui->cbxEncoderVariant->addItem(variants[varntIdx], varntIdx);
	}

	//Update encoder RC modes
	const QList<AbstractEncoderInfo::RCMode> rcModes = encoderInfo.getRCModes();
	ui->cbxRateControlMode->clear();
	for (quint32 rcIndex = 0; rcIndex < quint32(rcModes.count()); ++rcIndex)
	{
		ui->cbxRateControlMode->addItem(rcModes[rcIndex].first, rcIndex);
	}

	//Update presets
	const QStringList presets = encoderInfo.getPresets();
	if(presets.empty())
	{
		ui->cbxPreset->setEnabled(false);
		ui->cbxPreset->setCurrentIndex(0);
	}
	else
	{
		ui->cbxPreset->setEnabled(true);
		ui->cbxPreset->clear();
		ui->cbxPreset->addItem(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED));
		ui->cbxPreset->addItems(presets);
	}

	//Update tunings
	const QStringList tunings = encoderInfo.getTunings();
	if(tunings.empty())
	{
		ui->cbxTuning->setEnabled(false);
		ui->cbxTuning->setCurrentIndex(0);
	}
	else
	{
		ui->cbxTuning->setEnabled(true);
		ui->cbxTuning->clear();
		ui->cbxTuning->addItem(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED));
		ui->cbxTuning->addItems(tunings);
	}

	variantIndexChanged(ui->cbxEncoderVariant->currentIndex());
}

void AddJobDialog::variantIndexChanged(int index)
{
	const OptionsModel::EncType encType = static_cast<OptionsModel::EncType>(ui->cbxEncoderType->itemData(ui->cbxEncoderType->currentIndex()).toInt());
	const AbstractEncoderInfo &encoderInfo = EncoderFactory::getEncoderInfo(encType);

	//Update encoder profiles
	const QStringList profiles = encoderInfo.getProfiles(ui->cbxEncoderVariant->itemData(index).toUInt());
	if(profiles.empty())
	{
		ui->cbxProfile->setEnabled(false);
		ui->cbxProfile->setCurrentIndex(0);
	}
	else
	{
		ui->cbxProfile->setEnabled(true);
		ui->cbxProfile->clear();
		ui->cbxProfile->addItem(QString::fromLatin1(OptionsModel::PROFILE_UNRESTRICTED));
		ui->cbxProfile->addItems(profiles);
	}
	
	modeIndexChanged(ui->cbxRateControlMode->currentIndex());
}

void AddJobDialog::modeIndexChanged(int index)
{
	const OptionsModel::EncType encType = static_cast<OptionsModel::EncType>(ui->cbxEncoderType->itemData(ui->cbxEncoderType->currentIndex()).toInt());
	const AbstractEncoderInfo &encoderInfo = EncoderFactory::getEncoderInfo(encType);

	//Update bitrate/quantizer boxes
	const AbstractEncoderInfo::RCType rcType = encoderInfo.rcModeToType(ui->cbxRateControlMode->itemData(index).toUInt());
	ui->spinQuantizer->setEnabled(rcType == AbstractEncoderInfo::RC_TYPE_QUANTIZER);
	ui->spinBitrate  ->setEnabled(rcType != AbstractEncoderInfo::RC_TYPE_QUANTIZER);
}

void AddJobDialog::accept(void)
{
	//Get encoder info
	const OptionsModel::EncType encType = static_cast<OptionsModel::EncType>(ui->cbxEncoderType->itemData(ui->cbxEncoderType->currentIndex()).toInt());
	const AbstractEncoderInfo &encoderInfo = EncoderFactory::getEncoderInfo(encType);

	//Check 64-Bit support
	if (encoderInfo.archToType(ui->cbxEncoderArch->itemData(ui->cbxEncoderArch->currentIndex()).toUInt()) == AbstractEncoderInfo::ARCH_TYPE_X64)
	{
		if (!m_sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64))
		{
			QMessageBox::warning(this, tr("64-Bit unsupported!"), tr("<nobr>Sorry, this computer does <b>not</b> support 64-Bit encoders!</nobr>"));
			ui->cbxEncoderArch->setCurrentIndex(AbstractEncoderInfo::ARCH_TYPE_X86);
			return;
		}
	}
	
	//Selection complete?
	if(ui->editSource->text().trimmed().isEmpty())
	{
		QMessageBox::warning(this, tr("Not Found!"), tr("<nobr>Please select a valid source file first!<(nobr>"));
		return;
	}
	if(ui->editOutput->text().trimmed().isEmpty())
	{
		QMessageBox::warning(this, tr("Not Selected!"), tr("<nobr>Please select a valid output file first!</nobr>"));
		return;
	}

	//Does source exist?
	QFileInfo sourceFile = QFileInfo(this->sourceFile());
	if(!(sourceFile.exists() && sourceFile.isFile()))
	{
		QMessageBox::warning(this, tr("Not Found!"), tr("<nobr>The selected source file could not be found!</nobr>"));
		return;
	}

	//Is the type of the source file supported?
	const int sourceType = MediaInfo::analyze(sourceFile.canonicalFilePath());
	if(sourceType == MediaInfo::FILETYPE_AVISYNTH)
	{
		if(!m_sysinfo->hasAvisynth())
		{
			if(QMessageBox::warning(this, tr("Avisynth unsupported!"), tr("<nobr>An Avisynth script was selected as input, although Avisynth is <b>not</b> available!</nobr>"), tr("Abort"), tr("Ignore (at your own risk!)")) != 1)
			{
				return;
			}
		}
	}
	else if(sourceType == MediaInfo::FILETYPE_VAPOURSYNTH)
	{
		if(!m_sysinfo->hasVapourSynth())
		{
			if(QMessageBox::warning(this, tr("VapurSynth unsupported!"), tr("<nobr>A VapourSynth script was selected as input, although VapourSynth is <b>not/<b> available!</nobr>"), tr("Abort"), tr("Ignore (at your own risk!)")) != 1)
			{
				return;
			}
		}
	}
	else if(!encoderInfo.isInputTypeSupported(sourceType))
	{
		if(QMessageBox::warning(this, tr("Unsupported input format"), tr("<nobr>The selected encoder does <b>not</b> support the selected input format!</nobr>"), tr("Abort"), tr("Ignore (at your own risk!)")) != 1)
		{
			return;
		}
	}
	
	//Is output file extension supported by encoder?
	const QStringList outputFormats = encoderInfo.supportedOutputFormats();
	QFileInfo outputFile = QFileInfo(this->outputFile());
	if(!outputFormats.contains(outputFile.suffix(), Qt::CaseInsensitive))
	{
		QMessageBox::warning(this, tr("Unsupported output format"), tr("<nobr>Sorry, the selected encoder does not support the selected output format!</nobr>"));
		ui->editOutput->setText(QDir::toNativeSeparators(QString("%1/%2.%3").arg(outputFile.absolutePath(), outputFile.completeBaseName(), outputFormats.first())));
		return;
	}

	//Does output file already exist?
	if(outputFile.exists() && outputFile.isFile())
	{
		int ret = QMessageBox::question(this, tr("Already Exists!"), tr("<nobr>Output file already exists! Overwrite?</nobr>"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if(ret != QMessageBox::Yes) return;
	}
	if(outputFile.exists() && (!outputFile.isFile()))
	{
		QMessageBox::warning(this, tr("Not a File!"), tr("<nobr>Selected output file does not appear to be a valid file!</nobr>"));
		return;
	}

	//Is destination dir writable?
	QFileInfo outputDir = QFileInfo(outputFile.absolutePath());
	if(!(outputDir.exists() && outputDir.isDir() && outputDir.isWritable()))
	{
		QMessageBox::warning(this, tr("Not Writable!"), tr("<nobr>Output directory does not exist or is not writable!</nobr>"));
		return;
	}

	//Custom parameters okay?
	if(!ui->editCustomX264Params->hasAcceptableInput())
	{
		int ret = QMessageBox::warning(this, tr("Invalid Params"), tr("<nobr>Your custom parameters are invalid and will be discarded!</nobr>"), QMessageBox::Ignore | QMessageBox::Cancel, QMessageBox::Cancel);
		if(ret != QMessageBox::Ignore) return;
	}

	//Update recently used
	m_recentlyUsed->setFilterIndex(currentOutputIndx());
	m_recentlyUsed->setSourceDirectory(currentSourcePath());
	m_recentlyUsed->setOutputDirectory(currentOutputPath());
	RecentlyUsed::saveRecentlyUsed(m_recentlyUsed);

	//Save options
	saveOptions(m_options);
	QDialog::accept();
}

void AddJobDialog::browseButtonClicked(void)
{
	if(QObject::sender() == ui->buttonBrowseSource)
	{
		QString filePath = QFileDialog::getOpenFileName(this, tr("Open Source File"), currentSourcePath(true), getInputFilterLst(), NULL, QFileDialog::DontUseNativeDialog);
		if(!(filePath.isNull() || filePath.isEmpty()))
		{
			QString destFile = generateOutputFileName(filePath, currentOutputPath(), currentOutputIndx(), m_preferences->getSaveToSourcePath());
			ui->editSource->setText(QDir::toNativeSeparators(filePath));
			ui->editOutput->setText(QDir::toNativeSeparators(destFile));
		}
	}
	else if(QObject::sender() == ui->buttonBrowseOutput)
	{
		QString selectedType = getFilterStr(currentOutputIndx());
		QString filePath = QFileDialog::getSaveFileName(this, tr("Choose Output File"), currentOutputPath(true), getFilterLst(), &selectedType, QFileDialog::DontUseNativeDialog | QFileDialog::DontConfirmOverwrite);

		if(!(filePath.isNull() || filePath.isEmpty()))
		{
			if(getFilterIdx(QFileInfo(filePath).suffix()) < 0)
			{
				int tempIndex = -1;
				QRegExp regExp("\\(\\*\\.(\\w+)\\)");
				if(regExp.lastIndexIn(selectedType) >= 0)
				{
					tempIndex = getFilterIdx(regExp.cap(1));
				}
				if(tempIndex < 0)
				{
					tempIndex = m_recentlyUsed->filterIndex();
				}
				filePath = QString("%1.%2").arg(filePath, getFilterExt(tempIndex));
			}
			ui->editOutput->setText(QDir::toNativeSeparators(filePath));
		}
	}
}

void AddJobDialog::configurationChanged(void)
{
	if(!m_monitorConfigChanges)
	{
		return;
	}

	const OptionsModel* options = reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(ui->cbxTemplate->currentIndex()).value<const void*>());
	if(options)
	{
		ui->cbxTemplate->blockSignals(true);
		ui->cbxTemplate->insertItem(0, tr("<Modified Configuration>"), QVariant::fromValue<const void*>(NULL));
		ui->cbxTemplate->setCurrentIndex(0);
		ui->cbxTemplate->blockSignals(false);
	}
}

void AddJobDialog::templateSelected(void)
{
	const OptionsModel* options = reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(ui->cbxTemplate->currentIndex()).value<const void*>());
	if(options)
	{
		qDebug("Loading options!");
		m_lastTemplateName = ui->cbxTemplate->itemText(ui->cbxTemplate->currentIndex());
		REMOVE_USAFED_ITEM;
		restoreOptions(options);
	}
}

void AddJobDialog::saveTemplateButtonClicked(void)
{
	qDebug("Saving template");

	QString name = m_lastTemplateName;
	if(name.isEmpty() || name.contains('<') || name.contains('>'))
	{
		name = tr("New Template");
		int n = 1;
		while(OptionsModel::templateExists(name))
		{
			name = tr("New Template (%1)").arg(QString::number(++n));
		}
	}

	QScopedPointer<OptionsModel> options(new OptionsModel(m_sysinfo));
	saveOptions(options.data());

	if(options->equals(m_defaults))
	{
		QMessageBox::warning (this, tr("Oups"), tr("<nobr>It makes no sense to save the default settings!</nobr>"));
		ui->cbxTemplate->blockSignals(true);
		ui->cbxTemplate->setCurrentIndex(0);
		ui->cbxTemplate->blockSignals(false);
		REMOVE_USAFED_ITEM;
		return;
	}

	for(int i = 0; i < ui->cbxTemplate->count(); i++)
	{
		const QString tempName = ui->cbxTemplate->itemText(i);
		if(tempName.contains('<') || tempName.contains('>'))
		{
			continue;
		}
		const OptionsModel* test = reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(i).value<const void*>());
		if(test != NULL)
		{
			if(options->equals(test))
			{
				QMessageBox::information (this, tr("Oups"), tr("<nobr>The current settings are already saved as template:<br><b>%1</b></nobr>").arg(ui->cbxTemplate->itemText(i)));
				ui->cbxTemplate->blockSignals(true);
				ui->cbxTemplate->setCurrentIndex(i);
				ui->cbxTemplate->blockSignals(false);
				REMOVE_USAFED_ITEM;
				return;
			}
		}
	}

	forever
	{
		bool ok = false;
		
		QStringList items;
		items << name;
		for(int i = 0; i < ui->cbxTemplate->count(); i++)
		{
			const QString tempName = ui->cbxTemplate->itemText(i);
			if(!(tempName.contains('<') || tempName.contains('>')))
			{
				items << tempName;
			}
		}
		
		name = QInputDialog::getItem(this, tr("Save Template"), tr("Please enter the name of the template:").leftJustified(144, ' '), items, 0, true, &ok).simplified();
		if(!ok)
		{
			return;
		}
		if(name.isEmpty())
		{
			continue;
		}
		if(name.contains('<') || name.contains('>') || name.contains('\\') || name.contains('/') || name.contains('"'))
		{
			QMessageBox::warning (this, tr("Invalid Name"), tr("<nobr>Sorry, the name you have entered is invalid!</nobr>"));
			while(name.contains('<')) name.remove('<');
			while(name.contains('>')) name.remove('>');
			while(name.contains('\\')) name.remove('\\');
			while(name.contains('/')) name.remove('/');
			while(name.contains('"')) name.remove('"');
			name = name.simplified();
			continue;
		}
		if(OptionsModel::templateExists(name))
		{
			int ret = QMessageBox::warning (this, tr("Already Exists"), tr("<nobr>A template of that name already exists! Overwrite?</nobr>"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
			if(ret != QMessageBox::Yes)
			{
				continue;
			}
		}
		break;
	}
	
	if(!OptionsModel::saveTemplate(options.data(), name))
	{
		QMessageBox::critical(this, tr("Save Failed"), tr("Sorry, the template could not be saved!"));
		return;
	}
	
	ui->cbxTemplate->blockSignals(true);
	for(int i = 0; i < ui->cbxTemplate->count(); i++)
	{
		if(ui->cbxTemplate->itemText(i).compare(name, Qt::CaseInsensitive) == 0)
		{
			QScopedPointer<const OptionsModel> oldItem(reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(i).value<const void*>()));
			ui->cbxTemplate->setItemData(i, QVariant::fromValue<const void*>(options.take()));
			ui->cbxTemplate->setCurrentIndex(i);
		}
	}
	if(!options.isNull())
	{
		const int index = ui->cbxTemplate->model()->rowCount();
		ui->cbxTemplate->insertItem(index, name, QVariant::fromValue<const void*>(options.take()));
		ui->cbxTemplate->setCurrentIndex(index);
	}
	ui->cbxTemplate->blockSignals(false);

	m_lastTemplateName = name;
	REMOVE_USAFED_ITEM;
}

void AddJobDialog::deleteTemplateButtonClicked(void)
{
	const int index = ui->cbxTemplate->currentIndex();
	QString name = ui->cbxTemplate->itemText(index);

	if(name.contains('<') || name.contains('>') || name.contains('\\') || name.contains('/'))
	{
		QMessageBox::warning (this, tr("Invalid Item"), tr("Sorry, the selected item cannot be deleted!"));
		return;
	}

	int ret = QMessageBox::question (this, tr("Delete Template"), tr("<nobr>Do you really want to delete the selected template?<br><b>%1</b></nobr>").arg(name), QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
	if(ret != QMessageBox::Yes)
	{
		return;
	}
	
	OptionsModel::deleteTemplate(name);
	const OptionsModel *item = reinterpret_cast<const OptionsModel*>(ui->cbxTemplate->itemData(index).value<const void*>());
	ui->cbxTemplate->removeItem(index);
	MUTILS_DELETE(item);
}

void AddJobDialog::editorActionTriggered(void)
{

	if(QAction *action = dynamic_cast<QAction*>(QObject::sender()))
	{
		QLineEdit *lineEdit = reinterpret_cast<QLineEdit*>(action->data().value<void*>());
		
		EditorDialog *editor = new EditorDialog(this);
		editor->setEditText(lineEdit->text());

		if(editor->exec() == QDialog::Accepted)
		{
			lineEdit->setText(editor->getEditText());
		}

		MUTILS_DELETE(editor);
	}
}

void AddJobDialog::copyActionTriggered(void)
{
	if(QAction *action = dynamic_cast<QAction*>(QObject::sender()))
	{
		QClipboard *clipboard = QApplication::clipboard();
		QLineEdit *lineEdit = reinterpret_cast<QLineEdit*>(action->data().value<void*>());
		QString text = lineEdit->hasSelectedText() ? lineEdit->selectedText() : lineEdit->text();
		clipboard->setText(text);
	}
}

void AddJobDialog::pasteActionTriggered(void)
{
	if(QAction *action = dynamic_cast<QAction*>(QObject::sender()))
	{
		QClipboard *clipboard = QApplication::clipboard();
		QLineEdit *lineEdit = reinterpret_cast<QLineEdit*>(action->data().value<void*>());
		QString text = clipboard->text();
		if(!text.isEmpty()) lineEdit->setText(text);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Public functions
///////////////////////////////////////////////////////////////////////////////

QString AddJobDialog::sourceFile(void)
{
	return QDir::fromNativeSeparators(ui->editSource->text());
}

QString AddJobDialog::outputFile(void)
{
	return QDir::fromNativeSeparators(ui->editOutput->text());
}

bool AddJobDialog::runImmediately(void)
{
	return ui->checkBoxRun->isChecked();
}

bool AddJobDialog::applyToAll(void)
{
	return ui->checkBoxApplyToAll->isChecked();
}

void AddJobDialog::setRunImmediately(bool run)
{
	ui->checkBoxRun->setChecked(run);
}

void AddJobDialog::setSourceFile(const QString &path)
{
	ui->editSource->setText(QDir::toNativeSeparators(path));
}

void AddJobDialog::setOutputFile(const QString &path)
{
	ui->editOutput->setText(QDir::toNativeSeparators(path));}

void AddJobDialog::setSourceEditable(const bool editable)
{
	ui->buttonBrowseSource->setEnabled(editable);
}

void AddJobDialog::setApplyToAllVisible(const bool visible)
{
	ui->checkBoxApplyToAll->setVisible(visible);
}

///////////////////////////////////////////////////////////////////////////////
// Private functions
///////////////////////////////////////////////////////////////////////////////

void AddJobDialog::loadTemplateList(void)
{
	ui->cbxTemplate->addItem(tr("<Default>"), QVariant::fromValue<const void*>(m_defaults));
	ui->cbxTemplate->setCurrentIndex(0);

	QMap<QString, OptionsModel*> templates = OptionsModel::loadAllTemplates(m_sysinfo);
	QStringList templateNames = templates.keys();
	templateNames.sort();

	for(QStringList::ConstIterator current = templateNames.constBegin(); current != templateNames.constEnd(); current++)
	{
		OptionsModel *currentTemplate = templates.take(*current);
		ui->cbxTemplate->addItem(*current, QVariant::fromValue<const void*>(currentTemplate));
		if(currentTemplate->equals(m_options))
		{
			ui->cbxTemplate->setCurrentIndex(ui->cbxTemplate->count() - 1);
		}
	}

	if((ui->cbxTemplate->currentIndex() == 0) && (!m_options->equals(m_defaults)))
	{
		qWarning("Not the default -> recently used!");
		ui->cbxTemplate->insertItem(1, tr("<Recently Used>"), QVariant::fromValue<const void*>(m_options));
		ui->cbxTemplate->setCurrentIndex(1);
	}
}

void AddJobDialog::updateComboBox(QComboBox *const cbox, const QString &text)
{
	int index = 0;
	if(QAbstractItemModel *model = cbox->model())
	{
		for(int i = 0; i < cbox->model()->rowCount(); i++)
		{
			if(model->data(model->index(i, 0, QModelIndex())).toString().compare(text, Qt::CaseInsensitive) == 0)
			{
				index = i;
				break;
			}
		}
	}
	cbox->setCurrentIndex(index);
}

void AddJobDialog::updateComboBox(QComboBox *const cbox, const int &data)
{
	int index = 0;
	if (QAbstractItemModel *model = cbox->model())
	{
		for (int i = 0; i < cbox->model()->rowCount(); i++)
		{
			if (cbox->itemData(i).toInt() == data)
			{
				index = i;
				break;
			}
		}
	}
	cbox->setCurrentIndex(index);
}

void AddJobDialog::updateComboBox(QComboBox *const cbox, const quint32 &data)
{
	int index = 0;
	if(QAbstractItemModel *model = cbox->model())
	{
		for(int i = 0; i < cbox->model()->rowCount(); i++)
		{
			if(cbox->itemData(i).toUInt() == data)
			{
				index = i;
				break;
			}
		}
	}
	cbox->setCurrentIndex(index);
}

void AddJobDialog::restoreOptions(const OptionsModel *options)
{
	DisableHelperRAII disable(&m_monitorConfigChanges);

	updateComboBox(ui->cbxEncoderType,     options->encType());
	updateComboBox(ui->cbxEncoderArch,     options->encArch());
	updateComboBox(ui->cbxEncoderVariant,  options->encVariant());
	updateComboBox(ui->cbxRateControlMode, options->rcMode());

	ui->spinQuantizer->setValue(options->quantizer());
	ui->spinBitrate  ->setValue(options->bitrate());

	updateComboBox(ui->cbxPreset,  options->preset());
	updateComboBox(ui->cbxTuning,  options->tune());
	updateComboBox(ui->cbxProfile, options->profile());

	ui->editCustomX264Params   ->setText(options->customEncParams());
	ui->editCustomAvs2YUVParams->setText(options->customAvs2YUV());
}

void AddJobDialog::saveOptions(OptionsModel *options)
{
	options->setEncType(static_cast<OptionsModel::EncType>(ui->cbxEncoderType->itemData(ui->cbxEncoderType->currentIndex()).toInt()));

	options->setEncArch   (ui->cbxEncoderArch    ->itemData(ui->cbxEncoderArch    ->currentIndex()).toUInt());
	options->setEncVariant(ui->cbxEncoderVariant ->itemData(ui->cbxEncoderVariant ->currentIndex()).toUInt());
	options->setRCMode    (ui->cbxRateControlMode->itemData(ui->cbxRateControlMode->currentIndex()).toUInt());
	
	options->setQuantizer(ui->spinQuantizer->value());
	options->setBitrate  (ui->spinBitrate  ->value());
	
	options->setPreset (ui->cbxPreset ->model()->data(ui->cbxPreset ->model()->index(ui->cbxPreset ->currentIndex(), 0)).toString());
	options->setTune   (ui->cbxTuning ->model()->data(ui->cbxTuning ->model()->index(ui->cbxTuning ->currentIndex(), 0)).toString());
	options->setProfile(ui->cbxProfile->model()->data(ui->cbxProfile->model()->index(ui->cbxProfile->currentIndex(), 0)).toString());

	options->setCustomEncParams(ui->editCustomX264Params->hasAcceptableInput() ? ui->editCustomX264Params->text().simplified()     : QString());
	options->setCustomAvs2YUV(ui->editCustomAvs2YUVParams->hasAcceptableInput() ? ui->editCustomAvs2YUVParams->text().simplified() : QString());
}

QString AddJobDialog::currentSourcePath(const bool bWithName)
{
	QString path = m_recentlyUsed->sourceDirectory();
	QString currentSourceFile = this->sourceFile();
	
	if(!currentSourceFile.isEmpty())
	{
		QString currentSourceDir = QFileInfo(currentSourceFile).absolutePath();
		if(VALID_DIR(currentSourceDir))
		{
			path = currentSourceDir;
		}
		if(bWithName)
		{
			path.append("/").append(QFileInfo(currentSourceFile).fileName());
		}
	}

	return path;
}

QString AddJobDialog::currentOutputPath(const bool bWithName)
{
	QString path = m_recentlyUsed->outputDirectory();
	QString currentOutputFile = this->outputFile();
	
	if(!currentOutputFile.isEmpty())
	{
		QString currentOutputDir = QFileInfo(currentOutputFile).absolutePath();
		if(VALID_DIR(currentOutputDir))
		{
			path = currentOutputDir;
		}
		if(bWithName)
		{
			path.append("/").append(QFileInfo(currentOutputFile).fileName());
		}
	}

	return path;
}

int AddJobDialog::currentOutputIndx(void)
{
	const OptionsModel::EncType encType = static_cast<OptionsModel::EncType>(ui->cbxEncoderType->itemData(ui->cbxEncoderType->currentIndex()).toInt());
	if(encType == OptionsModel::EncType_X265)
	{
		return ARRAY_SIZE(X264_FILE_TYPE_FILTERS) - 1;
	}
	
	int index = m_recentlyUsed->filterIndex();
	const QString currentOutputFile = this->outputFile();

	if(!currentOutputFile.isEmpty())
	{
		const QString currentOutputExtn = QFileInfo(currentOutputFile).suffix();
		const int tempIndex = getFilterIdx(currentOutputExtn);
		if(tempIndex >= 0)
		{
			index = tempIndex;
		}
	}

	return index;
}

///////////////////////////////////////////////////////////////////////////////
// Static functions
///////////////////////////////////////////////////////////////////////////////

QString AddJobDialog::generateOutputFileName(const QString &sourceFilePath, const QString &destinationDirectory, const int filterIndex, const bool saveToSourceDir)
{
	QString name = QFileInfo(sourceFilePath).completeBaseName();
	QString path = saveToSourceDir ? QFileInfo(sourceFilePath).canonicalPath() : destinationDirectory;
	QString fext = getFilterExt(filterIndex);
	
	if(!VALID_DIR(path))
	{
		RecentlyUsed defaults;
		path = defaults.outputDirectory();
	}

	QString outPath = QString("%1/%2.%3").arg(path, name, fext);

	int n = 2;
	while(QFileInfo(outPath).exists())
	{
		outPath = QString("%1/%2 (%3).%4").arg(path, name, QString::number(n++), fext);
	}

	return outPath;
}

/* ------------------------------------------------------------------------- */

QString AddJobDialog::getFilterExt(const int filterIndex)
{
	const int count = ARRAY_SIZE(X264_FILE_TYPE_FILTERS);

	if((filterIndex >= 0) && (filterIndex < count))
	{
		return QString::fromLatin1(X264_FILE_TYPE_FILTERS[filterIndex].pcExt);
	}

	return QString::fromLatin1(X264_FILE_TYPE_FILTERS[0].pcExt);
}

int AddJobDialog::getFilterIdx(const QString &fileExt)
{
	const int count = ARRAY_SIZE(X264_FILE_TYPE_FILTERS);

	for(int i = 0; i < count; i++)
	{
		if(fileExt.compare(QString::fromLatin1(X264_FILE_TYPE_FILTERS[i].pcExt), Qt::CaseInsensitive) == 0)
		{
			return i;
		}
	}

	return -1;
}

QString AddJobDialog::getFilterStr(const int filterIndex)
{
	const int count = ARRAY_SIZE(X264_FILE_TYPE_FILTERS);

	if((filterIndex >= 0) && (filterIndex < count))
	{
		return QString("%1 (*.%2)").arg(QString::fromLatin1(X264_FILE_TYPE_FILTERS[filterIndex].pcStr), QString::fromLatin1(X264_FILE_TYPE_FILTERS[filterIndex].pcExt));
	}

	return QString("%1 (*.%2)").arg(QString::fromLatin1(X264_FILE_TYPE_FILTERS[0].pcStr), QString::fromLatin1(X264_FILE_TYPE_FILTERS[0].pcExt));
}

QString AddJobDialog::getFilterLst(void)
{
	QStringList filters;
	const int count = ARRAY_SIZE(X264_FILE_TYPE_FILTERS);
	
	for(int i = 0; i < count; i++)
	{
		filters << QString("%1 (*.%2)").arg(QString::fromLatin1(X264_FILE_TYPE_FILTERS[i].pcStr), QString::fromLatin1(X264_FILE_TYPE_FILTERS[i].pcExt));
	}

	return filters.join(";;");
}

QString AddJobDialog::getInputFilterLst(void)
{
	static const struct
	{
		const char *name;
		const char *fext;
	}
	s_filters[] =
	{
		{"Avisynth Scripts", "avs"},
		{"VapourSynth Scripts", "vpy"},
		{"Matroska Files", "mkv"},
		{"MPEG-4 Part 14 Container", "mp4"},
		{"Audio Video Interleaved", "avi"},
		{"Flash Video", "flv"},
		{"YUV4MPEG2 Stream", "y4m"},
		{"Uncompresses YUV Data", "yuv"},
	};

	const int count = ARRAY_SIZE(s_filters);

	QString allTypes;
	for(size_t index = 0; index < count; index++)
	{

		allTypes += QString((index > 0) ? " *.%1" : "*.%1").arg(QString::fromLatin1(s_filters[index].fext));
	}
	
	QStringList filters;
	filters << QString("All supported files (%1)").arg(allTypes);

	for(size_t index = 0; index < count; index++)
	{
		filters << QString("%1 (*.%2)").arg(QString::fromLatin1(s_filters[index].name), QString::fromLatin1(s_filters[index].fext));
	}
		
	filters << QString("All files (*.*)");
	return filters.join(";;");
}


