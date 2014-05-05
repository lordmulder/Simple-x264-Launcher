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

#include "source_abstract.h"

class AvisynthSource : public AbstractSource
{
public:
	AvisynthSource(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile);
	virtual ~AvisynthSource(void);
	
	virtual const QString &getName(void);

	virtual bool isSourceAvailable(void);
	virtual QString printVersion(const unsigned int &revision, const bool &modified);
	virtual bool isVersionSupported(const unsigned int &revision, const bool &modified);

	virtual void flushProcess(QProcess &processInput);

protected:
	virtual void checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine);
	virtual void checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified);
	virtual bool checkVersion_succeeded(const int &exitCode);

	virtual void checkSourceProperties_init(QList<QRegExp*> &patterns, QStringList &cmdLine);
	virtual void checkSourceProperties_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &frames, unsigned int &fSizeW, unsigned int &fSizeH, unsigned int &fpsNom, unsigned int &fpsDen);
	virtual const QString &getBinaryPath() { return m_binaryFile; }
	virtual void buildCommandLine(QStringList &cmdLine);

	const QString m_sourceName;
	const QString m_binaryFile;
};
