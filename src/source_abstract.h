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

#include "tool_abstract.h"
#include "model_options.h"

class QRegExp;
template<class T> class QList;
class QProcess;

class AbstractSourceInfo
{
public:
	virtual QString getBinaryPath(const SysinfoModel *sysinfo, const bool& x64) const = 0;
};

class AbstractSource : public AbstractTool
{
public:
	AbstractSource(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile);
	virtual ~AbstractSource(void);

	virtual bool isSourceAvailable(void) = 0;
	virtual bool checkSourceProperties(unsigned int &frames);
	virtual bool createProcess(QProcess &processEncode, QProcess&processInput);
	virtual void flushProcess(QProcess &processInput) = 0;

	static const AbstractSourceInfo& getSourceInfo(void);

protected:
	virtual void checkSourceProperties_init(QList<QRegExp*> &patterns, QStringList &cmdLine) = 0;
	virtual void checkSourceProperties_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &frames, unsigned int &fSizeW, unsigned int &fSizeH, unsigned int &fpsNom, unsigned int &fpsDen) = 0;
	
	virtual void buildCommandLine(QStringList &cmdLine) = 0;

	const QString &m_sourceFile;
};
