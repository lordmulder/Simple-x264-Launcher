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

#include "model_preferences.h"

#include "global.h"

#include <QSettings>
#include <QDesktopServices>
#include <QMouseEvent>
#include <QMessageBox>

///////////////////////////////////////////////////////////////////////////////

#define INIT_VALUE(NAME, VAL) do \
{ \
	preferences->set##NAME(VAL); \
} \
while(0)

#define STORE_VALUE(NAME) do \
{ \
	settings.setValue(#NAME, (preferences->get##NAME())); \
} \
while(0)

#define LOAD_VALUE_B(NAME) do \
{ \
	preferences->set##NAME(settings.value(#NAME, QVariant(defaults.get##NAME())).toBool()); \
} \
while(0)

#define LOAD_VALUE_I(NAME) do \
{ \
	preferences->set##NAME(settings.value(#NAME, QVariant(defaults.get##NAME())).toInt()); \
} \
while(0)

#define LOAD_VALUE_U(NAME) do \
{ \
	preferences->set##NAME(settings.value(#NAME, QVariant(defaults.get##NAME())).toUInt()); \
} \
while(0)

///////////////////////////////////////////////////////////////////////////////

PreferencesModel::PreferencesModel(void)
{
	initPreferences(this);
}

void PreferencesModel::initPreferences(PreferencesModel *preferences)
{
	INIT_VALUE(AutoRunNextJob,     true );
	INIT_VALUE(MaxRunningJobCount, 1    );
	INIT_VALUE(ShutdownComputer,   false);
	INIT_VALUE(UseAvisyth64Bit,    false);
	INIT_VALUE(SaveLogFiles,       false);
	INIT_VALUE(SaveToSourcePath,   false);
	INIT_VALUE(ProcessPriority,    -1   );
	INIT_VALUE(EnableSounds,       false);
	INIT_VALUE(DisableWarnings,    false);
	INIT_VALUE(NoUpdateReminder,   false);
	INIT_VALUE(AbortOnTimeout,     true );
	INIT_VALUE(SkipVersionTest,    false);

}

void PreferencesModel::loadPreferences(PreferencesModel *preferences)
{
	const QString appDir = x264_data_path();
	PreferencesModel defaults;
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);
	settings.beginGroup("preferences");

	LOAD_VALUE_B(AutoRunNextJob    );
	LOAD_VALUE_U(MaxRunningJobCount);
	LOAD_VALUE_B(ShutdownComputer  );
	LOAD_VALUE_B(UseAvisyth64Bit   );
	LOAD_VALUE_B(SaveLogFiles      );
	LOAD_VALUE_B(SaveToSourcePath  );
	LOAD_VALUE_I(ProcessPriority   );
	LOAD_VALUE_B(EnableSounds      );
	LOAD_VALUE_B(DisableWarnings   );
	LOAD_VALUE_B(NoUpdateReminder  );

	//Validation
	preferences->setProcessPriority(qBound(-2, preferences->getProcessPriority(), 2));
	preferences->setMaxRunningJobCount(qBound(1U, preferences->getMaxRunningJobCount(), 16U));
}

void PreferencesModel::savePreferences(PreferencesModel *preferences)
{
	const QString appDir = x264_data_path();
	QSettings settings(QString("%1/preferences.ini").arg(appDir), QSettings::IniFormat);
	settings.beginGroup("preferences");

	STORE_VALUE(AutoRunNextJob    );
	STORE_VALUE(MaxRunningJobCount);
	STORE_VALUE(ShutdownComputer  );
	STORE_VALUE(UseAvisyth64Bit   );
	STORE_VALUE(SaveLogFiles      );
	STORE_VALUE(SaveToSourcePath  );
	STORE_VALUE(ProcessPriority   );
	STORE_VALUE(EnableSounds      );
	STORE_VALUE(DisableWarnings   );
	STORE_VALUE(NoUpdateReminder  );
	
	settings.sync();
}
