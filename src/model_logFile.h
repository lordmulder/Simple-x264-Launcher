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

#include "thread_encode.h"

#include "QAbstractItemModel"
#include <QUuid>
#include <QList>
#include <QMap>

class LogFileModel : public QAbstractItemModel
{
	Q_OBJECT
		
public:
	LogFileModel(const QString &sourceName, const QString &outputName, const QString &configName);
	~LogFileModel(void);

	virtual int columnCount(const QModelIndex &parent) const;
	virtual int rowCount(const QModelIndex &parent) const;
	virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;
	virtual QModelIndex index(int row, int column, const QModelIndex &parent) const;
	virtual QModelIndex parent (const QModelIndex &index) const;
	virtual QVariant data(const QModelIndex &index, int role) const;

	void copyToClipboard(void) const;
	bool saveToLocalFile(const QString &fileName) const;

protected:
	bool m_firstLine;
	typedef QPair<qint64, QString> LogEntry;
	QList<LogEntry> m_lines;

public slots:
	void addLogMessage(const QUuid &jobId, const qint64 &timeStamp, const QString &text);
};
