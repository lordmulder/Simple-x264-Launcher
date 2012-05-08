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

#pragma once

#include <QObject>
#include <QString>
#include <QMap>

class OptionsModel
{
public:
	OptionsModel(void);
	~OptionsModel(void);

	enum RCMode
	{
		RCMode_CRF = 0,
		RCMode_CQ = 1,
		RCMode_2Pass = 2,
		RCMode_ABR = 3,
	};

	//Getter
	RCMode rcMode(void) const { return m_rcMode; }
	unsigned int bitrate(void) const { return m_bitrate; }
	double quantizer(void) const { return m_quantizer; }
	QString preset(void) const { return m_preset; }
	QString tune(void) const { return m_tune; }
	QString profile(void) const { return m_profile; }
	QString customX264(void) const { return m_custom_x264; }
	QString customAvs2YUV(void) const { return m_custom_avs2yuv; }

	//Setter
	void setRCMode(RCMode mode) { m_rcMode = qBound(RCMode_CRF, mode, RCMode_ABR); }
	void setBitrate(unsigned int bitrate) { m_bitrate = qBound(10U, bitrate, 800000U); }
	void setQuantizer(double quantizer) { m_quantizer = qBound(0.0, quantizer, 52.0); }
	void setPreset(const QString &preset) { m_preset = preset.trimmed(); }
	void setTune(const QString &tune) { m_tune = tune.trimmed(); }
	void setProfile(const QString &profile) { m_profile = profile.trimmed(); }
	void setCustomX264(const QString &custom) { m_custom_x264 = custom.trimmed(); }
	void setCustomAvs2YUV(const QString &custom) { m_custom_avs2yuv = custom.trimmed(); }

	//Stuff
	bool equals(OptionsModel *model);

	//Static functions
	static QString rcMode2String(RCMode mode);
	static bool saveTemplate(OptionsModel *model, const QString &name);
	static bool loadTemplate(OptionsModel *model, const QString &name);
	static QMap<QString, OptionsModel*> loadAllTemplates(void);
	static bool templateExists(const QString &name);
	static bool deleteTemplate(const QString &name);

protected:
	RCMode m_rcMode;
	unsigned int m_bitrate;
	double m_quantizer;
	QString m_preset;
	QString m_tune;
	QString m_profile;
	QString m_custom_x264;
	QString m_custom_avs2yuv;
};
