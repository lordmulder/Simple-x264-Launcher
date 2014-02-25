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

#include "source_vapoursynth.h"

#include "global.h"
#include "model_sysinfo.h"
#include "model_preferences.h"
#include "binaries.h"

#include <QDir>
#include <QProcess>

static const unsigned int VER_X264_VSPIPE_VER = 22;

VapoursynthSource::VapoursynthSource(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile)
:
	AbstractSource(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile),
	m_binaryFile(VPS_BINARY(m_sysinfo, m_preferences))
{
	/*Nothing to do here*/
}

VapoursynthSource::~VapoursynthSource(void)
{
	/*Nothing to do here*/
}

bool VapoursynthSource::isSourceAvailable()
{
	if(!(m_sysinfo->hasVPSSupport() && (!m_sysinfo->getVPSPath().isEmpty()) && QFileInfo(m_sysinfo->getVPSPath()).isFile()))
	{
		log(tr("\nVPY INPUT REQUIRES VAPOURSYNTH, BUT IT IS *NOT* AVAILABLE !!!"));
		return false;
	}
	return true;
}

void VapoursynthSource::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "-version";
	patterns << new QRegExp("\\bVapourSynth\\b", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bCore\\s+r(\\d+)\\b", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bAPI\\s+r(\\d+)\\b", Qt::CaseInsensitive);
}

void VapoursynthSource::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &coreVers, unsigned int &revision, bool &modified)
{
	int offset = -1;
	if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[1]->cap(1).toUInt(&ok);
		if(ok) revision = temp;
	}
	else if((offset = patterns[2]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[2]->cap(1).toUInt(&ok);
		if(ok) coreVers = temp;
	}
}

void VapoursynthSource::printVersion(const unsigned int &revision, const bool &modified)
{
	log(tr("VapourSynth version: r%1 (API r%2)").arg(QString::number(revision % REV_MULT), QString::number(revision / REV_MULT)));
}

bool VapoursynthSource::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	if((revision % REV_MULT) < VER_X264_VSPIPE_VER)
	{
		log(tr("\nERROR: Your version of VapourSynth is unsupported (requires version r%1 or newer").arg(QString::number(VER_X264_VSPIPE_VER)));
		log(tr("You can find the latest VapourSynth version at: http://www.vapoursynth.com/"));
		return false;
	}
	return true;
}

void VapoursynthSource::checkSourceProperties_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFile, true));
	cmdLine << "-" << "-info";

	patterns << new QRegExp("\\bFrames:\\s+(\\d+)\\b");
	patterns << new QRegExp("\\bWidth:\\s+(\\d+)\\b");
	patterns << new QRegExp("\\bHeight:\\s+(\\d+)\\b");
}

void VapoursynthSource::checkSourceProperties_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &frames, unsigned int &fSizeW, unsigned int &fSizeH, unsigned int &fpsNom, unsigned int &fpsDen)
{
	int offset = -1;
	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[0]->cap(1).toUInt(&ok);
		if(ok) frames = temp;
	}
	if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp =patterns[1]->cap(1).toUInt(&ok);
		if(ok) fSizeW = temp;
	}
	if((offset = patterns[2]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int temp = patterns[2]->cap(1).toUInt(&ok);
		if(ok) fSizeH = temp;
	}
	if(!line.isEmpty())
	{
		log(line);
	}
}

void VapoursynthSource::buildCommandLine(QStringList &cmdLine)
{
	cmdLine << QDir::toNativeSeparators(x264_path2ansi(m_sourceFile, true));
	cmdLine << "-" << "-y4m";
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
