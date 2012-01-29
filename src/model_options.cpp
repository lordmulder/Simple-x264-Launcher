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

#include "model_options.h"

OptionsModel::OptionsModel(void)
{
	m_rcMode = RCMode_CRF;
	m_bitrate = 1200;
	m_quantizer = 22;
	m_preset = "Medium";
	m_tune = "None";
	m_profile = "High";
	m_custom = "";
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
