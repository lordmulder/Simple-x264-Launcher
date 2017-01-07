///////////////////////////////////////////////////////////////////////////////
// Simple x264 Launcher
// Copyright (C) 2004-2017 LoRd_MuldeR <MuldeR2@GMX.de>
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

#include "string_validator.h"

///////////////////////////////////////////////////////////////////////////////
// StringValidator
///////////////////////////////////////////////////////////////////////////////

StringValidator::StringValidator(QLabel *notifier, QLabel *icon)
:
	m_notifier(notifier), m_icon(icon)
{
	m_notifier->hide();
	m_icon->hide();
}
	
void StringValidator::fixup(QString &input) const
{
	input = input.simplified();
}


bool StringValidator::checkParam(const QStringList &input, const char *const params[], const bool &doubleMinus) const
{
	for(QStringList::ConstIterator iter = input.constBegin(); iter != input.constEnd(); iter++)
	{
		for(size_t k = 0; params[k]; k++)
		{
			const QString param = QLatin1String(params[k]);
			const QString prefix = ((param.length() > 1) && doubleMinus) ? QLatin1String("--") : QLatin1String("-");
			if(iter->compare(QString("%1%2").arg(prefix, param), Qt::CaseInsensitive) == 0)
			{
				if(m_notifier)
				{
					m_notifier->setText(tr("Forbidden parameter: %1").arg(*iter));
				}
				return true;
			}
		}
		if(iter->startsWith(QLatin1String("--"), Qt::CaseInsensitive))
		{
			for(int i = 2; i < iter->length(); i++)
			{
				const QChar c = iter->at(i);
				if((c == QLatin1Char('=')) && (i > 2) && (i + 1 < iter->length()))
				{
					break; /*to allow "--param=value" format*/
				}
				if((!c.isLetter()) && ((i < 3) || ((!c.isNumber()) && ((i + 1 >= iter->length()) || (c != QLatin1Char('-'))))))
				{
					if(m_notifier)
					{
						m_notifier->setText(tr("Invalid string: %1").arg(*iter));
					}
					return true;
				}
			}
		}
	}

	return false;
}

bool StringValidator::checkPrefix(const QStringList &input, const bool &doubleMinus) const
{
	for(QStringList::ConstIterator iter = input.constBegin(); iter != input.constEnd(); iter++)
	{
		static const char *const c[3] = { "--", "-", NULL };
		for(size_t i = 0; c[i]; i++)
		{
			const QString prefix = QString::fromLatin1(c[i]);
			if(iter->compare(prefix, Qt::CaseInsensitive) == 0)
			{
				if(m_notifier)
				{
					m_notifier->setText(tr("Invalid parameter: %1").arg(prefix));
				}
				return true;
			}
		}
		if
		(
			((!doubleMinus) && iter->startsWith("--", Qt::CaseInsensitive)) ||
			(doubleMinus && iter->startsWith("-", Qt::CaseInsensitive) && (!iter->startsWith("--", Qt::CaseInsensitive)) && (iter->length() > 2) && (!iter->at(1).isDigit())) ||
			(doubleMinus && iter->startsWith("--", Qt::CaseInsensitive) && (iter->length() < 4))
		)
		{
			if(m_notifier)
			{
				m_notifier->setText(tr("Invalid syntax: %1").arg(*iter));
			}
			return true;
		}
	}
	return false;
}

bool StringValidator::checkCharacters(const QStringList &input) const
{
	static const char c[] = {'*', '?', '<', '>', '|', NULL};

	for(QStringList::ConstIterator iter = input.constBegin(); iter != input.constEnd(); iter++)
	{
		for(size_t i = 0; c[i]; i++)
		{
			if(iter->indexOf(QLatin1Char(c[i])) >= 0)
			{
				if(m_notifier)
				{
					m_notifier->setText(tr("Invalid character: '%1'").arg(QLatin1Char(c[i])));
				}
				return true;
			}
		}
	}
	return false;
}

const bool &StringValidator::setStatus(const bool &flag, const QString &toolName) const
{
	if(flag)
	{
		if(m_notifier)
		{
			if(m_notifier->isHidden()) m_notifier->show();
			if(m_icon) { if(m_icon->isHidden()) m_icon->show(); }
			if(QWidget *w = m_notifier->topLevelWidget()->focusWidget())
			{
				QToolTip::showText(static_cast<QWidget*>(w->parent())->mapToGlobal(w->pos()), tr("<b>Warning:</b> You entered a parameter that is forbidden. Please note that the GUI will automatically set <i>this</i> parameter for you (if required)."), m_notifier, QRect());
			}
		}
	}
	else
	{
		if(m_notifier)
		{
			if(m_notifier->isVisible()) m_notifier->hide();
			if(m_icon) { if(m_icon->isVisible()) m_icon->hide(); }
			QToolTip::hideText();
		}
	}
	return flag;
}

///////////////////////////////////////////////////////////////////////////////
// StringValidatorEncoder
///////////////////////////////////////////////////////////////////////////////

QValidator::State StringValidatorEncoder::validate(QString &input, int &pos) const
{
	static const char *const params[] = {"B", "o", "h", "p", "q", /*"fps", "frames",*/ "preset", "tune", "profile",
		"stdin", "crf", "bitrate", "qp", "pass", "stats", "output", "help", "quiet", "codec", "y4m", NULL};

	const QString commandLine = input.trimmed();
	const QStringList tokens =  commandLine.isEmpty() ? QStringList() : MUtils::OS::crack_command_line(commandLine);

	const bool invalid = checkCharacters(tokens) || checkPrefix(tokens, true) || checkParam(tokens, params, true);
	return setStatus(invalid, "encoder") ? QValidator::Intermediate : QValidator::Acceptable;
}

///////////////////////////////////////////////////////////////////////////////
// StringValidatorSource
///////////////////////////////////////////////////////////////////////////////

QValidator::State StringValidatorSource::validate(QString &input, int &pos) const
{
	static const char *const params[] = {"o", "frames", "seek", "raw", "hfyu", "slave", NULL};

	const QString commandLine = input.trimmed();
	const QStringList tokens =  commandLine.isEmpty() ? QStringList() : MUtils::OS::crack_command_line(commandLine);

	const bool invalid = checkCharacters(tokens) || checkPrefix(tokens, false) || checkParam(tokens, params, false);
	return setStatus(invalid, "Avs2YUV") ? QValidator::Intermediate : QValidator::Acceptable;
}
