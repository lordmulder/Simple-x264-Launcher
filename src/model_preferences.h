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

#pragma once

class PreferencesModel
{
public:
	PreferencesModel(void);

	//Getter
	bool autoRunNextJob(void)             const { return m_autoRunNextJob;     }
	unsigned int maxRunningJobCount(void) const { return m_maxRunningJobCount; }
	bool shutdownComputer(void)           const { return m_shutdownComputer;   }
	bool useAvisyth64Bit(void)            const { return m_useAvisyth64Bit;    }
	bool saveLogFiles(void)               const { return m_saveLogFiles;       }
	bool saveToSourcePath(void)           const { return m_saveToSourcePath;   }
	int processPriority(void)             const { return m_processPriority;    }
	bool enableSounds(void)               const { return m_enableSounds;       }
	bool disableWarnings(void)            const { return m_disableWarnings;    }
	bool noUpdateReminder(void)           const { return m_noUpdateReminder;   }

	//Setter
	void setAutoRunNextJob(const bool autoRunNextJob)                 { m_autoRunNextJob     = autoRunNextJob;     }
	void setMaxRunningJobCount(const unsigned int maxRunningJobCount) { m_maxRunningJobCount = maxRunningJobCount; }
	void setShutdownComputer(const bool shutdownComputer)             { m_shutdownComputer   = shutdownComputer;   }
	void setUseAvisyth64Bit(const bool useAvisyth64Bit)               { m_useAvisyth64Bit    = useAvisyth64Bit;    }
	void setSaveLogFiles(const bool saveLogFiles)                     { m_saveLogFiles       = saveLogFiles;       }
	void setSaveToSourcePath(const bool saveToSourcePath)             { m_saveToSourcePath   = saveToSourcePath;   }
	void setProcessPriority(const int processPriority)                { m_processPriority    = processPriority;    }
	void setEnableSounds(const bool enableSounds)                     { m_enableSounds       = enableSounds;       }
	void setDisableWarnings(const bool disableWarnings)               { m_disableWarnings    = disableWarnings;    }
	void setNoUpdateReminder(const bool noUpdateReminder)             { m_noUpdateReminder   = noUpdateReminder;   }

	//Static
	static void initPreferences(PreferencesModel *preferences);
	static void loadPreferences(PreferencesModel *preferences);
	static void savePreferences(PreferencesModel *preferences);

protected:
	bool m_autoRunNextJob;
	unsigned int m_maxRunningJobCount;
	bool m_shutdownComputer;
	bool m_useAvisyth64Bit;
	bool m_saveLogFiles;
	bool m_saveToSourcePath;
	int m_processPriority;
	bool m_enableSounds;
	bool m_disableWarnings;
	bool m_noUpdateReminder;
};
