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

#include "input_filter.h"

#include "global.h"

#include <QWidget>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QHash>

InputEventFilter::InputEventFilter(QWidget *target)
:
	m_target(target),
	m_keyMapping(new QHash<int, int>()),
	m_mouseMapping(new QHash<int, int>())
{
	m_target->installEventFilter(this);
}

InputEventFilter::~InputEventFilter(void)
{
	m_target->removeEventFilter(this);
	X264_DELETE(m_keyMapping);
	X264_DELETE(m_mouseMapping);
}

void InputEventFilter::addKeyFilter(const int &keyCode, const int &tag)
{
	m_keyMapping->insert(keyCode, tag);
}

void InputEventFilter::addMouseFilter(const int &mouseCode, const int &tag)
{
	m_mouseMapping->insert(mouseCode, tag);
}

bool InputEventFilter::eventFilter(QObject *obj, QEvent *event)
{
	if(obj == m_target)
	{
		if(event->type() == QEvent::KeyPress)
		{
			QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>(event);
			if(keyEvent)
			{
				return eventFilter(keyEvent);
			}
		}
		else if(event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
			if(mouseEvent)
			{
				return eventFilter(mouseEvent);
			}
		}
	}
	return false;
}

bool InputEventFilter::eventFilter(QKeyEvent *keyEvent)
{
	const int keyCode = keyEvent->key() | keyEvent->modifiers();
	if(m_keyMapping->contains(keyCode))
	{
		emit keyPressed(m_keyMapping->value(keyCode));
		return true;
	}
	return false;
}

bool InputEventFilter::eventFilter(QMouseEvent *mouseEvent)
{
	if(m_mouseMapping->contains(mouseEvent->button()))
	{
		emit mouseClicked(m_mouseMapping->value(mouseEvent->button()));
		return true;
	}
	return false;
}
