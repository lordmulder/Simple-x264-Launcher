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

#include <QMutex>
#include <QMutexLocker>

///////////////////////////////////////////////////////////////////////////////

#define PREFERENCES_MAKE_X(NAME, PREFIX, TYPE) \
	public: \
		inline TYPE get##NAME(void) const \
		{ \
			QMutexLocker lock(&m_mutex); \
			const TYPE value = m_##PREFIX##NAME; \
			return value; \
		} \
		inline void set##NAME(const TYPE PREFIX##NAME) \
		{ \
			QMutexLocker lock(&m_mutex); \
			m_##PREFIX##NAME = PREFIX##NAME; \
		} \
	protected: \
		TYPE m_##PREFIX##NAME;

#define PREFERENCES_MAKE_B(NAME) PREFERENCES_MAKE_X(NAME, b, bool)
#define PREFERENCES_MAKE_I(NAME) PREFERENCES_MAKE_X(NAME, i, int)
#define PREFERENCES_MAKE_U(NAME) PREFERENCES_MAKE_X(NAME, u, unsigned int)

///////////////////////////////////////////////////////////////////////////////

class PreferencesModel
{
public:
	PreferencesModel(void);

	PREFERENCES_MAKE_B(AutoRunNextJob)
	PREFERENCES_MAKE_U(MaxRunningJobCount)
	PREFERENCES_MAKE_B(Prefer64BitSource)
	PREFERENCES_MAKE_B(SaveLogFiles)
	PREFERENCES_MAKE_B(SaveToSourcePath)
	PREFERENCES_MAKE_I(ProcessPriority)
	PREFERENCES_MAKE_B(EnableSounds)
	PREFERENCES_MAKE_B(DisableWarnings)
	PREFERENCES_MAKE_B(NoUpdateReminder)
	PREFERENCES_MAKE_B(AbortOnTimeout)
	PREFERENCES_MAKE_B(SkipVersionTest)
	PREFERENCES_MAKE_B(NoSystrayWarning)
	PREFERENCES_MAKE_B(SaveQueueNoConfirm)

public:
	static void initPreferences(PreferencesModel *preferences);
	static void loadPreferences(PreferencesModel *preferences);
	static void savePreferences(PreferencesModel *preferences);

protected:
	mutable QMutex m_mutex;
};

///////////////////////////////////////////////////////////////////////////////

#undef PREFERENCES_MAKE_X
#undef PREFERENCES_MAKE_B
#undef PREFERENCES_MAKE_I
#undef PREFERENCES_MAKE_U
