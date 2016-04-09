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

#include "encoder_x265.h"

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
static const unsigned int VERSION_X265_MINIMUM_VER =  19;
static const unsigned int VERSION_X265_MINIMUM_REV = 106;

// ------------------------------------------------------------
// Helper Macros
// ------------------------------------------------------------

#define X265_UPDATE_PROGRESS(X) do \
{ \
	bool ok[2] = { false, false }; \
	unsigned int progressInt = (X)->cap(1).toUInt(&ok[0]); \
	unsigned int progressFrc = (X)->cap(2).toUInt(&ok[1]); \
	setStatus((pass == 2) ? JobStatus_Running_Pass2 : ((pass == 1) ? JobStatus_Running_Pass1 : JobStatus_Running)); \
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

class X265EncoderInfo : public AbstractEncoderInfo
{
public:
	virtual QFlags<OptionsModel::EncVariant> getVariants(void) const
	{
		QFlags<OptionsModel::EncVariant> variants;
		variants |= OptionsModel::EncVariant_8Bit;
		variants |= OptionsModel::EncVariant_10Bit;
		variants |= OptionsModel::EncVariant_12Bit;
		return variants;
	}

	virtual QStringList getTunings(void) const
	{
		QStringList tunings;
		tunings << "Grain" << "PSNR" << "SSIM" << "FastDecode" << "ZeroLatency";
		return tunings;
	}

	virtual QStringList getPresets(void) const
	{
		QStringList presets;
		presets << "ultrafast" << "superfast" << "veryfast" << "faster"   << "fast";
		presets << "medium"    << "slow"      << "slower"   << "veryslow" << "placebo";
		return presets;
	}

	virtual QStringList getProfiles(const OptionsModel::EncVariant &variant) const
	{
		QStringList profiles;
		switch(variant)
		{
		case OptionsModel::EncVariant_8Bit:
			profiles << "main" << "main-intra" << "mainstillpicture" << "main444-8" << "main444-intra" << "main444-stillpicture";
			break;
		case OptionsModel::EncVariant_10Bit:
			profiles << "main10" << "main10-intra" << "main422-10" << "main422-10-intra" << "main444-10" << "main444-10-intra";
			break;
		case OptionsModel::EncVariant_12Bit:
			profiles << "main12" << "main12-intra" << "main422-12" << "main422-12-intra" << "main444-12" << "main444-12-intra";
			break;
		}
		return profiles;
	}

	virtual QStringList supportedOutputFormats(void) const
	{
		QStringList extLst;
		extLst << "hevc";
		return extLst;
	}

	virtual bool isRCModeSupported(const OptionsModel::RCMode &rcMode) const
	{
		switch(rcMode)
		{
		case OptionsModel::RCMode_CRF:
		case OptionsModel::RCMode_CQ:
		case OptionsModel::RCMode_2Pass:
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
			case OptionsModel::EncVariant_10Bit: variant = "10bit"; break;
			case OptionsModel::EncVariant_12Bit: variant = "12bit"; break;
			default: MUTILS_THROW("Unknown encoder arch!");
		}
		return QString("%1/toolset/%2/x265_%3_%2.exe").arg(sysinfo->getAppPath(), arch, variant);
	}
};

static const X265EncoderInfo s_x265EncoderInfo;

const AbstractEncoderInfo &X265Encoder::getEncoderInfo(void)
{
	return s_x265EncoderInfo;
}

// ------------------------------------------------------------
// Constructor & Destructor
// ------------------------------------------------------------

X265Encoder::X265Encoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile)
:
	AbstractEncoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile)
{
	if(options->encType() != OptionsModel::EncType_X265)
	{
		MUTILS_THROW("Invalid encoder type!");
	}
}

X265Encoder::~X265Encoder(void)
{
	/*Nothing to do here*/
}

QString X265Encoder::getName(void) const
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
		case OptionsModel::EncVariant_10Bit: variant = "10-Bit"; break;
		case OptionsModel::EncVariant_12Bit: variant = "12-Bit"; break;
		default: MUTILS_THROW("Unknown encoder arch!");
	}
	return QString("x265 (H.265/HEVC), %1, %2").arg(arch, variant);
}

// ------------------------------------------------------------
// Check Version
// ------------------------------------------------------------

void X265Encoder::checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine)
{
	cmdLine << "--version";
	patterns << new QRegExp("\\bHEVC\\s+encoder\\s+version\\s+(\\d)\\.(\\d+)\\+(\\d+)\\b", Qt::CaseInsensitive);
	patterns << new QRegExp("\\bHEVC\\s+encoder\\s+version\\s+(\\d)\\.(\\d+)\\b", Qt::CaseInsensitive);
}

void X265Encoder::checkVersion_parseLine(const QString &line, QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified)
{
	int offset = -1;

	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		bool ok[3] = { false, false, false };
		unsigned int temp[3];
		temp[0] = patterns[0]->cap(1).toUInt(&ok[0]);
		temp[1] = patterns[0]->cap(2).toUInt(&ok[1]);
		temp[2] = patterns[0]->cap(3).toUInt(&ok[2]);
		if(ok[0] && ok[1])
		{
			core = (10 * temp[0]) + temp[1];
		}
		if(ok[2]) build = temp[2];
	}
	else if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok[2] = { false, false };
		unsigned int temp[2];
		temp[0] = patterns[1]->cap(1).toUInt(&ok[0]);
		temp[1] = patterns[1]->cap(2).toUInt(&ok[1]);
		if(ok[0] && ok[1])
		{
			core = (10 * temp[0]) + temp[1];
		}
		build = 0;
	}

	if(!line.isEmpty())
	{
		log(line);
	}
}

QString X265Encoder::printVersion(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	return tr("x265 version: %1.%2+%3").arg(QString::number(core / 10), QString::number(core % 10), QString::number(build));
}

bool X265Encoder::isVersionSupported(const unsigned int &revision, const bool &modified)
{
	unsigned int core, build;
	splitRevision(revision, core, build);

	if((core < VERSION_X265_MINIMUM_VER) || ((core == VERSION_X265_MINIMUM_VER) && (build < VERSION_X265_MINIMUM_REV)))
	{
		log(tr("\nERROR: Your version of x265 is too old! (Minimum required revision is 0.%1+%2)").arg(QString::number(VERSION_X265_MINIMUM_VER), QString::number(VERSION_X265_MINIMUM_REV)));
		return false;
	}
	else if(core > VERSION_X265_MINIMUM_VER)
	{
		log(tr("\nWARNING: Your version of x265 is newer than the latest tested version, take care!"));
		log(tr("This application works best with x265 version %1.%2. Newer versions may work or not.").arg(QString::number(VERSION_X265_MINIMUM_VER / 10), QString::number(VERSION_X265_MINIMUM_VER % 10)));
	}
	
	return true;
}

// ------------------------------------------------------------
// Encoding Functions
// ------------------------------------------------------------

void X265Encoder::buildCommandLine(QStringList &cmdLine, const bool &usePipe, const unsigned int &frames, const QString &indexFile, const int &pass, const QString &passLogFile)
{
	double crf_int = 0.0, crf_frc = 0.0;

	switch(m_options->rcMode())
	{
	case OptionsModel::RCMode_CQ:
		cmdLine << "--qp" << QString::number(qRound(m_options->quantizer()));
		break;
	case OptionsModel::RCMode_CRF:
		crf_frc = modf(m_options->quantizer(), &crf_int);
		cmdLine << "--crf" << QString("%1.%2").arg(QString::number(qRound(crf_int)), QString::number(qRound(crf_frc * 10.0)));
		break;
	case OptionsModel::RCMode_2Pass:
	case OptionsModel::RCMode_ABR:
		cmdLine << "--bitrate" << QString::number(m_options->bitrate());
		break;
	default:
		MUTILS_THROW("Bad rate-control mode !!!");
		break;
	}
	
	if((pass == 1) || (pass == 2))
	{
		cmdLine << "--pass" << QString::number(pass);
		cmdLine << "--stats" << QDir::toNativeSeparators(passLogFile);
	}

	const QString preset = m_options->preset().simplified().toLower();
	if(!preset.isEmpty())
	{
		if(preset.compare(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED), Qt::CaseInsensitive) != 0)
		{
			cmdLine << "--preset" << preset;
		}
	}

	const QString tune = m_options->tune().simplified().toLower();
	if(!tune.isEmpty())
	{
		if(tune.compare(QString::fromLatin1(OptionsModel::SETTING_UNSPECIFIED), Qt::CaseInsensitive) != 0)
		{
			cmdLine << "--tune" << tune;
		}
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
		if(frames < 1) MUTILS_THROW("Frames not set!");
		cmdLine << "--frames" << QString::number(frames);
		cmdLine << "--y4m" << "-";
	}
	else
	{
		cmdLine << QDir::toNativeSeparators(m_sourceFile);
	}
}

void X265Encoder::runEncodingPass_init(QList<QRegExp*> &patterns)
{
	patterns << new QRegExp("\\[(\\d+)\\.(\\d+)%\\].+frames");   //regExpProgress
	patterns << new QRegExp("indexing.+\\[(\\d+)\\.(\\d+)%\\]"); //regExpIndexing
	patterns << new QRegExp("^(\\d+) frames:"); //regExpFrameCnt
	patterns << new QRegExp("\\[\\s*(\\d+)\\.(\\d+)%\\]\\s+(\\d+)/(\\d+)\\s(\\d+).(\\d+)\\s(\\d+).(\\d+)\\s+(\\d+):(\\d+):(\\d+)\\s+(\\d+):(\\d+):(\\d+)"); //regExpModified
}

void X265Encoder::runEncodingPass_parseLine(const QString &line, QList<QRegExp*> &patterns, const int &pass, double &last_progress, double &size_estimate)
{
	int offset = -1;
	if((offset = patterns[0]->lastIndexIn(line)) >= 0)
	{
		X265_UPDATE_PROGRESS(patterns[0]);
	}
	else if((offset = patterns[1]->lastIndexIn(line)) >= 0)
	{
		bool ok = false;
		unsigned int progress = patterns[1]->cap(1).toUInt(&ok);
		setStatus(JobStatus_Indexing);
		if(ok)
		{
			setProgress(progress);
		}
		setDetails(line.mid(offset).trimmed());
	}
	else if((offset = patterns[2]->lastIndexIn(line)) >= 0)
	{
		setStatus((pass == 2) ? JobStatus_Running_Pass2 : ((pass == 1) ? JobStatus_Running_Pass1 : JobStatus_Running));
		setDetails(line.mid(offset).trimmed());
	}
	else if((offset = patterns[3]->lastIndexIn(line)) >= 0)
	{
		X265_UPDATE_PROGRESS(patterns[3]);
	}
	else if(!line.isEmpty())
	{
		log(line);
	}
}
