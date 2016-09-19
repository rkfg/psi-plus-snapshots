/*
 * imageplugin.cpp - plugin
 * Copyright (C) 2009-2010  VampiRus
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include "psiplugin.h"
#include "activetabaccessinghost.h"
#include "activetabaccessor.h"
#include "toolbariconaccessor.h"
#include "iconfactoryaccessor.h"
#include "iconfactoryaccessinghost.h"
#include "gctoolbariconaccessor.h"
#include "stanzasender.h"
#include "stanzasendinghost.h"
#include "accountinfoaccessinghost.h"
#include "accountinfoaccessor.h"
#include "plugininfoprovider.h"
#include "psiaccountcontroller.h"
#include "psiaccountcontrollinghost.h"
#include "optionaccessinghost.h"
#include "optionaccessor.h"
#include "chattabaccessor.h"
#include <QDomElement>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QTextEdit>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QImageReader>
#include <QTextDocumentFragment>

#define constVersion "0.1.2"

#define CONST_LAST_FOLDER "lastfolder"

const int MAX_SIZE = 400;

class ImagePreviewPlugin: public QObject,
		public PsiPlugin,
		public ToolbarIconAccessor,
		public IconFactoryAccessor,
		public ActiveTabAccessor,
		public PluginInfoProvider,
		public OptionAccessor,
		public ChatTabAccessor {
Q_OBJECT
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "com.psi-plus.ImagePreviewPlugin")
#endif
Q_INTERFACES(PsiPlugin ToolbarIconAccessor
		IconFactoryAccessor ActiveTabAccessor
		PluginInfoProvider OptionAccessor ChatTabAccessor)
public:
	ImagePreviewPlugin();
	virtual QString name() const;
	virtual QString shortName() const;
	virtual QString version() const;
	virtual QWidget* options();
	virtual bool enable();
	virtual bool disable();

	virtual void applyOptions() {
	}
	virtual void restoreOptions() {
	}
	virtual QList<QVariantHash> getButtonParam();
	virtual QAction* getAction(QObject*, int, const QString&) {
		return 0;
	}
	virtual QList<QVariantHash> getGCButtonParam();
	virtual QAction* getGCAction(QObject*, int, const QString&) {
		return 0;
	}
	virtual void setIconFactoryAccessingHost(IconFactoryAccessingHost* host);
	virtual void setStanzaSendingHost(StanzaSendingHost *host);
	virtual void setActiveTabAccessingHost(ActiveTabAccessingHost* host);
	virtual void setAccountInfoAccessingHost(AccountInfoAccessingHost* host);
	virtual void setPsiAccountControllingHost(PsiAccountControllingHost *host);
	virtual void setOptionAccessingHost(OptionAccessingHost *host);
	virtual void optionChanged(const QString &) {
	}
	virtual QString pluginInfo();
	virtual QPixmap icon() const;
	virtual void setupChatTab(QWidget*, int, const QString&) {
	}
	virtual void setupGCTab(QWidget* widget, int, const QString&) {
		//GCMainDlg* dlg = qobject_cast<GCMainDlg*>(widget);
		connect(widget, SIGNAL(messageAppended(const QString &, QTextEdit*)),
				this, SLOT(messageAppended(const QString &, QTextEdit*)));
	}

	virtual bool appendingChatMessage(int account, const QString& contact,
			QString& body, QDomElement& html, bool local);

private slots:
	void actionActivated();
	void messageAppended(const QString &, QTextEdit*);
	void imageReply(QNetworkReply* reply);
private:
	IconFactoryAccessingHost* iconHost;
	StanzaSendingHost* stanzaSender;
	ActiveTabAccessingHost* activeTab;
	AccountInfoAccessingHost* accInfo;
	PsiAccountControllingHost *psiController;
	OptionAccessingHost *psiOptions;
	bool enabled;
	QHash<QString, int> accounts_;
	QNetworkAccessManager* manager;
	QSet<QString> pending;
	QSet<QString> failed;
};

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN(ImagePreviewPlugin)
#endif

ImagePreviewPlugin::ImagePreviewPlugin() :
		iconHost(0), stanzaSender(0), activeTab(0), accInfo(0), psiController(
				0), psiOptions(0), enabled(false), manager(
				new QNetworkAccessManager(this)) {
	connect(manager, SIGNAL(finished(QNetworkReply *)), this,
			SLOT(imageReply(QNetworkReply *)));
}

QString ImagePreviewPlugin::name() const {
	return "Image Preview Plugin";
}

QString ImagePreviewPlugin::shortName() const {
	return "imgpreview";
}
QString ImagePreviewPlugin::version() const {
	return constVersion;
}

bool ImagePreviewPlugin::enable() {
	QFile file(":/imagepreviewplugin/imagepreviewplugin.gif");
	if (file.open(QIODevice::ReadOnly)) {
		QByteArray image = file.readAll();
		iconHost->addIcon("imagepreviewplugin/icon", image);
		file.close();
		enabled = true;
	} else {
		enabled = false;
	}
	return enabled;
}

bool ImagePreviewPlugin::disable() {
	enabled = false;
	return true;
}

QWidget* ImagePreviewPlugin::options() {
	if (!enabled) {
		return 0;
	}
	QWidget *optionsWid = new QWidget();
	QVBoxLayout *vbox = new QVBoxLayout(optionsWid);
	QLabel *wikiLink =
			new QLabel(
					tr(
							"<a href=\"http://psi-plus.com/wiki/plugins#image_plugin\">Wiki (Online)</a>"),
					optionsWid);
	wikiLink->setOpenExternalLinks(true);
	vbox->addWidget(wikiLink);
	vbox->addStretch();
	return optionsWid;
}

QList<QVariantHash> ImagePreviewPlugin::getButtonParam() {
	QVariantHash hash;
	hash["tooltip"] = QVariant(tr("Send Image"));
	hash["icon"] = QVariant(QString("imagepreviewplugin/icon"));
	hash["reciver"] = qVariantFromValue(qobject_cast<QObject *>(this));
	hash["slot"] = QVariant(SLOT(actionActivated()));
	QList<QVariantHash> l;
	l.push_back(hash);
	return l;
}

QList<QVariantHash> ImagePreviewPlugin::getGCButtonParam() {
	return getButtonParam();
}

void ImagePreviewPlugin::setAccountInfoAccessingHost(
		AccountInfoAccessingHost* host) {
	accInfo = host;
}

void ImagePreviewPlugin::setIconFactoryAccessingHost(
		IconFactoryAccessingHost* host) {
	iconHost = host;
}

void ImagePreviewPlugin::setPsiAccountControllingHost(
		PsiAccountControllingHost *host) {
	psiController = host;
}

void ImagePreviewPlugin::setOptionAccessingHost(OptionAccessingHost *host) {
	psiOptions = host;
}

void ImagePreviewPlugin::setStanzaSendingHost(StanzaSendingHost *host) {
	stanzaSender = host;
}

void ImagePreviewPlugin::setActiveTabAccessingHost(
		ActiveTabAccessingHost* host) {
	activeTab = host;
}

void ImagePreviewPlugin::actionActivated() {
	if (!enabled)
		return;

	QString fileName("");
	QString jid = activeTab->getYourJid();
	QString jidToSend = activeTab->getJid();
	int account = 0;
	QString tmpJid("");
	while (jid != (tmpJid = accInfo->getJid(account))) {
		++account;
		if (tmpJid == "-1")
			return;
	}

	QMenu m;
	QList<QAction*> list;
	list << new QAction(tr("Open file"), &m)
			<< new QAction(tr("From clipboard"), &m);
	QAction *act = m.exec(list, QCursor::pos());
	if (!act)
		return;

	if ("offline" == accInfo->getStatus(account)) {
		return;
	}

	QPixmap pix;
	QString imageName;
	if (list.indexOf(act) == 1) {
		if (!QApplication::clipboard()->mimeData()->hasImage())
			return;

		pix = QPixmap::fromImage(QApplication::clipboard()->image());
		imageName = QApplication::clipboard()->text();
	} else {
		const QString lastPath = psiOptions->getPluginOption(CONST_LAST_FOLDER,
				QDir::homePath()).toString();
		fileName = QFileDialog::getOpenFileName(0, tr("Open Image"), lastPath,
				tr("Images (*.png *.gif *.jpg *.jpeg *.ico)"));
		if (fileName.isEmpty())
			return;

		QFile file(fileName);
		if (file.open(QIODevice::ReadOnly)) {
			pix = QPixmap::fromImage(QImage::fromData(file.readAll()));
			imageName = QFileInfo(file).fileName();

		} else {
			return;
		}
		psiOptions->setPluginOption(CONST_LAST_FOLDER, QFileInfo(file).path());
	}

	QByteArray image;
	QString mimeType("jpeg");
	if (pix.height() > MAX_SIZE || pix.width() > MAX_SIZE) {
		pix = pix.scaled(MAX_SIZE, MAX_SIZE, Qt::KeepAspectRatio,
				Qt::SmoothTransformation);
	}
	QBuffer b(&image);
	pix.save(&b, mimeType.toLatin1().constData());
	QString imageBase64(image.toBase64());
	int length = image.length();
	if (length > 61440) {
		QMessageBox::information(0, tr("The image size is too large."),
				tr("Image size must be less than 60 kb"));
	}
	QString mType =
			QLatin1String(sender()->parent()->metaObject()->className())
					== "PsiChatDlg" ? "chat" : "groupchat";
	QString body = tr("Image %1 bytes received.").arg(QString::number(length));
	QString msgHtml = QString("<message type=\"%1\" to=\"%2\" id=\"%3\" >"
			"<body>%4</body>"
			"<html xmlns=\"http://jabber.org/protocol/xhtml-im\">"
			"<body xmlns=\"http://www.w3.org/1999/xhtml\">"
			"<br/><img src=\"data:image/%5;base64,%6\" alt=\"img\"/> "
			"</body></html></message>").arg(mType).arg(jidToSend).arg(
			stanzaSender->uniqueId(account)).arg(body).arg(mimeType).arg(
			imageBase64);

	stanzaSender->sendStanza(account, msgHtml);
	psiController->appendSysMsg(account, jidToSend,
			tr(
					"Image %1 sent <br/><img src=\"data:image/%2;base64,%3\" alt=\"img\"/> ").arg(
					imageName).arg(mimeType).arg(imageBase64));

}

QString ImagePreviewPlugin::pluginInfo() {
	return tr("Author: ") + "rkfg\n\n"
			+ trUtf8("This plugin shows the preview image for image URL.\n");
}

QPixmap ImagePreviewPlugin::icon() const {
	return QPixmap(":/imagepreviewplugin/imagepreviewplugin.gif");
}

bool ImagePreviewPlugin::appendingChatMessage(int account,
		const QString& contact, QString& body, QDomElement& html, bool local) {
	qDebug() << "Appending" << contact << ":" << body << " / "
			<< html.toDocument().toString();
	return false;
}

void ImagePreviewPlugin::messageAppended(const QString & message,
		QTextEdit* te_log) {
	if (!enabled) {
		return;
	}
	qDebug() << "RECEIVED:" << message;
	auto cur = te_log->textCursor();
	te_log->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
	while (te_log->find("http", QTextDocument::FindBackward)) {
		te_log->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
		auto url = QString(te_log->textCursor().selectedText()).replace(
				QRegExp("(https?://\\S*)\\s?.*$"), "\\1");
		qDebug() << "URL FOUND:" << url;
		if (failed.contains(url)) {
			break;
		}
		if (!pending.contains(url)) {
			pending.insert(url);
			QNetworkRequest req;
			req.setUrl(QUrl(url));
			req.setOriginatingObject(te_log);
			manager->head(req);
		}
	}
	te_log->setTextCursor(cur);
}

void ImagePreviewPlugin::imageReply(QNetworkReply* reply) {
	bool ok;
	qlonglong size = 0;
	QString contentType;
	QStringList allowedTypes = { "image/jpeg", "image/png", "image/gif" };
	QTextEdit* te_log = qobject_cast<QTextEdit*>(
			reply->request().originatingObject());
	QUrl url = reply->request().url();
	QString urlStr = url.toEncoded();
	switch (reply->operation()) {
	case QNetworkAccessManager::HeadOperation:
		size = reply->header(QNetworkRequest::ContentLengthHeader).toULongLong(
				&ok);
		contentType =
				reply->header(QNetworkRequest::ContentTypeHeader).toString();
		qDebug() << "URL:" << url << "RESULT:" << reply->error() << "SIZE:"
				<< size << "Content-type:" << contentType;
		if (ok && allowedTypes.contains(contentType, Qt::CaseInsensitive)
				&& size < 1024 * 1024) {
			manager->get(reply->request());
		} else {
			pending.remove(urlStr);
			failed.insert(urlStr);
		}
		break;
	case QNetworkAccessManager::GetOperation:
		try {
			QImageReader imageReader(reply);
			auto image = imageReader.read().scaled(150, 150,
					Qt::KeepAspectRatio, Qt::SmoothTransformation);
			if (imageReader.error() != QImageReader::UnknownError) {
				qWarning() << "ERROR:" << imageReader.errorString();
			}
			QUrl url = reply->request().url();
			qDebug() << "Image size:" << image.size();
			te_log->document()->addResource(QTextDocument::ImageResource, url,
					image);
			auto saved = te_log->textCursor();
			te_log->moveCursor(QTextCursor::End);
			while (te_log->find(urlStr, QTextDocument::FindBackward)) {
				auto cur = te_log->textCursor();
				auto sel = cur.selection().toHtml().replace(
						QRegExp("(<a href=\"[^\"]*\">)(.*)(</a>)"),
						QString("\\1<img src='%1'/>\\3").arg(url.toString()));
				cur.insertHtml(sel);
			}
			te_log->setTextCursor(saved);
		} catch (std::exception& e) {
			qWarning() << "ERROR: " << e.what();
		}
		pending.remove(urlStr);
		break;
	default:
		break;
	}
	reply->deleteLater();
}

#include "imagepreviewplugin.moc"
