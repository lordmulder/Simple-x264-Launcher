///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2020 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QFlags>

///////////////////////////////////////////////////////////////////////////////

#define SYSINFO_MAKE_FLAG(NAME) \
	protected: \
		QFlags<NAME##_t> m_flag##NAME; \
	public: \
		inline void set##NAME(const NAME##_t &flag, const bool &enable) \
		{ \
			QMutexLocker lock(&m_mutex); \
			if(enable) m_flag##NAME |= flag; else m_flag##NAME &= (~flag); \
		} \
		inline bool get##NAME(const NAME##_t &flag) const \
		{ \
			QMutexLocker lock(&m_mutex); \
			return m_flag##NAME.testFlag(flag); \
		} \
		inline bool has##NAME(void) const \
		{ \
			QMutexLocker lock(&m_mutex); \
			return !!m_flag##NAME; \
		} \
		inline void clear##NAME(void) \
		{ \
			QMutexLocker lock(&m_mutex); \
			m_flag##NAME &= 0; \
		}

#define SYSINFO_MAKE_PATH(NAME) \
	protected: \
		QString m_path##NAME; \
	public: \
		inline void set##NAME##Path(const QString &path) \
		{ \
			QMutexLocker lock(&m_mutex); \
			m_path##NAME = path; \
		} \
		inline const QString get##NAME##Path(void) const \
		{ \
			QMutexLocker lock(&m_mutex); \
			const QString path = m_path##NAME; \
			return path; \
		} \
		inline void clear##NAME##Path(void) \
		{ \
			QMutexLocker lock(&m_mutex); \
			m_path##NAME.clear(); \
		}

///////////////////////////////////////////////////////////////////////////////

class SysinfoModel
{
public:
	SysinfoModel(void) {}
	
	typedef enum _CPUFeatures_t
	{
		CPUFeatures_MMX = 0x1,
		CPUFeatures_SSE = 0x2,
		CPUFeatures_X64 = 0x4,
	}
	CPUFeatures_t;

	typedef enum _Avisynth_t
	{
		Avisynth_X86 = 0x1,
		Avisynth_X64 = 0x2,
	}
	Avisynth_t;

	typedef enum _VapourSynth_t
	{
		VapourSynth_X86 = 0x1,
		VapourSynth_X64 = 0x2,
	}
	VapourSynth_t;

	SYSINFO_MAKE_FLAG(CPUFeatures)
	SYSINFO_MAKE_FLAG(Avisynth)
	SYSINFO_MAKE_FLAG(VapourSynth)

	SYSINFO_MAKE_PATH(AVS)
	SYSINFO_MAKE_PATH(VPS32)
	SYSINFO_MAKE_PATH(VPS64)
	SYSINFO_MAKE_PATH(App)

protected:
	mutable QMutex m_mutex;
};

#undef SYSINFO_MAKE_FLAG
#undef SYSINFO_MAKE_PATH
