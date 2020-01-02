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

#include "encoder_abstract.h"

class X265Encoder : public AbstractEncoder
{
public:
	X265Encoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile);
	virtual ~X265Encoder(void);

	virtual QString getName(void) const;

	virtual QString printVersion(const unsigned int &revision, const bool &modified);
	virtual bool isVersionSupported(const unsigned int &revision, const bool &modified);

	virtual const AbstractEncoderInfo& getEncoderInfo(void) const;
	static const AbstractEncoderInfo& encoderInfo(void);

protected:
	virtual QString getBinaryPath() const { return getEncoderInfo().getBinaryPath(m_sysinfo, m_options->encArch(), m_options->encVariant()); }
	virtual void buildCommandLine(QStringList &cmdLine, const bool &usePipe, const ClipInfo &clipInfo, const QString &indexFile, const int &pass, const QString &passLogFile);

	virtual void checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine);
	virtual void checkVersion_parseLine(const QString &line, const QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified);

	virtual void runEncodingPass_init(QList<QRegExp*> &patterns);
	virtual void runEncodingPass_parseLine(const QString &line, const QList<QRegExp*> &patterns, const ClipInfo &clipInfo, const int &pass, double &last_progress, double &size_estimate);
};
