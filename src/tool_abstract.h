///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2024 LoRd_MuldeR <MuldeR2@GMX.de>
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
#include <QUuid>
#include <QMutex>
#include <QStringList>
#include <QHash>

class OptionsModel;
class SysinfoModel;
class PreferencesModel;
class JobObject;
class QProcess;
class QSemaphore;
enum JobStatus;

// ------------------------------------------------------------
// Base Class
// ------------------------------------------------------------

class AbstractTool : public QObject
{
	Q_OBJECT

public:
	AbstractTool(JobObject *jobObject, const OptionsModel *options, const SysinfoModel *const sysinfo, const PreferencesModel *const preferences, JobStatus &jobStatus, volatile bool *abort, volatile bool *pause, QSemaphore *semaphorePause);
	virtual ~AbstractTool(void) {/*NOP*/}
	
	virtual QString getName(void) const = 0;

	virtual unsigned int checkVersion(bool &modified);
	virtual bool isVersionSupported(const unsigned int &revision, const bool &modified) = 0;
	virtual QString printVersion(const unsigned int &revision, const bool &modified) = 0;

signals:
	void statusChanged(const JobStatus &newStatus);
	void progressChanged(unsigned int newProgress);
	void messageLogged(const QString &text);
	void detailsChanged(const QString &details);

protected:
	static const unsigned int m_processTimeoutInterval = 2500;
	static const unsigned int m_processTimeoutMaxCounter = 120;
	static const unsigned int m_processTimeoutWarning = 24;

	virtual QString getBinaryPath(void) const = 0;
	virtual QHash<QString, QString> getExtraEnv(void) const { return QHash<QString, QString>(); }
	virtual QStringList getExtraPaths(void) const { return QStringList(); }

	virtual void checkVersion_init(QList<QRegExp*> &patterns, QStringList &cmdLine) = 0;
	virtual void checkVersion_parseLine(const QString &line, const QList<QRegExp*> &patterns, unsigned int &core, unsigned int &build, bool &modified) = 0;
	virtual bool checkVersion_succeeded(const int &exitCode);

	void log(const QString &text) { emit messageLogged(text); }
	void setStatus(const JobStatus &newStatus) { emit statusChanged(newStatus); } 
	void setProgress(unsigned int newProgress) { emit progressChanged(newProgress); }
	void setDetails(const QString &text) { emit detailsChanged(text); }

	bool startProcess(QProcess &process, const QString &program, const QStringList &args, bool mergeChannels = true, const QStringList *const extraPath = NULL, const QHash<QString, QString> *const extraEnv = NULL);

	JobObject *const m_jobObject;
	const OptionsModel *const m_options;
	const SysinfoModel *const m_sysinfo;
	const PreferencesModel *const m_preferences;
	JobStatus &m_jobStatus;
	volatile bool *const m_abort;
	volatile bool *const m_pause;
	QSemaphore *const m_semaphorePause;

	static QString commandline2string(const QString &program, const QStringList &arguments);
	static QStringList splitParams(const QString &params, const QString &sourceFile = QString(), const QString &outputFile = QString());
	static QString stringToHash(const QString &string);
	static unsigned int makeRevision(const unsigned int &core, const unsigned int &build);
	static void splitRevision(const unsigned int &revision, unsigned int &core, unsigned int &build);

	static QMutex s_mutexStartProcess;
};

// ------------------------------------------------------------
// Helper Macros
// ------------------------------------------------------------

#define PROCESS_PENDING_LINES(PROC, HANDLER, ...) do \
{ \
	while((PROC).bytesAvailable() > 0) \
	{ \
		QList<QByteArray> lines = (PROC).readLine().split('\r'); \
		while(!lines.isEmpty()) \
		{ \
			const QString text = QString::fromUtf8(lines.takeFirst().constData()).simplified(); \
			HANDLER(text, __VA_ARGS__); \
		} \
	} \
} \
while(0)
