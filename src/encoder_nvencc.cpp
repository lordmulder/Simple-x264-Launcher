///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2019 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "encoder_nvencc.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "model_status.h"
#include "mediainfo.h"
#include "model_sysinfo.h"
#include "model_clipInfo.h"

//MUtils
#include <MUtils/Global.h>
#include <MUtils/Exception.h>

//Qt
#include <QStringList>
#include <QDir>
#include <QRegExp>
#include <QPair>

//x265 version info
static const unsigned int VERSION_NVENCC_MINIMUM_VER = 449;

// ------------------------------------------------------------
// Helper Macros
// ------------------------------------------------------------

#define NVENCC_UPDATE_PROGRESS(X) do \
{ \
	bool ok[2] = { false, false }; \
	unsigned int progressInt = (X)->cap(1).toUInt(&ok[0]); \
	unsigned int progressFrc = (X)->cap(2).toUInt(&ok[1]); \
	setStatus(JobStatus_Running); \
	if(ok[0] && ok[1]) \
	{ \
		const double progress = (double(progressInt) / 100.0) + (double(progressFrc) / 1000.0); \
		if(!qFuzzyCompare(progress, last_progress)) \
		{ \
			setProgress(floor(progress * 100.0)); \
			size_estimate = qFuzzyIsNull(size_estimate) ? estimateSize(m_outputFile, progress) : ((0.667 * size_estimate) + (0.333 * estimateSize(m_outputFile, progress))); \
			last_progress = progress; \
		} \
	} \
	setDetails(line.mid(offset).trimmed()); \
} \
while(0)

#define NVENCC_UPDATE_PROGRESS_OLD(X) do \
{ \
	bool ok = false; \
	unsigned int progressFrames = (X)->cap(1).toUInt(&ok); \
	double progress = 0.0; \
	setStatus(JobStatus_Running); \
	if(ok && (clipInfo.getFrameCount() > 0)) \
	{ \
		progress = (double(progressFrames) / double(clipInfo.getFrameCount())); \
		if(!qFuzzyCompare(progress, last_progress)) \
		{ \
			setProgress(floor(progress * 100.0)); \
			size_estimate = qFuzzyIsNull(size_estimate) ? estimateSize(m_outputFile, progress) : ((0.667 * size_estimate) + (0.333 * estimateSize(m_outputFile, progress))); \
			last_progress = progress; \
		} \
	} \
	setDetails(tr("[%1] %2, est. file size %3").arg(QString().sprintf("%.1f%%", 100.0 * progress), line.mid(offset).trimmed(), sizeToString(qRound64(size_estimate)))); \
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
	virtual QString getName(void) const
	{
		return "NVEncC";
	}

	virtual QList<ArchId> getArchitectures(void) const
	{
		return QList<ArchId>()
		<< qMakePair(QString("32-Bit (x86)"), ARCH_TYPE_X86)
		<< qMakePair(QString("64-Bit (x64)"), ARCH_TYPE_X64);
	}

	virtual QStringList getVariants(void) const
	{
		return QStringList() << "AVC" << "HEVC";
	}

	virtual QList<RCMode> getRCModes(void) const
	{
		return QList<RCMode>()
		<< qMakePair(QString("CQP"),  RC_TYPE_QUANTIZER)
		<< qMakePair(QString("VBR"),  RC_TYPE_RATE_KBPS)
		<< qMakePair(QString("VBR2"), RC_TYPE_RATE_KBPS)
		<< qMakePair(QString("CBR"),  RC_TYPE_RATE_KBPS);
	}

	virtual QStringList getTunings(void) const
	{
		return QStringList();
	}

	virtual QStringList getPresets(void) const
	{
		return QStringList();
	}

	virtual QStringList getProfiles(const quint32 &variant) const
	{
		QStringList profiles;
		switch(variant)
		{
			case 0: profiles << "baseline" << "main" << "high"; break;
			case 1: profiles << "main";                         break;
			default: MUTILS_THROW("Unknown encoder variant!");
		}
		return profiles;
	}

	virtual QStringList supportedOutputFormats(void) const
	{
		QStringList extLst;
		extLst << "mp4" << "264" << "hevc";
		return extLst;
	}

	virtual bool isInputTypeSupported(const int format) const
	{
		switch(format)
		{
		case MediaInfo::FILETYPE_AVISYNTH:
		case MediaInfo::FILETYPE_YUV4MPEG2:
		case MediaInfo::FILETYPE_UNKNOWN:
			return true;
		default:
			return false;
		}
	}

	virtual QString getBinaryPath(const SysinfoModel *sysinfo, const quint32 &encArch, const quint32 &encVariant) const
	{
		QString arch, ext;
		switch(encArch)
		{
			case 0: arch = "x86";             break;
			case 1: arch = "x64"; ext = "64"; break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		switch(encVariant)
		{
			case 0: break;
			case 1: break;
			default: MUTILS_THROW("Unknown encoder variant!");
		}
		return QString("%1/toolset/%2/nvencc/nvencc%3.exe").arg(sysinfo->getAppPath(), arch, ext);
	}

	virtual QStringList getDependencies(const SysinfoModel *sysinfo, const quint32 &encArch, const quint32 &encVariant) const
	{
		QString arch;
		switch (encArch)
		{
			case 0: arch = "x86"; break;
			case 1: arch = "x64"; break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		switch (encVariant)
		{
			case 0: break;
			case 1: break;
			default: MUTILS_THROW("Unknown encoder variant!");

		}
		return QStringList()
		<< QString("%1/toolset/%2/nvencc/avcodec-58.dll"  ).arg(sysinfo->getAppPath(), arch)
		<< QString("%1/toolset/%2/nvencc/avfilter-7.dll"  ).arg(sysinfo->getAppPath(), arch)
		<< QString("%1/toolset/%2/nvencc/avformat-58.dll" ).arg(sysinfo->getAppPath(), arch)
		<< QString("%1/toolset/%2/nvencc/avutil-56.dll"   ).arg(sysinfo->getAppPath(), arch)
		<< QString("%1/toolset/%2/nvencc/swresample-3.dll").arg(sysinfo->getAppPath(), arch);
	}

	virtual QString getHelpCommand(void) const
	{
		return "--help";
	}
};

static const NVEncEncoderInfo s_nvencEncoderInfo;

const AbstractEncoderInfo& NVEncEncoder::encoderInfo(void)
{
	return s_nvencEncoderInfo;
}

const AbstractEncoderInfo &NVEncEncoder::getEncoderInfo(void) const
{
	return encoderInfo();
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
	return s_nvencEncoderInfo.getFullName(m_options->encArch(), m_options->encVariant());
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

void NVEncEncoder::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegExp("\\bNVEncC\\s+\\(x\\d+\\)\\s+(\\d)\\.(\\d+)\\s+\\(r(\\d+)\\)", Qt::CaseInsensitive);
}

void NVEncEncoder::checkVersion_parseLine(const QString &line, const QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified)
{
	if(patterns[0]->lastIndexIn(line) >= 0)
	{
		unsigned int temp[3];
		if(MUtils::regexp_parse_uint32(*patterns[0], temp, 3))
		{
			core = (100 * temp[0]) + temp[1];
			build = temp[2];
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

	return tr("NVEncC version: %1.%2 [rev #%3]").arg(QString::number(core / 100), QString::number(core % 100).leftJustified(2, QLatin1Char('0')), QString::number(build));
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

	return true;
}

// ------------------------------------------------------------
// Encoding Functions
// ------------------------------------------------------------

void NVEncEncoder::buildCommandLine(QStringList &cmdLine, const bool &usePipe, const ClipInfo &clipInfo, const QString &indexFile, const int &pass, const QString &passLogFile)
{
	switch (m_options->encVariant())
	{
	case 0:
		cmdLine << "--codec" << "avc";
		break;
	case 1:
		cmdLine << "--codec" << "hevc";
		break;
	default:
		MUTILS_THROW("Bad encoder variant !!!");
	}

	switch(m_options->rcMode())
	{
	case 0:
		cmdLine << "--cqp" << QString::number(qRound(m_options->quantizer()));
		break;
	case 1:
		cmdLine << "--vbr" << QString::number(m_options->bitrate());
		break;
	case 2:
		cmdLine << "--vbr2" << QString::number(m_options->bitrate());
		break;
	case 3:
		cmdLine << "--cbr" << QString::number(m_options->bitrate());
		break;
	default:
		MUTILS_THROW("Bad rate-control mode !!!");
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
	patterns << new QRegExp("\\[(\\d+)\\.(\\d+)%\\].+frames", Qt::CaseInsensitive);
	patterns << new QRegExp("^(\\d+) frames:", Qt::CaseInsensitive);
	patterns << new QRegExp("Selected\\s+codec\\s+is\\s+not\\s+supported", Qt::CaseInsensitive);
	patterns << new QRegExp("nvEncodeAPI.dll\\s+does\\s+not\\s+exists\\s+in\\s+your\\s+system", Qt::CaseInsensitive);
}

void NVEncEncoder::runEncodingPass_parseLine(const QString &line, const QList<QRegExp*> &patterns, const ClipInfo &clipInfo, const int &pass, double &last_progress, double &size_estimate)
{
	int offset = -1;
	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		NVENCC_UPDATE_PROGRESS(patterns[0]);
	}
	else if ((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		NVENCC_UPDATE_PROGRESS_OLD(patterns[1]);
	}
	else if ((offset = patterns[2]->lastIndexIn(line)) >= 0)
	{
		log(QString("ERROR: YOUR HARDWARE DOES *NOT* SUPPORT THE '%1' CODEC !!!\n").arg(s_nvencEncoderInfo.variantToString(m_options->encVariant())));
	}
	else if ((offset = patterns[3]->lastIndexIn(line)) >= 0)
	{
		log("ERROR: NVIDIA ENCODER API (NVENCODEAPI.DLL) IS *NOT* AVAILABLE !!!\n");
	}
	else if(!line.isEmpty())
	{
		log(line);
	}
}
