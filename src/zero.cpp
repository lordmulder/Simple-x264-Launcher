///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2022 LoRd_MuldeR <MuldeR2@GMX.de>
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

//MUtils
#include <MUtils/OSSupport.h>

//Qt
#include <QtGlobal>

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////

static size_t x264_entry_check(void);
static size_t g_x264_entry_check_result = x264_entry_check();
static size_t g_x264_entry_check_flag = 0x789E09B2;

/*
 * Entry point checks
 */
static size_t x264_entry_check(void)
{
	volatile size_t retVal = 0xA199B5AF;
	if(g_x264_entry_check_flag != 0x8761F64D)
	{
		MUtils::OS::fatal_exit(L"Application initialization has failed, take care!");
	}
	return retVal;
}

/*
 * Function declarations
 */
extern "C" int mainCRTStartup(void);

/*
 * Application entry point (runs before static initializers)
 */
extern "C" int x264_entry_point(void)
{
	if(g_x264_entry_check_flag != 0x789E09B2)
	{
		MUtils::OS::fatal_exit(L"Application initialization has failed, take care!");
	}

	//Make sure we will pass the check
	g_x264_entry_check_flag = ~g_x264_entry_check_flag;

	//Now initialize the C Runtime library!
	return mainCRTStartup();
}
