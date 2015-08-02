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

#include "model_options.h"

//Internal
#include "global.h"
#include "model_sysinfo.h"

//Mutils
#include <MUtils/Global.h>

//Qt
#include <QDesktopServices>
#include <QSettings>
#include <QStringList>
#include <QApplication>

#define COMPARE_VAL(OTHER, NAME) ((this->NAME) == (OTHER->NAME))
#define COMPARE_STR(OTHER, NAME) ((this->NAME).compare((model->NAME), Qt::CaseInsensitive) == 0)
#define ASSIGN_FROM(OTHER, NAME) ((this->NAME) = (OTHER.NAME))

//Template keys
static const char *KEY_ENCODER_TYPE    = "encoder_type";
static const char *KEY_ENCODER_ARCH    = "encoder_arch";
static const char *KEY_ENCODER_VARIANT = "encoder_variant";
static const char *KEY_RATECTRL_MODE   = "rate_control_mode";
static const char *KEY_TARGET_BITRATE  = "target_bitrate";
static const char *KEY_TARGET_QUANT    = "target_quantizer";
static const char *KEY_PRESET_NAME     = "preset_name";
static const char *KEY_TUNING_NAME     = "tuning_name";
static const char *KEY_PROFILE_NAME    = "profile_name";
static const char *KEY_CUSTOM_ENCODER  = "custom_params_encoder";
static const char *KEY_CUSTOM_AVS2YUV  = "custom_params_avs2yuv";

const char *const OptionsModel::TUNING_UNSPECIFIED   = "<None>";
const char *const OptionsModel::PROFILE_UNRESTRICTED = "<Unrestricted>";

OptionsModel::OptionsModel(const SysinfoModel *sysinfo)
{
	m_encoderType = EncType_X264;
	m_encoderArch = sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64) ? EncArch_x86_64 : EncArch_x86_32;
	m_encoderVariant = EncVariant_8Bit;
	m_rcMode = RCMode_CRF;
	m_bitrate = 1200;
	m_quantizer = 22;
	m_preset = "Medium";
	m_tune = QString::fromLatin1(OptionsModel::TUNING_UNSPECIFIED);
	m_profile = QString::fromLatin1(OptionsModel::PROFILE_UNRESTRICTED);
	m_custom_encoder = QString();
	m_custom_avs2yuv = QString();
}

OptionsModel::OptionsModel(const OptionsModel &rhs)
{
	ASSIGN_FROM(rhs, m_encoderType);
	ASSIGN_FROM(rhs, m_encoderArch);
	ASSIGN_FROM(rhs, m_encoderVariant);
	ASSIGN_FROM(rhs, m_rcMode);
	ASSIGN_FROM(rhs, m_bitrate);
	ASSIGN_FROM(rhs, m_quantizer);
	ASSIGN_FROM(rhs, m_preset);
	ASSIGN_FROM(rhs, m_tune);
	ASSIGN_FROM(rhs, m_profile);
	ASSIGN_FROM(rhs, m_custom_encoder);
	ASSIGN_FROM(rhs, m_custom_avs2yuv);
}

OptionsModel::~OptionsModel(void)
{
}

QString OptionsModel::rcMode2String(RCMode mode)
{
	switch(mode)
	{
	case RCMode_CRF:
		return QObject::tr("CRF");
		break;
	case RCMode_CQ:
		return QObject::tr("CQ");
		break;
	case RCMode_2Pass:
		return QObject::tr("2-Pass");
		break;
	case RCMode_ABR:
		return QObject::tr("ABR");
		break;
	default:
		return QString();
		break;
	}
}

bool OptionsModel::equals(const OptionsModel *model)
{
	bool equal = true;

	equal = equal && COMPARE_VAL(model, m_encoderType);
	equal = equal && COMPARE_VAL(model, m_encoderArch);
	equal = equal && COMPARE_VAL(model, m_encoderVariant);
	equal = equal && COMPARE_VAL(model, m_rcMode);
	equal = equal && COMPARE_VAL(model, m_bitrate);
	equal = equal && COMPARE_VAL(model, m_quantizer);
	equal = equal && COMPARE_STR(model, m_preset);
	equal = equal && COMPARE_STR(model, m_tune);
	equal = equal && COMPARE_STR(model, m_profile);
	equal = equal && COMPARE_STR(model, m_custom_encoder);
	equal = equal && COMPARE_STR(model, m_custom_avs2yuv);

	return equal;
}

bool OptionsModel::saveTemplate(const OptionsModel *model, const QString &name)
{
	const QString templateName = name.simplified();
	const QString appDir = x264_data_path();

	if(templateName.contains('\\') || templateName.contains('/'))
	{
		return false;
	}

	QSettings settings(QString("%1/templates.ini").arg(appDir), QSettings::IniFormat);
	settings.beginGroup(templateName);

	const bool okay = saveOptions(model, settings);

	settings.endGroup();
	settings.sync();
	
	return okay;
}

bool OptionsModel::loadTemplate(OptionsModel *model, const QString &name)
{
	const QString appDir = x264_data_path();
	
	if(name.contains('\\') || name.contains('/'))
	{
		return false;
	}
	
	QSettings settings(QString("%1/templates.ini").arg(appDir), QSettings::IniFormat);
	
	settings.beginGroup(name);
	const bool okay = loadOptions(model, settings);
	settings.endGroup();

	return okay;
}

QMap<QString, OptionsModel*> OptionsModel::loadAllTemplates(const SysinfoModel *sysinfo)
{
	QMap<QString, OptionsModel*> list;
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/templates.ini").arg(appDir), QSettings::IniFormat);
	QStringList allTemplates = settings.childGroups();

	while(!allTemplates.isEmpty())
	{
		QString name = allTemplates.takeFirst();
		if(!(name.contains('<') || name.contains('>') || name.contains('\\') || name.contains('/')))
		{
			OptionsModel *options = new OptionsModel(sysinfo);
			if(loadTemplate(options, name))
			{
				list.insert(name, options);
				continue;
			}
			MUTILS_DELETE(options);
		}
	}

	return list;
}

bool OptionsModel::templateExists(const QString &name)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/templates.ini").arg(appDir), QSettings::IniFormat);
	QStringList allGroups = settings.childGroups();
	return allGroups.contains(name, Qt::CaseInsensitive);
}

bool OptionsModel::deleteTemplate(const QString &name)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/templates.ini").arg(appDir), QSettings::IniFormat);

	if(settings.childGroups().contains(name, Qt::CaseInsensitive))
	{
		settings.remove(name);
		return true;
	}

	return false;
}

bool OptionsModel::saveOptions(const OptionsModel *model, QSettings &settingsFile)
{
	settingsFile.setValue(KEY_ENCODER_TYPE,    model->m_encoderType);
	settingsFile.setValue(KEY_ENCODER_ARCH,    model->m_encoderArch);
	settingsFile.setValue(KEY_ENCODER_VARIANT, model->m_encoderVariant);
	settingsFile.setValue(KEY_RATECTRL_MODE,   model->m_rcMode);
	settingsFile.setValue(KEY_TARGET_BITRATE,  model->m_bitrate);
	settingsFile.setValue(KEY_TARGET_QUANT,    model->m_quantizer);
	settingsFile.setValue(KEY_PRESET_NAME,     model->m_preset);
	settingsFile.setValue(KEY_TUNING_NAME,     model->m_tune);
	settingsFile.setValue(KEY_PROFILE_NAME,    model->m_profile);
	settingsFile.setValue(KEY_CUSTOM_ENCODER,  model->m_custom_encoder);
	settingsFile.setValue(KEY_CUSTOM_AVS2YUV,  model->m_custom_avs2yuv);
	
	return true;
}

bool OptionsModel::loadOptions(OptionsModel *model, QSettings &settingsFile)
{
	fixTemplate(settingsFile); /*for backward compatibility*/
	
	bool complete = true;
	if(!settingsFile.contains(KEY_ENCODER_TYPE))    complete = false;
	if(!settingsFile.contains(KEY_ENCODER_ARCH))    complete = false;
	if(!settingsFile.contains(KEY_ENCODER_VARIANT)) complete = false;
	if(!settingsFile.contains(KEY_RATECTRL_MODE))   complete = false;
	if(!settingsFile.contains(KEY_TARGET_BITRATE))  complete = false;
	if(!settingsFile.contains(KEY_TARGET_QUANT))    complete = false;
	if(!settingsFile.contains(KEY_PRESET_NAME))     complete = false;
	if(!settingsFile.contains(KEY_TUNING_NAME))     complete = false;
	if(!settingsFile.contains(KEY_PROFILE_NAME))    complete = false;
	if(!settingsFile.contains(KEY_CUSTOM_ENCODER))  complete = false;
	if(!settingsFile.contains(KEY_CUSTOM_AVS2YUV))  complete = false;

	if(complete)
	{
		model->setEncType        (static_cast<OptionsModel::EncType>   (settingsFile.value(KEY_ENCODER_TYPE,    model->m_encoderType)   .toInt()));
		model->setEncArch        (static_cast<OptionsModel::EncArch>   (settingsFile.value(KEY_ENCODER_ARCH,    model->m_encoderArch)   .toInt()));
		model->setEncVariant     (static_cast<OptionsModel::EncVariant>(settingsFile.value(KEY_ENCODER_VARIANT, model->m_encoderVariant).toInt()));
		model->setRCMode         (static_cast<OptionsModel::RCMode>    (settingsFile.value(KEY_RATECTRL_MODE,   model->m_rcMode)        .toInt()));
		model->setBitrate        (settingsFile.value(KEY_TARGET_BITRATE, model->m_bitrate)       .toUInt()  );
		model->setQuantizer      (settingsFile.value(KEY_TARGET_QUANT,   model->m_quantizer)     .toDouble());
		model->setPreset         (settingsFile.value(KEY_PRESET_NAME,    model->m_preset)        .toString());
		model->setTune           (settingsFile.value(KEY_TUNING_NAME,    model->m_tune)          .toString());
		model->setProfile        (settingsFile.value(KEY_PROFILE_NAME,   model->m_profile)       .toString());
		model->setCustomEncParams(settingsFile.value(KEY_CUSTOM_ENCODER, model->m_custom_encoder).toString());
		model->setCustomAvs2YUV  (settingsFile.value(KEY_CUSTOM_AVS2YUV, model->m_custom_avs2yuv).toString());
	}
	
	return complete;
}

void OptionsModel::fixTemplate(QSettings &settingsFile)
{
	if(!(settingsFile.contains(KEY_ENCODER_TYPE) || settingsFile.contains(KEY_ENCODER_ARCH) || settingsFile.contains(KEY_ENCODER_VARIANT)))
	{
		settingsFile.setValue(KEY_ENCODER_TYPE,    OptionsModel::EncType_X264);
		settingsFile.setValue(KEY_ENCODER_ARCH,    OptionsModel::EncArch_x86_32);
		settingsFile.setValue(KEY_ENCODER_VARIANT, OptionsModel::EncVariant_8Bit);
	}

	static const char *legacyKey[] = { "custom_params", "custom_params_x264", NULL };
	for(int i = 0; legacyKey[i]; i++)
	{
		if(settingsFile.contains(legacyKey[i]))
		{
			settingsFile.setValue(KEY_CUSTOM_ENCODER, settingsFile.value(legacyKey[i]));
			settingsFile.remove(legacyKey[i]);
		}
	}

	if(settingsFile.value(KEY_PROFILE_NAME).toString().compare("auto", Qt::CaseInsensitive) == 0)
	{
		settingsFile.setValue(KEY_PROFILE_NAME, QString::fromLatin1(OptionsModel::PROFILE_UNRESTRICTED));
	}
	if(settingsFile.value(KEY_TUNING_NAME).toString().compare("none", Qt::CaseInsensitive) == 0)
	{
		settingsFile.setValue(KEY_TUNING_NAME, QString::fromLatin1(OptionsModel::TUNING_UNSPECIFIED));
	}
}
