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

/*
   BLAKE2 reference source code package - reference C implementations

   Written in 2012 by Samuel Neves <sneves@dei.uc.pt>

   To the extent possible under law, the author(s) have dedicated all copyright
   and related and neighboring rights to this software to the public domain
   worldwide. This software is distributed without any warranty.

   You should have received a copy of the CC0 Public Domain Dedication along with
   this software. If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

#include "checksum.h"

#include "global.h"
#include "3rd_party/blake2.h"

#include <malloc.h>
#include <string.h>
#include <stdexcept>

static const size_t HASH_SIZE = 64;

class QBlake2ChecksumContext
{
	friend QBlake2Checksum;

	QBlake2ChecksumContext(void)
	{
		if(!(state = (blake2b_state*) _aligned_malloc(sizeof(blake2b_state), 64)))
		{
			THROW("Aligend malloc has failed!");
		}
		memset(state, 0, sizeof(blake2b_state));
	}

	~QBlake2ChecksumContext(void)
	{
		memset(state, 0, sizeof(blake2b_state));
		_aligned_free(state);
	}

private:
	blake2b_state *state;
};

QBlake2Checksum::QBlake2Checksum(const char* key)
:
	m_hash(HASH_SIZE, '\0'),
	m_context(new QBlake2ChecksumContext()),
	m_finalized(false)
{
	if(key && key[0])
	{
		blake2b_init_key(m_context->state, HASH_SIZE, key, strlen(key));
	}
	else
	{
		blake2b_init(m_context->state, HASH_SIZE);
	}
}

QBlake2Checksum::~QBlake2Checksum(void)
{
	delete m_context;
}

void QBlake2Checksum::update(const QByteArray &data)
{
	if(m_finalized)
	{
		THROW("BLAKE2 was already finalized!");
	}

	if(data.size() > 0)
	{
		if(blake2b_update(m_context->state, (const uint8_t*) data.constData(), data.size()) != 0)
		{
			THROW("BLAKE2 internal error!");
		}
	}
}

void QBlake2Checksum::update(QFile &file)
{
	bool okay = false;
	
	for(;;)
	{
		QByteArray data = file.read(16384);
		if(data.size() > 0)
		{
			okay = true;
			update(data);
			continue;
		}
		break;
	}

	if(!okay)
	{
		qWarning("[QBlake2Checksum] Could not ready any data from file!");
	}
}

QByteArray QBlake2Checksum::finalize(const bool bAsHex)
{
	if(!m_finalized)
	{
		if(blake2b_final(m_context->state, (uint8_t*) m_hash.data(), m_hash.size()) != 0)
		{
			THROW("BLAKE2 internal error!");
		}

		m_finalized = true;
	}

	return bAsHex ? m_hash.toHex() : m_hash;
}
