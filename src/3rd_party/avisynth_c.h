// Avisynth C Interface Version 0.20
// Copyright 2003 Kevin Atkinson

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
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// As a special exception, I give you permission to link to the
// Avisynth C interface with independent modules that communicate with
// the Avisynth C interface solely through the interfaces defined in
// avisynth_c.h, regardless of the license terms of these independent
// modules, and to copy and distribute the resulting combined work
// under terms of your choice, provided that every copy of the
// combined work is accompanied by a complete copy of the source code
// of the Avisynth C interface and Avisynth itself (with the version
// used to produce the combined work), being distributed under the
// terms of the GNU General Public License plus this exception.  An
// independent module is a module which is not derived from or based
// on Avisynth C Interface, such as 3rd-party filters, import and
// export plugins, or graphical user interfaces.

// NOTE: this is a partial update of the Avisynth C interface to recognize
// new color spaces added in Avisynth 2.60. By no means is this document
// completely Avisynth 2.60 compliant.

#ifndef __AVISYNTH_C__
#define __AVISYNTH_C__

/* AVS uses a versioned interface to control backwards compatibility */
#define AVS_INTERFACE_25 2

/* Types */
typedef struct AVS_Clip AVS_Clip;
typedef struct AVS_ScriptEnvironment AVS_ScriptEnvironment;

/* AVS_Value is layed out identicly to AVSValue */
typedef struct AVS_Value AVS_Value;
struct AVS_Value
{
	short type; // 'a'rray, 'c'lip, 'b'ool, 'i'nt, 'f'loat, 's'tring, 'v'oid, or 'l'ong
	short array_size;
	union
	{
		void * clip; // do not use directly, use avs_take_clip
		char boolean;
		int integer;
		float floating_pt;
		const char * string;
		const AVS_Value * array;
	} d;
};

/* Function pointers */
typedef AVS_ScriptEnvironment* (__stdcall *avs_create_script_environment_func)(int version);
typedef AVS_Value (__stdcall *avs_invoke_func)(AVS_ScriptEnvironment *, const char * name, AVS_Value args, const char** arg_names);
typedef int (__stdcall *avs_function_exists_func)(AVS_ScriptEnvironment *, const char * name);
typedef void (__stdcall *avs_delete_script_environment_func)(AVS_ScriptEnvironment *);
typedef void (__stdcall *avs_release_value_func)(AVS_Value);

/* Inline functions */
inline static int avs_is_int(AVS_Value v) { return v.type == 'i'; }
inline static int avs_is_float(AVS_Value v) { return v.type == 'f' || v.type == 'i'; }
inline static int avs_is_error(AVS_Value v) { return v.type == 'e'; }
inline static int avs_as_bool(AVS_Value v) { return v.d.boolean; }
inline static double avs_as_float(AVS_Value v) { return avs_is_int(v) ? v.d.integer : v.d.floating_pt; }
inline static AVS_Value avs_new_value_array(AVS_Value * v0, int size) { AVS_Value v; v.type = 'a'; v.d.array = v0; v.array_size = size; return v; }

#endif //__AVISYNTH_C__
