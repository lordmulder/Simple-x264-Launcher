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
class AbstractSource;

class AbstractEncoderInfo
{
public:
	virtual QFlags<OptionsModel::EncVariant> getVariants(void) const = 0;
	virtual QStringList getProfiles(const int &variant) const = 0;
	virtual QStringList getTunings(void) const = 0;
	virtual QStringList supportedOutputFormats(void) const = 0;
	virtual bool isRCModeSupported(const int rcMode) const = 0;
	virtual bool isInputTypeSupported(const int format) const = 0;
	virtual QString getBinaryPath(const SysinfoModel *sysinfo, const OptionsModel::EncArch &encArch, const OptionsModel::EncVariant &encVariant) const = 0;
};

class AbstractEncoder : public AbstractTool
{
public:
	AbstractEncoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile);
	virtual ~AbstractEncoder(void);

	virtual bool runEncodingPass(AbstractSource* pipedSource, const QString outputFile, const unsigned int &frames, const int &pass = 0, const QString &passLogFile = QString());
	
	static const AbstractEncoderInfo& getEncoderInfo(void);

protected:
	virtual void buildCommandLine(QStringList &cmdLine, const bool &usePipe, const unsigned int &frames, const QString &indexFile, const int &pass, const QString &passLogFile) = 0;

	virtual void runEncodingPass_init(QList<QRegExp*> &patterns) = 0;
	virtual void runEncodingPass_parseLine(const QString &line, QList<QRegExp*> &patterns, const int &pass, double &last_progress, double &size_estimate) = 0;

	static QStringList splitParams(const QString &params, const QString &sourceFile, const QString &outputFile);
	static double estimateSize(const QString &fileName, const double &progress);
	static QString sizeToString(qint64 size);

	const QString &m_sourceFile;
	const QString &m_outputFile;
	const QString m_indexFile;
};
