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

#include "encoder_nvenc.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_status.h"
#include "mediainfo.h"
#include "model_sysinfo.h"

//MUtils
#include <MUtils/Exception.h>

//Qt
#include <QStringList>
#include <QDir>
#include <QRegExp>

//x265 version info
static const unsigned int VERSION_NVENCC_MINIMUM_VER = 206;
static const unsigned int VERSION_NVENCC_MINIMUM_API =  60;

// ------------------------------------------------------------
// Helper Macros
// ------------------------------------------------------------

#define NVENCC_UPDATE_PROGRESS(X) do \
{ \
	bool ok = false; \
	unsigned int progressFrames = (X)->cap(1).toUInt(&ok); \
	setStatus(JobStatus_Running); \
	if(ok && (totalFrames > 0) && (totalFrames != UINT_MAX)) \
	{ \
		const double progress = (double(progressFrames) / double(totalFrames)); \
		if(!qFuzzyCompare(progress, last_progress)) \
		{ \
			setProgress(floor(progress * 100.0)); \
			size_estimate = qFuzzyIsNull(size_estimate) ? estimateSize(m_outputFile, progress) : ((0.667 * size_estimate) + (0.333 * estimateSize(m_outputFile, progress))); \
			last_progress = progress; \
		} \
	} \
	setDetails(tr("%1, est. file size %2").arg(line.mid(offset).trimmed(), sizeToString(qRound64(size_estimate)))); \
} \
while(0)

#define REMOVE_CUSTOM_ARG(LIST, ITER, FLAG, PARAM) do \
{ \
	if(ITER != LIST.end()) \
	{ \
		if((*ITER).compare(PARAM, Qt::CaseInsensitive) == 0) \
		{ \
			log(tr("WARNING: Custom parameter \"" PARAM "\" will be ignored in Pipe'd mode!\n")); \
			ITER = LIST.erase(ITER); \
			if(ITER != LIST.end()) \
			{ \
				if(!((*ITER).startsWith("--", Qt::CaseInsensitive))) ITER = LIST.erase(ITER); \
			} \
			FLAG = true; \
		} \
	} \
} \
while(0)

// ------------------------------------------------------------
// Encoder Info
// ------------------------------------------------------------

class NVEncEncoderInfo : public AbstractEncoderInfo
{
public:
	virtual QFlags<OptionsModel::EncVariant> getVariants(void) const
	{
		QFlags<OptionsModel::EncVariant> variants;
		variants |= OptionsModel::EncVariant_8Bit;
		return variants;
	}

	virtual QStringList getTunings(void) const
	{
		return QStringList();
	}

	virtual QStringList getPresets(void) const
	{
		return QStringList();
	}

	virtual QStringList getProfiles(const OptionsModel::EncVariant &variant) const
	{
		QStringList profiles;
		switch(variant)
		{
		case OptionsModel::EncVariant_8Bit:
			profiles << "baseline" << "main" << "high";
			break;
		}
		return profiles;
	}

	virtual QStringList supportedOutputFormats(void) const
	{
		QStringList extLst;
		extLst << "mp4";
		return extLst;
	}

	virtual bool isRCModeSupported(const OptionsModel::RCMode &rcMode) const
	{
		switch(rcMode)
		{
		case OptionsModel::RCMode_CQ:
		case OptionsModel::RCMode_ABR:
			return true;
		default:
			return false;
		}
	}

	virtual bool isInputTypeSupported(const int format) const
	{
		switch(format)
		{
		case MediaInfo::FILETYPE_YUV4MPEG2:
			return true;
		default:
			return false;
		}
	}

	virtual QString getBinaryPath(const SysinfoModel *sysinfo, const OptionsModel::EncArch &encArch, const OptionsModel::EncVariant &encVariant) const
	{
		QString arch, variant;
		switch(encArch)
		{
			case OptionsModel::EncArch_x86_32: arch = "x86"; break;
			case OptionsModel::EncArch_x86_64: arch = "x64"; break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		switch(encVariant)
		{
			case OptionsModel::EncVariant_8Bit:  variant = "8bit";  break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		return QString("%1/toolset/%2/nvencc_%2.exe").arg(sysinfo->getAppPath(), arch);
	}

	virtual QStringList getDependencies(const SysinfoModel *sysinfo, const OptionsModel::EncArch &encArch, const OptionsModel::EncVariant &encVariant) const
	{
		QString arch, variant;
		switch (encArch)
		{
			case OptionsModel::EncArch_x86_32: arch = "x86"; break;
			case OptionsModel::EncArch_x86_64: arch = "x64"; break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		switch (encVariant)
		{
			case OptionsModel::EncVariant_8Bit:  variant = "8bit";  break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		QStringList dependencies;
		dependencies << QString("%1/toolset/%2/avcodec-57.dll"  ).arg(sysinfo->getAppPath(), arch);
		dependencies << QString("%1/toolset/%2/avfilter-6.dll"  ).arg(sysinfo->getAppPath(), arch);
		dependencies << QString("%1/toolset/%2/avformat-57.dll" ).arg(sysinfo->getAppPath(), arch);
		dependencies << QString("%1/toolset/%2/avutil-55.dll"   ).arg(sysinfo->getAppPath(), arch);
		dependencies << QString("%1/toolset/%2/swresample-2.dll").arg(sysinfo->getAppPath(), arch);
		return dependencies;
	}
};

static const NVEncEncoderInfo s_x265EncoderInfo;

const AbstractEncoderInfo &NVEncEncoder::getEncoderInfo(void)
{
	return s_x265EncoderInfo;
}

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

NVEncEncoder::NVEncEncoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile)
:
	AbstractEncoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile)
{
	if(options->encType() != OptionsModel::EncType_NVEnc)
	{
		MUTILS_THROW("Invalid encoder type!");
	}
}

NVEncEncoder::~NVEncEncoder(void)
{
	/*Nothing to do here*/
}

QString NVEncEncoder::getName(void) const
{
	QString arch, variant;
	switch(m_options->encArch())
	{
		case OptionsModel::EncArch_x86_32: arch = "x86"; break;
		case OptionsModel::EncArch_x86_64: arch = "x64"; break;
		default: MUTILS_THROW("Unknown encoder arch!");
	}
	switch(m_options->encVariant())
	{
		case OptionsModel::EncVariant_8Bit:  variant = "8-Bit";  break;
		default: MUTILS_THROW("Unknown encoder arch!");
	}
	return QString("NVEncC, %1, %2").arg(arch, variant);
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

void NVEncEncoder::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegExp("\\bNVEncC\\s+\\(\\w+\\)\\s+(\\d)\\.(\\d+)\\s+by\\s+rigaya\\s+\\[NVENC\\s+API\\s+v(\\d+)\\.(\\d+)\\]", Qt::CaseInsensitive);
}

void NVEncEncoder::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified)
{
	int offset = -1;

	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok[4] = { false, false, false, false };
		unsigned int temp[4];
		temp[0] = patterns[0]->cap(1).toUInt(&ok[0]);
		temp[1] = patterns[0]->cap(2).toUInt(&ok[1]);
		temp[2] = patterns[0]->cap(2).toUInt(&ok[2]);
		temp[3] = patterns[0]->cap(2).toUInt(&ok[3]);
		if(ok[0] && ok[1])
		{
			core = (100 * temp[0]) + temp[1];
		}
		if (ok[2] && ok[3])
		{
			build = (10 * temp[2]) + temp[3];
		}
	}

	if(!line.isEmpty())
	{
		log(line);
	}
}

bool NVEncEncoder::checkVersion_succeeded(const int &exitCode)
{
	return (exitCode == 0) || (exitCode == 1);
}

QString NVEncEncoder::printVersion(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	return tr("NVEncC version: %1.%2").arg(QString::number(core / 100), QString::number(core % 100).leftJustified(2, QLatin1Char('0')));
}

bool NVEncEncoder::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	if(core < VERSION_NVENCC_MINIMUM_VER)
	{
		log(tr("\nERROR: Your version of NVEncC is too old! (Minimum required version is %1.%2)").arg(QString::number(VERSION_NVENCC_MINIMUM_VER / 100), QString::number(VERSION_NVENCC_MINIMUM_VER % 100)));
		return false;
	}
	else if(core > VERSION_NVENCC_MINIMUM_VER)
	{
		log(tr("\nWARNING: Your version of NVEncC is newer than the latest tested version, take care!"));
		log(tr("This application works best with NVEncC version %1.%2. Newer versions may work or not.").arg(QString::number(VERSION_NVENCC_MINIMUM_VER / 100), QString::number(VERSION_NVENCC_MINIMUM_VER % 100)));
	}

	if (build < VERSION_NVENCC_MINIMUM_API)
	{
		log(tr("\nERROR: Your version of NVENC API is too old! (Minimum required version is %1.%2)").arg(QString::number(VERSION_NVENCC_MINIMUM_API / 10), QString::number(VERSION_NVENCC_MINIMUM_API % 10)));
		return false;
	}

	return true;
}

// ------------------------------------------------------------
// Encoding Functions
// ------------------------------------------------------------

void NVEncEncoder::buildCommandLine(QStringList &cmdLine, const bool &usePipe, const unsigned int &frames, const QString &indexFile, const int &pass, const QString &passLogFile)
{
	double crf_int = 0.0, crf_frc = 0.0;

	switch(m_options->rcMode())
	{
	case OptionsModel::RCMode_ABR:
		cmdLine << "--vbr" << QString::number(m_options->bitrate());
		break;
	case OptionsModel::RCMode_CQ:
		cmdLine << "--cqp" << QString::number(qRound(m_options->quantizer()));
		break;
	default:
		MUTILS_THROW("Bad rate-control mode !!!");
		break;
	}
	
	const QString profile = m_options->profile().simplified().toLower();
	if(!profile.isEmpty())
	{
		if(profile.compare(QString::fromLatin1(OptionsModel::PROFILE_UNRESTRICTED), Qt::CaseInsensitive) != 0)
		{
			cmdLine << "--profile" << profile;
		}
	}

	if(!m_options->customEncParams().isEmpty())
	{
		QStringList customArgs = splitParams(m_options->customEncParams(), m_sourceFile, m_outputFile);
		if(usePipe)
		{
			QStringList::iterator i = customArgs.begin();
			while(i != customArgs.end())
			{
				bool bModified = false;
				REMOVE_CUSTOM_ARG(customArgs, i, bModified, "--fps");
				REMOVE_CUSTOM_ARG(customArgs, i, bModified, "--frames");
				if(!bModified) i++;
			}
		}
		cmdLine.append(customArgs);
	}

	cmdLine << "--output" << QDir::toNativeSeparators(m_outputFile);
	
	if(usePipe)
	{
		cmdLine << "--y4m" << "--input" << "-";
	}
	else
	{
		cmdLine << "--input" << QDir::toNativeSeparators(m_sourceFile);
	}
}

void NVEncEncoder::runEncodingPass_init(QList<QRegExp*> &patterns)
{
	patterns << new QRegExp("^(\\d+) frames:"); //regExpFrameCnt
}

void NVEncEncoder::runEncodingPass_parseLine(const QString &line, QList<QRegExp*> &patterns, const unsigned int &totalFrames, const int &pass, double &last_progress, double &size_estimate)
{
	int offset = -1;
	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		NVENCC_UPDATE_PROGRESS(patterns[0]);
	}
	else if(!line.isEmpty())
	{
		log(line);
	}
}
