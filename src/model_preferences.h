///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2013 LoRd_MuldeR <MuldeR2@GMX.de>
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

class PreferencesModel
{
public:
	PreferencesModel(void);

	enum
	{
		X264_PRIORITY_ABOVENORMAL = 0,
		X264_PRIORITY_NORMAL = 1,
		X264_PRIORITY_BELOWNORMAL = 2,
		X264_PRIORITY_IDLE = 3,
	}
	x264_priority_t;

	//Getter
	bool autoRunNextJob(void) { return m_autoRunNextJob; }
	unsigned int maxRunningJobCount(void) { return m_maxRunningJobCount; }
	bool shutdownComputer(void) { return m_shutdownComputer; }
	bool use10BitEncoding(void) { return m_use10BitEncoding; }
	bool useAvisyth64Bit(void) { return m_useAvisyth64Bit; }
	bool saveLogFiles(void) { return m_saveLogFiles; }
	bool saveToSourcePath(void) { return m_saveToSourcePath; }
	int processPriority(void) { return m_processPriority; }
	bool enableSounds(void) { return m_enableSounds; }
	bool disableWarnings(void) { return m_disableWarnings; }

	//Setter
	void setAutoRunNextJob(const bool autoRunNextJob) { m_autoRunNextJob = autoRunNextJob; }
	void setMaxRunningJobCount(const unsigned int maxRunningJobCount) { m_maxRunningJobCount = maxRunningJobCount; }
	void setShutdownComputer(const bool shutdownComputer) { m_shutdownComputer = shutdownComputer; }
	void setUse10BitEncoding(const bool use10BitEncoding) { m_use10BitEncoding = use10BitEncoding; }
	void setUseAvisyth64Bit(const bool useAvisyth64Bit) { m_useAvisyth64Bit = useAvisyth64Bit; }
	void setSaveLogFiles(const bool saveLogFiles) { m_saveLogFiles = saveLogFiles; }
	void setSaveToSourcePath(const bool saveToSourcePath) { m_saveToSourcePath = saveToSourcePath; }
	void setProcessPriority(const int processPriority) { m_processPriority = processPriority; }
	void setEnableSounds(const bool enableSounds) { m_enableSounds = enableSounds; }
	void setDisableWarnings(const bool disableWarnings) { m_disableWarnings = disableWarnings; }

	//Static
	static void initPreferences(PreferencesModel *preferences);
	static void loadPreferences(PreferencesModel *preferences);
	static void savePreferences(PreferencesModel *preferences);

protected:
	bool m_autoRunNextJob;
	unsigned int m_maxRunningJobCount;
	bool m_shutdownComputer;
	bool m_use10BitEncoding;
	bool m_useAvisyth64Bit;
	bool m_saveLogFiles;
	bool m_saveToSourcePath;
	int m_processPriority;
	bool m_enableSounds;
	bool m_disableWarnings;
};
