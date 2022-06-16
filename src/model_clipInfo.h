///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2022 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include <qglobal.h>
template <class T1, class T2> struct QPair;

class ClipInfo
{
public:
	ClipInfo(const quint32 &count = 0, const quint32 &width = 0, const quint32 &height = 0, const quint32 &numerator = 0, const quint32 &denominator = 0);

	const quint32 &getFrameCount(void) const;
	QPair<quint32, quint32> getFrameSize(void) const;
	QPair<quint32, quint32> getFrameRate(void) const;

	void setFrameCount(const quint32 &count);
	void setFrameSize(const quint32 &width, const quint32 &height);
	void setFrameRate(const quint32 &numerator, const quint32 &denominator);

	void reset(void);

protected:
	quint32 m_frameCount;
	quint32 m_frameSize_wid;
	quint32 m_frameSize_hei;
	quint32 m_frameRate_num;
	quint32 m_frameRate_den;
};
