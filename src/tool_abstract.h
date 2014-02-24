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

#include <QObject>
#include <QUuid>
#include <QMutex>

class OptionsModel;
class SysinfoModel;
class PreferencesModel;
class JobObject;
class QProcess;

class AbstractTool : QObject
{
	Q_OBJECT

public:
	AbstractTool(const QUuid *jobId, JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, volatile bool *abort);
	virtual ~AbstractTool(void) {/*NOP*/}
	
signals:
	void messageLogged(const QUuid &jobId, const QString &text);

protected:
	inline void log(const QString &text) { emit messageLogged((*m_jobId), text); }
	bool startProcess(QProcess &process, const QString &program, const QStringList &args, bool mergeChannels = true);
	static QString commandline2string(const QString &program, const QStringList &arguments);

	const QUuid *const m_jobId;
	JobObject *const m_jobObject;
	const OptionsModel *const m_options;
	const SysinfoModel *const m_sysinfo;
	const PreferencesModel *const m_preferences;
	volatile bool *const m_abort;

	static QMutex s_mutexStartProcess;
};
