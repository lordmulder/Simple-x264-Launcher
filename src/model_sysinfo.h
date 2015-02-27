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

#pragma once

#include <QMutex>
#include <QMutexLocker>
#include <QString>

///////////////////////////////////////////////////////////////////////////////

#define SYSINFO_MAKE_FLAG(NAME, VALUE) \
	protected: \
		static const unsigned int FLAG_HAS_##NAME = VALUE; \
	public: \
		inline void set##NAME##Support(const bool &enable) \
		{ \
			QMutexLocker lock(&m_mutex); \
			if(enable) setFlag(FLAG_HAS_##NAME); else clrFlag(FLAG_HAS_##NAME); \
		} \
		inline bool has##NAME##Support(void) const \
		{ \
			QMutexLocker lock(&m_mutex); \
			const bool enabeld = ((m_flags & (FLAG_HAS_##NAME)) == FLAG_HAS_##NAME); \
			return enabeld; \
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
		}

///////////////////////////////////////////////////////////////////////////////

class SysinfoModel
{
public:
	SysinfoModel(void) { m_flags = 0; }

	SYSINFO_MAKE_FLAG(X64,   0x00000001)
	SYSINFO_MAKE_FLAG(MMX,   0x00000002)
	SYSINFO_MAKE_FLAG(SSE,   0x00000004)
	SYSINFO_MAKE_FLAG(AVS,   0x00000008)
	SYSINFO_MAKE_FLAG(VPS32, 0x00000010)
	SYSINFO_MAKE_FLAG(VPS64, 0x00000020)
	
	SYSINFO_MAKE_PATH(VPS)
	SYSINFO_MAKE_PATH(App)

protected:
	inline void setFlag(const unsigned int &flag) { m_flags = (m_flags | flag);    }
	inline void clrFlag(const unsigned int &flag) { m_flags = (m_flags & (~flag)); }

	unsigned int m_flags;
	mutable QMutex m_mutex;
};

#undef SYSINFO_MAKE_FLAG
#undef SYSINFO_MAKE_PATH
