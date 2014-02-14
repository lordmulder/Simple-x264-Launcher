///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2014 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "global.h"
#include "model_sysinfo.h"

#include <QDesktopServices>
#include <QSettings>
#include <QStringList>
#include <QApplication>

#define COMPARE_VAL(OTHER, NAME) ((this->NAME) == (OTHER->NAME))
#define COMPARE_STR(OTHER, NAME) ((this->NAME).compare((model->NAME), Qt::CaseInsensitive) == 0)
#define ASSIGN_FROM(OTHER, NAME) ((this->NAME) = (OTHER.NAME))

OptionsModel::OptionsModel(const SysinfoModel *sysinfo)
{
	m_encoderType = EncType_X264;
	m_encoderArch = sysinfo->hasX64Support() ? EncArch_x64 : EncArch_x32;
	m_encoderVariant = EncVariant_LoBit;
	m_rcMode = RCMode_CRF;
	m_bitrate = 1200;
	m_quantizer = 22;
	m_preset = "Medium";
	m_tune = "None";
	m_profile = "Auto";
	m_custom_encoder = "";
	m_custom_avs2yuv = "";
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

bool OptionsModel::saveTemplate(OptionsModel *model, const QString &name)
{
	const QString templateName = name.simplified();
	const QString appDir = x264_data_path();

	if(templateName.contains('\\') || templateName.contains('/'))
	{
		return false;
	}

	QSettings settings(QString("%1/templates.ini").arg(appDir), QSettings::IniFormat);
	settings.beginGroup(templateName);
	
	settings.setValue("encoder_type", model->m_encoderType);
	settings.setValue("encoder_arch", model->m_encoderArch);
	settings.setValue("encoder_variant", model->m_encoderVariant);
	settings.setValue("rate_control_mode", model->m_rcMode);
	settings.setValue("target_bitrate", model->m_bitrate);
	settings.setValue("target_quantizer", model->m_quantizer);
	settings.setValue("preset_name", model->m_preset);
	settings.setValue("tuning_name", model->m_tune);
	settings.setValue("profile_name", model->m_profile);
	settings.setValue("custom_params_encoder", model->m_custom_encoder);
	settings.setValue("custom_params_avs2yuv", model->m_custom_avs2yuv);
	
	settings.endGroup();
	settings.sync();
	
	return true;
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

	//For backward-compatibility
	if(settings.contains("custom_params"))
	{
		settings.setValue("custom_params_x264", settings.value("custom_params"));
		settings.remove("custom_params"); settings.sync();
	}

	bool complete = true;
	if(!settings.contains("encoder_type")) complete = false;
	if(!settings.contains("encoder_arch")) complete = false;
	if(!settings.contains("encoder_variant")) complete = false;
	if(!settings.contains("rate_control_mode")) complete = false;
	if(!settings.contains("target_bitrate")) complete = false;
	if(!settings.contains("target_quantizer")) complete = false;
	if(!settings.contains("preset_name")) complete = false;
	if(!settings.contains("tuning_name")) complete = false;
	if(!settings.contains("profile_name")) complete = false;
	if(!settings.contains("custom_params_encoder")) complete = false;
	if(!settings.contains("custom_params_avs2yuv")) complete = false;

	if(complete)
	{
		model->setEncType(static_cast<OptionsModel::EncType>(settings.value("encoder_type", model->m_encoderType).toInt()));
		model->setEncArch(static_cast<OptionsModel::EncArch>(settings.value("encoder_arch", model->m_encoderArch).toInt()));
		model->setEncVariant(static_cast<OptionsModel::EncVariant>(settings.value("encoder_variant", model->m_encoderVariant).toInt()));
		model->setRCMode(static_cast<OptionsModel::RCMode>(settings.value("rate_control_mode", model->m_rcMode).toInt()));
		model->setBitrate(settings.value("target_bitrate", model->m_bitrate).toUInt());
		model->setQuantizer(settings.value("target_quantizer", model->m_quantizer).toDouble());
		model->setPreset(settings.value("preset_name", model->m_preset).toString());
		model->setTune(settings.value("tuning_name", model->m_tune).toString());
		model->setProfile(settings.value("profile_name", model->m_profile).toString());
		model->setCustomEncParams(settings.value("custom_params_x264", model->m_custom_encoder).toString());
		model->setCustomAvs2YUV(settings.value("custom_params_avs2yuv", model->m_custom_avs2yuv).toString());
	}

	settings.endGroup();
	return complete;
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
			X264_DELETE(options);
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
