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

#include "model_logFile.h"
#include "thread_encode.h"

#include <QIcon>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QTextStream>

LogFileModel::LogFileModel(const QString &sourceName, const QString &outputName, const QString &configName)
{
	m_lines << "Job not started yet." << QString();
	m_lines << QString("Scheduled source: %1").arg(QDir::toNativeSeparators(sourceName));
	m_lines << QString("Scheduled output: %1").arg(QDir::toNativeSeparators(outputName));
	m_lines << QString("Scheduled config: %1").arg(configName);
	m_firstLine = true;
}

LogFileModel::~LogFileModel(void)
{
}

///////////////////////////////////////////////////////////////////////////////
// Model interface
///////////////////////////////////////////////////////////////////////////////

int LogFileModel::columnCount(const QModelIndex &parent) const
{
	return 1;
}

int LogFileModel::rowCount(const QModelIndex &parent) const
{
	return m_lines.count();
}

QVariant LogFileModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
	return QVariant();
}

QModelIndex LogFileModel::index(int row, int column, const QModelIndex &parent) const
{
	return createIndex(row, column, NULL);
}

QModelIndex LogFileModel::parent(const QModelIndex &index) const
{
	return QModelIndex();
}

QVariant LogFileModel::data(const QModelIndex &index, int role) const
{
	if((role == Qt::DisplayRole) || (role == Qt::ToolTipRole))
	{
		if(index.row() >= 0 && index.row() < m_lines.count() && index.column() == 0)
		{
			return m_lines.at(index.row());
		}
	}

	return QVariant();
}

///////////////////////////////////////////////////////////////////////////////
// Public API
///////////////////////////////////////////////////////////////////////////////

void LogFileModel::copyToClipboard(void)
{
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(m_lines.join("\r\n"));
}

bool  LogFileModel::saveToLocalFile(const QString &fileName)
{
	QFile file(fileName);
	if(!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		return false;
	}

	QTextStream out(&file);
	out.setCodec("UTF-8");
	for(QStringList::ConstIterator iter = m_lines.constBegin(); iter != m_lines.constEnd(); iter++)
	{
		out << (*iter) << QLatin1String("\r\n");
		if(out.status() != QTextStream::Status::Ok)
		{
			file.close();
			return false;
		}
	}

	out.flush();
	if(out.status() != QTextStream::Status::Ok)
	{
		file.close();
		return false;
	}

	file.close();
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// Slots
///////////////////////////////////////////////////////////////////////////////

void LogFileModel::addLogMessage(const QUuid &jobId, const QString &text)
{
	beginInsertRows(QModelIndex(), m_lines.count(), m_lines.count());

	if(m_firstLine)
	{
		m_firstLine = false;
		m_lines.clear();
	}

	QStringList lines = text.split("\n");
	for(int i = 0; i < lines.count(); i++)
	{
		m_lines.append(lines.at(i));
	}

	endInsertRows();
}
