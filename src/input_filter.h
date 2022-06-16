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

#pragma once

#include <QObject>

template<class K, class T> class QHash;
class QKeyEvent;
class QMouseEvent;

class InputEventFilter : public QObject
{
	Q_OBJECT

public:
	InputEventFilter(QWidget *target);
	~InputEventFilter(void);

	void addKeyFilter(const int &keyCode, const int &tag);
	void addMouseFilter(const int &keyCode, const int &tag);

signals:
	void keyPressed(const int &tag);
	void mouseClicked(const int &tag);

protected:
	bool eventFilter(QObject *obj, QEvent *event);
	bool eventFilter(QKeyEvent *keyEvent);
	bool eventFilter(QMouseEvent *mouseEvent);

	QWidget *const m_target;
	QHash<int, int> *m_keyMapping;
	QHash<int, int> *m_mouseMapping;
};
