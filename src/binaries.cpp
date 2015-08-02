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

#include "binaries.h"

//Internal
#include "global.h"
#include "model_sysinfo.h"
#include "model_preferences.h"
#include "model_options.h"

//MUtils
#include <MUtils/Exception.h>

/* --- Avisynth --- */

QString AVS_BINARY(const SysinfoModel *sysinfo, const bool& x64)
{
	return QString("%1/toolset/%2/avs2yuv_%2.exe").arg(sysinfo->getAppPath(), (x64 ? "x64": "x86"));
}

QString AVS_BINARY(const SysinfoModel *sysinfo, const PreferencesModel *preferences)
{
	const bool avs32 = sysinfo->getAvisynth(SysinfoModel::Avisynth_X86);
	const bool avs64 = sysinfo->getAvisynth(SysinfoModel::Avisynth_X64) && sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64);
	return AVS_BINARY(sysinfo, (avs32 && avs64) ? preferences->getPrefer64BitSource() : avs64);
}

/* --- VapurSynth --- */

QString VPS_BINARY(const SysinfoModel *sysinfo, const bool& x64)
{
	return QString("%1/core%2/vspipe.exe").arg(sysinfo->getVPSPath(), (x64 ? "64" : "32"));
}

QString VPS_BINARY(const SysinfoModel *sysinfo, const PreferencesModel *preferences)
{
	const bool vps32 = sysinfo->getVapourSynth(SysinfoModel::VapourSynth_X86);
	const bool vps64 = sysinfo->getVapourSynth(SysinfoModel::VapourSynth_X64) && sysinfo->getCPUFeatures(SysinfoModel::CPUFeatures_X64);
	return VPS_BINARY(sysinfo, (vps32 && vps64) ? preferences->getPrefer64BitSource() : vps64);
}

/* --- AVS Checker--- */

QString CHK_BINARY(const SysinfoModel *sysinfo, const bool& x64)
{
	return QString("%1/toolset/%2/avs_check_%2.exe").arg(sysinfo->getAppPath(), (x64 ? "x64": "x86"));
}
