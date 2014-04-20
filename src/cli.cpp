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

#include "cli.h"
#include "global.h"

///////////////////////////////////////////////////////////////////////////////
// Pre-defined commands
///////////////////////////////////////////////////////////////////////////////

#define MAKE_ARG(X,Y,Z) { "--"X, (Y), (Z) }

static struct
{
	const char *longName;
	const int optionCount;
	const int identifier;
}
s_parameters[] =
{
	MAKE_ARG( "add",                    1, CLI_PARAM_ADD_FILE         ),
	MAKE_ARG( "add-file",               1, CLI_PARAM_ADD_FILE         ),
	MAKE_ARG( "add-job",                3, CLI_PARAM_ADD_JOB          ),
	MAKE_ARG( "force-start",            0, CLI_PARAM_FORCE_START      ),
	MAKE_ARG( "no-force-start",         0, CLI_PARAM_NO_FORCE_START   ),
	MAKE_ARG( "force-enqueue",          0, CLI_PARAM_FORCE_ENQUEUE    ),
	MAKE_ARG( "no-force-enqueue",       0, CLI_PARAM_NO_FORCE_ENQUEUE ),
	MAKE_ARG( "skip-avisynth-check",    0, CLI_PARAM_SKIP_AVS_CHECK   ),
	MAKE_ARG( "skip-vapoursynth-check", 0, CLI_PARAM_SKIP_VPS_CHECK   ),
	MAKE_ARG( "no-deadlock-detection",  0, CLI_PARAM_NO_DEADLOCK      ),
	MAKE_ARG( "no-style",               0, CLI_PARAM_NO_GUI_STYLE     ),
	MAKE_ARG( "first-run",              0, CLI_PARAM_FIRST_RUN        ),
	MAKE_ARG( "console",                0, CLI_PARAM_OTHER            ),
	MAKE_ARG( "no-console",             0, CLI_PARAM_OTHER            ),
	MAKE_ARG( "force-cpu-no-64bit",     0, CLI_PARAM_OTHER            ),
	MAKE_ARG( "force-cpu-no-sse",       0, CLI_PARAM_OTHER            ),
	MAKE_ARG( "force-cpu-no-intel",     0, CLI_PARAM_OTHER            ),
	{ NULL, 0, 0 }
};

///////////////////////////////////////////////////////////////////////////////
// CLI Parser
///////////////////////////////////////////////////////////////////////////////

CLIParser::CLIParser(const QStringList &args)
:
	m_args(args)
{
	m_iter = m_args.constBegin();

	//Skip the application name, which is expected at args[0]
	if(m_iter != m_args.constEnd()) m_iter++;
}

CLIParser::~CLIParser(void)
{
	/*Nothing to do*/
}

bool CLIParser::nextOption(int &identifier, QStringList *options)
{
	identifier = -1;
	if(options) options->clear();
	int numOpts = 0;

	if(m_iter != m_args.constEnd())
	{
		const QString current = *(m_iter++);
		for(size_t i = 0; s_parameters[i].longName != NULL; i++)
		{
			if(X264_STRCMP(current, QString::fromLatin1(s_parameters[i].longName)))
			{
				numOpts = s_parameters[i].optionCount;
				identifier = s_parameters[i].identifier;
				break;
			}
		}
		if(identifier < 0)
		{
			qWarning("Invalid command-line option \"%s\" was encountered!", current.toUtf8().constData());
			m_iter = m_args.constEnd();
		}
	}

	while((identifier >= 0) && (numOpts > 0))
	{
		if(m_iter != m_args.constEnd())
		{
			if(options)
			{
				*options << *m_iter;
			}
			m_iter++; numOpts--;
		}
		else
		{
			qWarning("Too few arguments for command \"--%s\" specified!", identifier2string(identifier));
			break;
		}
	}

	return ((identifier >= 0) && (numOpts <= 0));
}

bool CLIParser::checkFlag(const int &identifier, const QStringList &args)
{
	CLIParser parser(args);
	int currentId;

	while(parser.nextOption(currentId, NULL))
	{
		if(currentId == identifier)
		{
			return true;
		}
	}

	return false;
}

const char *CLIParser::identifier2string(const int &identifier)
{
	const char *longName = NULL;
	
	for(size_t i = 0; s_parameters[i].longName != NULL; i++)
	{
		if(s_parameters[i].identifier == identifier)
		{
			longName = s_parameters[i].longName;
		}
	}

	return longName;
}
