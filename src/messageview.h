/*
 * messageview.h - message data for chatview
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

#ifndef MESSAGEVIEW_H
#define MESSAGEVIEW_H

#include <QDateTime>
#include <QVariantMap>

class MessageView
{
public:
	enum Type {
		Message,
		System,
		Status,
		Subject,
		Urls
	};

	enum SystemMessageType {
		MUCJoin,
		MUCPart,
		FileTransferRequest,
		FileTransferFinished
	};

	MessageView(Type);

	static MessageView fromPlainText(const QString &, Type);
	static MessageView fromHtml(const QString &, Type);
	static MessageView urlsMessage(const QMap<QString, QString> &);
	static MessageView subjectMessage(const QString &subject,
									  const QString &prefix = QString());

	// accepts plain/text nick, plain/text status and rich/text statusText
	static MessageView statusMessage(const QString &nick, int status,
									 const QString &statusText = QString(),
									 int priority = 0);

	inline Type type() const { return _type; }
	inline const QString &text() const { return _text; }
	inline void setText(const QString &text) { _text = text; }
	inline const QString &userText() const { return _userText; }
	inline void setUserText(const QString &text) { _userText = text; }

	void setPlainText(const QString &);
	void setHtml(const QString &);
	QString formattedText() const;
	QString formattedUserText() const;

	inline void setAlert(bool state) { _alert = state; }
	inline bool isAlert() const { return _alert; }
	inline void setLocal(bool state) { _local = state; }
	inline bool isLocal() const { return _local; }
	inline void setEmote(bool state) { _emote = state; }
	inline bool isEmote() const { return _emote; }
	inline void setSpooled(bool state) { _spooled = state; }
	inline bool isSpooled() const { return _spooled; }
	inline void setStatus(int s) { _status = s; }
	inline int status() const { return _status; }
	inline void setStatusPriority(int s) { _statusPriority = s; }
	inline int statusPriority() const { return _statusPriority; }
	inline void setAwaitingReceipt(bool b) { _awaitingReceipt = b; }
	inline bool isAwaitingReceipt() const { return _awaitingReceipt; }
	inline void setNick(const QString &nick) { _nick = nick; }
	inline const QString &nick() const { return _nick; }
	inline void setMessageId(const QString &id) { _messageId = id; }
	inline const QString &messageId() const { return _messageId; }
	inline void setUserId(const QString &id) { _userId = id; }
	inline const QString &userId() const { return _userId; }
	inline void setDateTime(const QDateTime &dt) { _dateTime = dt; }
	inline const QDateTime &dateTime() const { return _dateTime; }
	inline QMap<QString, QString> urls() const { return _urls; }
	inline void setReplaceId(const QString &id) { _replaceId = id; }
	inline const QString &replaceId() const { return _replaceId; }

	QVariantMap toVariantMap(bool isMuc, bool formatted = false) const;

private:
	Type _type;
	bool _emote;
	bool _alert;
	bool _local;
	bool _spooled;
	bool _awaitingReceipt;
	int _status;
	int _statusPriority;
	QString _messageId;
	QString _userId;
	QString _nick; // rich / as is
	QString _text; // always rich (plain text converted to rich)
	QString _userText; // rich
	QDateTime _dateTime;
	QMap<QString, QString> _urls;
	QString _replaceId;
};

#endif
