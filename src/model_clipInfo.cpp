///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2023 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "model_clipInfo.h"

#include <QPair>

ClipInfo::ClipInfo(const quint32 &count, const quint32 &width, const quint32 &height, const quint32 &numerator, const quint32 &denominator)
{
	m_frameCount    = count;
	m_frameSize_wid = width;
	m_frameSize_hei = height;
	m_frameRate_num = numerator;
	m_frameRate_den = denominator;
}

void ClipInfo::reset(void)
{
	m_frameCount = m_frameSize_wid = m_frameSize_hei = m_frameRate_num = m_frameRate_den = 0;
}

const quint32 &ClipInfo::getFrameCount(void) const
{
	return m_frameCount;
}

QPair<quint32, quint32> ClipInfo::getFrameSize(void) const
{
	return qMakePair(m_frameSize_wid, m_frameSize_hei);
}

QPair<quint32, quint32> ClipInfo::getFrameRate(void) const
{
	return qMakePair(m_frameRate_num, m_frameRate_den);
}

void ClipInfo::setFrameCount(const quint32 &count)
{
	m_frameCount = count;
}

void ClipInfo::setFrameSize(const quint32 &width, const quint32 &height)
{
	m_frameSize_wid = width;
	m_frameSize_hei = height;
}

void ClipInfo::setFrameRate(const quint32 &numerator, const quint32 &denominator)
{
	m_frameRate_num = numerator;
	m_frameRate_den = denominator;
}
