///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2021 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include <QStringList>

///////////////////////////////////////////////////////////////////////////////
// CLI parameter identifiers
///////////////////////////////////////////////////////////////////////////////

static const char *const CLI_PARAM_ADD_FILE           = "add-file";
static const char *const CLI_PARAM_ADD_JOB            = "add-job";
static const char *const CLI_PARAM_FORCE_START        = "force-start";
static const char *const CLI_PARAM_FORCE_ENQUEUE      = "force-enqueue";
static const char *const CLI_PARAM_SKIP_AVS_CHECK     = "skip-avisynth-check";
static const char *const CLI_PARAM_SKIP_VPS_CHECK     = "skip-vapoursynth-check";
static const char *const CLI_PARAM_SKIP_VERSION_CHECK = "skip-version-checks";
static const char *const CLI_PARAM_NO_DEADLOCK        = "no-deadlock-detection";
static const char *const CLI_PARAM_NO_GUI_STYLE       = "no-style";
static const char* const CLI_PARAM_DARK_GUI_MODE      = "dark-gui-mode";
static const char *const CLI_PARAM_FIRST_RUN          = "first-run";
static const char *const CLI_PARAM_CONSOLE_SHOW       = "console";
static const char *const CLI_PARAM_CONSOLE_HIDE       = "no-console";
static const char *const CLI_PARAM_CPU_NO_64BIT       = "force-cpu-no-64bit";
static const char *const CLI_PARAM_CPU_NO_SSE         = "force-cpu-no-sse";
static const char *const CLI_PARAM_CPU_NO_INTEL       = "force-cpu-no-intel";
