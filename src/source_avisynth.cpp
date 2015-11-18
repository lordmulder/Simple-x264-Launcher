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

#include "source_avisynth.h"

#include "global.h"

#include <QDir>
#include <QProcess>

static const unsigned int VER_X264_AVS2YUV_VER = 243;

// ------------------------------------------------------------
// Encoder Info
// ------------------------------------------------------------

class AvisynthSourceInfo : public AbstractSourceInfo
{
public:
	virtual QString getBinaryPath(const SysinfoModel *sysinfo, const bool& x64) const
	{
		return QString("%1/toolset/%2/avs2yuv_%2.exe").arg(sysinfo->getAppPath(), (x64 ? "x64": "x86"));
	}
};

static const AvisynthSourceInfo s_avisynthEncoderInfo;

const AbstractSourceInfo &AvisynthSource::getSourceInfo(void)
{
	return s_avisynthEncoderInfo;
}

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

AvisynthSource::AvisynthSource(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile)
:
	AbstractSource(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile)
{
	/*Nothing to do here*/
}

AvisynthSource::~AvisynthSource(void)
{
	/*Nothing to do here*/
}

QString AvisynthSource::getName(void) const
{
	return tr("Avisynth (avs)");
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

bool AvisynthSource::isSourceAvailable()
{
	if(!(m_sysinfo->hasAvisynth()))
	{
		log(tr("\nAVS INPUT REQUIRES AVISYNTH, BUT IT IS *NOT* AVAILABLE !!!"));
		return false;
	}
	return true;
}

void AvisynthSource::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	patterns << new QRegExp("\\bAvs2YUV (\\d+).(\\d+)\\b", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bAvs2YUV (\\d+).(\\d+)bm(\\d)\\b", Qt::CaseInsensitive);
}

void AvisynthSource::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified)
{
	int offset = -1;

	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		unsigned int temp1 = patterns[0]->cap(1).toUInt(&ok1);
		unsigned int temp2 = patterns[0]->cap(2).toUInt(&ok2);
		if(ok1 && ok2)
		{
			core  = temp1;
			build = temp2;
		}
		log(line);
	}
	else if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false, ok3 = false;
		unsigned int temp1 = patterns[1]->cap(1).toUInt(&ok1);
		unsigned int temp2 = patterns[1]->cap(2).toUInt(&ok2);
		unsigned int temp3 = patterns[1]->cap(3).toUInt(&ok3);
		if(ok1 && ok2 && ok3)
		{
			core  = temp1;
			build = (temp2 * 10) + (temp3 % 10);
		}
		modified = true;
		log(line);
	}
}

bool AvisynthSource::checkVersion_succeeded(const int &exitCode)
{
	return (exitCode == EXIT_SUCCESS) || (exitCode == 2);
}

QString AvisynthSource::printVersion(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	return tr("Avs2YUV version: %1.%2.%3").arg(QString::number(core), QString::number(build / 10),QString::number(build % 10));
}

bool AvisynthSource::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	if((revision != UINT_MAX) && (build < VER_X264_AVS2YUV_VER))
	{
		log(tr("\nERROR: Your version of avs2yuv is unsupported (required version: v0.24 BugMaster's mod 2)"));
		log(tr("You can find the required version at: http://komisar.gin.by/tools/avs2yuv/"));
		return false;
	}
	return true;
}

// ------------------------------------------------------------
// Check Source Properties
// ------------------------------------------------------------

void AvisynthSource::checkSourceProperties_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	if(!m_options->customAvs2YUV().isEmpty())
	{
		cmdLine << splitParams(m_options->customAvs2YUV());
	}

	cmdLine << "-frames" << "1";
	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFile, true)) << "NUL";

	patterns << new QRegExp(": (\\d+)x(\\d+), (\\d+) fps, (\\d+) frames");
	patterns << new QRegExp(": (\\d+)x(\\d+), (\\d+)/(\\d+) fps, (\\d+) frames");
}

void AvisynthSource::checkSourceProperties_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &frames, unsigned int &fSizeW, unsigned int &fSizeH, unsigned int &fpsNom, unsigned int &fpsDen)
{
	int offset = -1;

	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		bool ok3 = false, ok4 = false;
		unsigned int temp1 = patterns[0]->cap(1).toUInt(&ok1);
		unsigned int temp2 = patterns[0]->cap(2).toUInt(&ok2);
		unsigned int temp3 = patterns[0]->cap(3).toUInt(&ok3);
		unsigned int temp4 = patterns[0]->cap(4).toUInt(&ok4);
		if(ok1) fSizeW = temp1;
		if(ok2) fSizeH = temp2;
		if(ok3) fpsNom = temp3;
		if(ok4) frames = temp4;
	}
	else if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		bool ok3 = false, ok4 = false, ok5 = false;
		unsigned int temp1 = patterns[1]->cap(1).toUInt(&ok1);
		unsigned int temp2 = patterns[1]->cap(2).toUInt(&ok2);
		unsigned int temp3 = patterns[1]->cap(3).toUInt(&ok3);
		unsigned int temp4 = patterns[1]->cap(4).toUInt(&ok4);
		unsigned int temp5 = patterns[1]->cap(5).toUInt(&ok5);
		if(ok1) fSizeW = temp1;
		if(ok2) fSizeH = temp2;
		if(ok3) fpsNom = temp3;
		if(ok4) fpsDen = temp4;
		if(ok5) frames = temp5;
	}

	if(!line.isEmpty())
	{
		log(line);
	}

	if(line.contains("failed to load avisynth.dll", Qt::CaseInsensitive))
	{
		log(tr("\nWarning: It seems that Avisynth is not currently installed/available !!!"));
	}
	if(line.contains(QRegExp("couldn't convert input clip to (YV16|YV24)", Qt::CaseInsensitive)))
	{
		log(tr("\nWarning: YV16 (4:2:2) and YV24 (4:4:4) color-spaces only supported in Avisynth 2.6 !!!"));
	}
}

// ------------------------------------------------------------
// Source Processing
// ------------------------------------------------------------

void AvisynthSource::buildCommandLine(QStringList &cmdLine)
{
	if(!m_options->customAvs2YUV().isEmpty())
	{
		cmdLine << splitParams(m_options->customAvs2YUV());
	}

	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFile, true));
	cmdLine << "-";
}

void AvisynthSource::flushProcess(QProcess &processInput)
{
	while(processInput.bytesAvailable() > 0)
	{
		log(tr("av2y [info]: %1").arg(QString::fromUtf8(processInput.readLine()).simplified()));
	}
	
	if(processInput.exitCode() != EXIT_SUCCESS)
	{
		const int exitCode = processInput.exitCode();
		log(tr("\nWARNING: Input process exited with error (code: %1), your encode might be *incomplete* !!!").arg(QString::number(exitCode)));
		if((exitCode < 0) || (exitCode >= 32))
		{
			log(tr("\nIMPORTANT: The Avs2YUV process terminated abnormally. This means Avisynth or one of your Avisynth-Plugin's just crashed."));
			log(tr("IMPORTANT: Please fix your Avisynth script and try again! If you use Avisynth-MT, try using a *stable* Avisynth instead!"));
		}
	}
}
