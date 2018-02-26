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

#pragma once

#include <QObject>
#include <QString>
#include <QMap>

class SysinfoModel;
class QSettings;

class OptionsModel
{
public:
	OptionsModel(const SysinfoModel *sysinfo);
	OptionsModel(const OptionsModel &rhs);
	~OptionsModel(void);

	enum EncType
	{
		EncType_X264  = 0,
		EncType_X265  = 1,
		EncType_NVEnc = 2,

		EncType_MIN  = EncType_X264,
		EncType_MAX  = EncType_NVEnc,
	};

	static const char *const SETTING_UNSPECIFIED;
	static const char *const PROFILE_UNRESTRICTED;

	//Getter
	EncType encType(void)         const { return m_encoderType; }
	quint32 encArch(void)         const { return m_encoderArch; }
	quint32 encVariant(void)      const { return m_encoderVariant; }
	quint32 rcMode(void)          const { return m_rcMode; }
	unsigned int bitrate(void)    const { return m_bitrate; }
	double quantizer(void)        const { return m_quantizer; }
	QString preset(void)          const { return m_preset; }
	QString tune(void)            const { return m_tune; }
	QString profile(void)         const { return m_profile; }
	QString customEncParams(void) const { return m_custom_encoder; }
	QString customAvs2YUV(void)   const { return m_custom_avs2yuv; }

	//Setter
	void setEncType(quint32 type)                  { setEncType(static_cast<EncType>(type)); }
	void setEncType(EncType type)                  { m_encoderType = qBound(EncType_MIN, type, EncType_MAX); }
	void setEncArch(quint32 arch)                  { m_encoderArch = arch; }
	void setEncVariant(quint32 variant)            { m_encoderVariant = variant; }
	void setRCMode(quint32 mode)                   { m_rcMode = mode; }
	void setBitrate(unsigned int bitrate)          { m_bitrate = qBound(10U, bitrate, 800000U); }
	void setQuantizer(double quantizer)            { m_quantizer = qBound(0.0, quantizer, 52.0); }
	void setPreset(const QString &preset)          { m_preset = preset.trimmed(); }
	void setTune(const QString &tune)              { m_tune = tune.trimmed(); }
	void setProfile(const QString &profile)        { m_profile = profile.trimmed(); }
	void setCustomEncParams(const QString &custom) { m_custom_encoder = custom.trimmed(); }
	void setCustomAvs2YUV(const QString &custom)   { m_custom_avs2yuv = custom.trimmed(); }

	//Stuff
	bool equals(const OptionsModel *model);

	//Static functions
	static bool saveTemplate(const OptionsModel *model, const QString &name);
	static bool loadTemplate(OptionsModel *model, const QString &name);
	static QMap<QString, OptionsModel*> loadAllTemplates(const SysinfoModel *sysinfo);
	static bool templateExists(const QString &name);
	static bool deleteTemplate(const QString &name);
	static bool saveOptions(const OptionsModel *model, QSettings &settingsFile);
	static bool loadOptions(OptionsModel *model, QSettings &settingsFile);

protected:
	EncType m_encoderType;
	quint32 m_encoderArch;
	quint32 m_encoderVariant;
	quint32 m_rcMode;
	unsigned int m_bitrate;
	double m_quantizer;
	QString m_preset;
	QString m_tune;
	QString m_profile;
	QString m_custom_encoder;
	QString m_custom_avs2yuv;

private:
	static void fixTemplate(QSettings &settingsFile);
};
