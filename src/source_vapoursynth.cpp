///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2016 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "source_vapoursynth.h"

#include "global.h"

#include <QDir>
#include <QProcess>
#include <QPair>

static const unsigned int VER_X264_VSPIPE_API =  3;
static const unsigned int VER_X264_VSPIPE_VER = 24;

// ------------------------------------------------------------
// Encoder Info
// ------------------------------------------------------------

class VapoursyntSourceInfo : public AbstractSourceInfo
{
public:
	virtual QString getBinaryPath(const SysinfoModel *const sysinfo, const bool& x64) const
	{
		return QString("%1/core%2/vspipe.exe").arg(sysinfo->getVPSPath(), (x64 ? "64" : "32"));
	}
};

static const VapoursyntSourceInfo s_vapoursynthEncoderInfo;

const AbstractSourceInfo &VapoursynthSource::getSourceInfo(void)
{
	return s_vapoursynthEncoderInfo;
}

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

VapoursynthSource::VapoursynthSource(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile)
:
	AbstractSource(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile)
{
	/*Nothing to do here*/
}

VapoursynthSource::~VapoursynthSource(void)
{
	/*Nothing to do here*/
}

QString VapoursynthSource::getName(void) const
{
	return tr("VapourSynth (vpy)");
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

bool VapoursynthSource::isSourceAvailable()
{
	if(!(m_sysinfo->hasVapourSynth() && (!m_sysinfo->getVPSPath().isEmpty()) && QFileInfo(getBinaryPath()).isFile()))
	{
		log(tr("\nVPY INPUT REQUIRES VAPOURSYNTH, BUT IT IS *NOT* AVAILABLE !!!"));
		return false;
	}
	return true;
}

void VapoursynthSource::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegExp("\\bVapourSynth\\b", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bCore\\s+r(\\d+)\\b", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bAPI\\s+r(\\d+)\\b", Qt::CaseInsensitive);
}

void VapoursynthSource::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified)
{
	int offset = -1;

	if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[1]->cap(1).toUInt(&ok);
		if(ok) build = temp;
	}
	else if((offset = patterns[2]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[2]->cap(1).toUInt(&ok);
		if(ok) core = temp;
	}

	if(!line.isEmpty())
	{
		log(line);
	}
}

QString VapoursynthSource::printVersion(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	return tr("\nVapourSynth version: r%1 (API r%2)").arg(QString::number(build), QString::number(core));
}

bool VapoursynthSource::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	if((build < VER_X264_VSPIPE_VER) || (core < VER_X264_VSPIPE_API))
	{
		
		if(core < VER_X264_VSPIPE_API)
		{
			log(tr("\nERROR: Your version of VapourSynth is unsupported! (requires API r%1 or newer)").arg(QString::number(VER_X264_VSPIPE_API)));
		}
		if(build < VER_X264_VSPIPE_VER)
		{
			log(tr("\nERROR: Your version of VapourSynth is unsupported! (requires version r%1 or newer)").arg(QString::number(VER_X264_VSPIPE_VER)));
		}
		log(tr("You can find the latest VapourSynth version at: http://www.vapoursynth.com/"));
		return false;
	}

	if(core != VER_X264_VSPIPE_API)
	{
		log(tr("\nWARNING: Running with an unknown VapourSynth API version, problem may appear! (this application works best with API r%1)").arg(QString::number(VER_X264_VSPIPE_API)));
	}

	return true;
}

// ------------------------------------------------------------
// Check Source Properties
// ------------------------------------------------------------

void VapoursynthSource::checkSourceProperties_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--info";
	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFile, true));
	cmdLine << "-";

	patterns << new QRegExp("\\bFrames:\\s+(\\d+)\\b");
	patterns << new QRegExp("\\bWidth:\\s+(\\d+)\\b");
	patterns << new QRegExp("\\bHeight:\\s+(\\d+)\\b");
	patterns << new QRegExp("\\bFPS:\\s+(\\d+)\\b");
	patterns << new QRegExp("\\bFPS:\\s+(\\d+)/(\\d+)\\b");
}

void VapoursynthSource::checkSourceProperties_parseLine(const QString &line, QList<QRegExp*> &patterns, ClipInfo &clipInfo)
{
	int offset = -1;

	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[0]->cap(1).toUInt(&ok);
		if(ok) clipInfo.setFrameCount(temp);
	}
	if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp =patterns[1]->cap(1).toUInt(&ok);
		if(ok) clipInfo.setFrameSize(temp, clipInfo.getFrameSize().second);
	}
	if((offset = patterns[2]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[2]->cap(1).toUInt(&ok);
		if(ok) clipInfo.setFrameSize(clipInfo.getFrameSize().first, temp);
	}
	if((offset = patterns[3]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[3]->cap(1).toUInt(&ok);
		if(ok) clipInfo.setFrameRate(temp, 0);
	}
	if((offset = patterns[4]->lastIndexIn(line)) >= 0)
	{
		bool ok1 = false, ok2 = false;
		unsigned int temp1 = patterns[4]->cap(1).toUInt(&ok1);
		unsigned int temp2 = patterns[4]->cap(2).toUInt(&ok2);
		if(ok1 && ok2)
		{
			clipInfo.setFrameRate(temp1, temp2);
		}
	}

	if(!line.isEmpty())
	{
		log(line);
	}
}

// ------------------------------------------------------------
// Check Source Properties
// ------------------------------------------------------------

void VapoursynthSource::buildCommandLine(QStringList &cmdLine)
{
	cmdLine << "--y4m";
	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFile, true));
	cmdLine << "-";
}

void VapoursynthSource::flushProcess(QProcess &processInput)
{
	while(processInput.bytesAvailable() > 0)
	{
		log(tr("vpyp [info]: %1").arg(QString::fromUtf8(processInput.readLine()).simplified()));
	}
	
	if(processInput.exitCode() != EXIT_SUCCESS)
	{
		const int exitCode = processInput.exitCode();
		log(tr("\nWARNING: Input process exited with error (code: %1), your encode might be *incomplete* !!!").arg(QString::number(exitCode)));
		if((exitCode < 0) || (exitCode >= 32))
		{
			log(tr("\nIMPORTANT: The Vapoursynth process terminated abnormally. This means Vapoursynth or one of your Vapoursynth-Plugin's just crashed."));
		}
	}
}
