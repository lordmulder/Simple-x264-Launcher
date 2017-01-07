///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2017 LoRd_MuldeR <MuldeR2@GMX.de>
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
// 51 Franklin Street, Fifth Floory, Boston, MA 02110-1301 USA.
//
// http://www.gnu.org/licenses/gpl-2.0.txt
///////////////////////////////////////////////////////////////////////////////

#include "encoder_factory.h"

//Internal
#include "global.h"
#include "model_options.h"
#include "encoder_x264.h"
#include "encoder_x265.h"
#include "encoder_nvenc.h"

//MUtils
#include <MUtils/Exception.h>

AbstractEncoder *EncoderFactory::createEncoder(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause, const QString &sourceFile, const QString &outputFile)
{
	AbstractEncoder *encoder = NULL;

	switch(options->encType())
	{
	case OptionsModel::EncType_X264:
		encoder = new X264Encoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile);
		break;
	case OptionsModel::EncType_X265:
		encoder = new X265Encoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile);
		break;
	case OptionsModel::EncType_NVEnc:
		encoder = new NVEncEncoder(jobObject, options, sysinfo, preferences, jobStatus, abort, pause, semaphorePause, sourceFile, outputFile);
		break;
	default:
		MUTILS_THROW("Unknown encoder type encountered!");
	}

	return encoder;
}

const AbstractEncoderInfo& EncoderFactory::getEncoderInfo(const OptionsModel::EncType &encoderType)
{
	switch(encoderType)
	{
	case OptionsModel::EncType_X264:
		return X264Encoder::encoderInfo();
	case OptionsModel::EncType_X265:
		return X265Encoder::encoderInfo();
	case OptionsModel::EncType_NVEnc:
		return NVEncEncoder::encoderInfo();
	default:
		MUTILS_THROW("Unknown encoder type encountered!");
	}

	return *((AbstractEncoderInfo*)NULL);
}
