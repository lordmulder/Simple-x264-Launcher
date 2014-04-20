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

#include <QStringList>

///////////////////////////////////////////////////////////////////////////////
// CLI parameter identifiers
///////////////////////////////////////////////////////////////////////////////

static const int CLI_PARAM_ADD_FILE         =  0;
static const int CLI_PARAM_ADD_JOB          =  1;
static const int CLI_PARAM_FORCE_START      =  2;
static const int CLI_PARAM_NO_FORCE_START   =  3;
static const int CLI_PARAM_FORCE_ENQUEUE    =  4;
static const int CLI_PARAM_NO_FORCE_ENQUEUE =  5;
static const int CLI_PARAM_SKIP_AVS_CHECK   =  6;
static const int CLI_PARAM_SKIP_VPS_CHECK   =  7;
static const int CLI_PARAM_SKIP_X264_CHECK  =  8;
static const int CLI_PARAM_NO_DEADLOCK      =  9;
static const int CLI_PARAM_NO_GUI_STYLE     = 10;
static const int CLI_PARAM_FIRST_RUN        = 11;
static const int CLI_PARAM_OTHER            = 42;

///////////////////////////////////////////////////////////////////////////////
// CLI Parser
///////////////////////////////////////////////////////////////////////////////

class CLIParser
{
public:
	CLIParser(const QStringList &args);
	~CLIParser(void);

	bool nextOption(int &identifier, QStringList *options = NULL);

	static bool checkFlag(const int &identifier, const QStringList &args);
	static const char *identifier2string(const int &identifier);

protected:
	const QStringList &m_args;
	QStringList::ConstIterator m_iter;
};
