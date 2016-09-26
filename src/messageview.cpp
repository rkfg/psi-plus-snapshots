/*
 * messageview.cpp - message data for chatview
 * Copyright (C) 2010 Rion
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include "messageview.h"
#include "textutil.h"
#include "psioptions.h"
#include "common.h"

#include <QTextDocument>

static const QString me_cmd = "/me ";

MessageView::MessageView(Type t)
	: _type(t)
	, _emote(false)
	, _alert(false)
	, _local(false)
	, _spooled(false)
	, _awaitingReceipt(false)
	, _status(0)
	, _statusPriority(0)
	, _dateTime(QDateTime::currentDateTime())
{

}

MessageView MessageView::fromPlainText(const QString &text, Type type)
{
	MessageView mv(type);
	mv.setPlainText(text);
	return mv;
}

MessageView MessageView::fromHtml(const QString &text, Type type)
{
	MessageView mv(type);
	mv.setHtml(text);
	return mv;
}

MessageView MessageView::urlsMessage(const QMap<QString, QString> &urls)
{
	MessageView mv(Urls);
	mv._type = Urls;
	mv._urls = urls;
	return mv;
}

MessageView MessageView::subjectMessage(const QString &subject, const QString &prefix)
{
	MessageView mv(Subject);
	mv._text = TextUtil::escape(prefix);
	mv._userText = subject;
	return mv;
}

MessageView MessageView::statusMessage(const QString &nick, int status,
									   const QString &statusText, int priority)
{
	MessageView mv = MessageView::fromPlainText(QObject::tr("%1 is now %2")
												.arg(nick, status2txt(status)),
												Status);
	mv.setNick(nick);
	mv.setStatus(status);
	mv.setStatusPriority(priority);
	mv.setUserText(statusText);
	return mv;
}

// getters and setters

void MessageView::setPlainText(const QString &text)
{
	if (!text.isEmpty()) {
		if (_type == Message) {
			_emote = text.startsWith(me_cmd);
		}
		_text = TextUtil::plain2rich(text);
		if (_type == Message) {
			_text = TextUtil::linkify(_text);
		}
	}
}

void MessageView::setHtml(const QString &text)
{
	if (_type == Message) {
		QString str = TextUtil::rich2plain(text).trimmed();
		_emote = str.startsWith(me_cmd);
		if(_emote) {
			setPlainText(str);
			return;
		}
	}
	_text = text;
}

QString MessageView::formattedText() const
{
	QString txt = _text;

	if (_emote && _type == Message) {
		int cmd = txt.indexOf(me_cmd);
		txt = txt.remove(cmd, me_cmd.length());
	}
	if (PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool())
		txt = TextUtil::emoticonify(txt);
	if (PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool())
		txt = TextUtil::legacyFormat(txt);

	return txt + "<span id=\"msgid_" + TextUtil::escape(messageId() + "_" + userId()) + "\"> </span>";
}

QString MessageView::formattedUserText() const
{
	if (!_userText.isEmpty()) {
		QString text = TextUtil::plain2rich(_userText);
		text = TextUtil::linkify(text);
		if (PsiOptions::instance()->getOption("options.ui.emoticons.use-emoticons").toBool())
			text = TextUtil::emoticonify(text);
		if (PsiOptions::instance()->getOption("options.ui.chat.legacy-formatting").toBool())
			text = TextUtil::legacyFormat(text);
		return text;
	}
	return "";
}

QVariantMap MessageView::toVariantMap(bool isMuc, bool formatted) const
{
	static QHash<Type, QString> types;
	if (types.isEmpty()) {
		types.insert(Message,	"message");
		types.insert(System,	"system");
		types.insert(Status,	"status");
		types.insert(Subject,	"subject");
		types.insert(Urls,		"urls");
	}
	QVariantMap m;
	m["time"] = _dateTime;
	m["type"] = types.value(_type);
	switch (_type) {
		case Message:
			m["message"] = formatted?formattedText():_text;
			m["emote"] = _emote;
			m["local"] = _local;
			m["sender"] = _nick;
			m["userid"] = _userId;
			m["spooled"] = _spooled;
			m["id"] = _messageId;
			if (isMuc) { // maybe w/o conditions ?
				m["alert"] = _alert;
			} else {
				m["awaitingReceipt"] = _awaitingReceipt;
			}
			break;
		case Status:
			m["sender"] = _nick;
			m["status"] = _status;
			m["priority"] = _statusPriority;
			m["message"] = _text;
			m["usertext"] = formatted?formattedUserText():_userText;
			break;
		case System:
		case Subject:
			m["message"] = formatted?formattedText():_text;
			m["usertext"] = formatted?formattedUserText():_userText;
			break;
		case Urls:
			QVariantMap vmUrls;
			foreach (const QString &u, _urls.keys()) {
				vmUrls[u] = _urls.value(u);
			}
			m["urls"] = vmUrls;
			break;
	}
	return m;
}
