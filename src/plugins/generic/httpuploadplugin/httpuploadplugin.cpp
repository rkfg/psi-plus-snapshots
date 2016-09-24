/*
 * httpuploadplugin.cpp - plugin
 * Copyright (C) 2016  rkfg
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
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QMenu>
#include <QApplication>
#include <QTextDocument>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include "chattabaccessor.h"
#include "stanzafilter.h"
#include <QDomElement>
#include "uploadservice.h"
#include "currentupload.h"

#define constVersion "0.1.0"
#define CONST_LAST_FOLDER "httpupload-lastfolder"
#define SLOT_TIMEOUT 10000

class HttpUploadPlugin: public QObject,
		public PsiPlugin,
		public ToolbarIconAccessor,
		public GCToolbarIconAccessor,
		public StanzaSender,
		public IconFactoryAccessor,
		public ActiveTabAccessor,
		public PluginInfoProvider,
		public AccountInfoAccessor,
		public PsiAccountController,
		public OptionAccessor,
		public ChatTabAccessor,
		public StanzaFilter {
Q_OBJECT
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "com.psi-plus.HttpUploadPlugin")
#endif
Q_INTERFACES(PsiPlugin ToolbarIconAccessor GCToolbarIconAccessor
		StanzaSender ActiveTabAccessor PsiAccountController OptionAccessor
		IconFactoryAccessor AccountInfoAccessor PluginInfoProvider ChatTabAccessor StanzaFilter)
public:
	HttpUploadPlugin();
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
	virtual void setupChatTab(QWidget*, int account, const QString&) {
		checkUploadAvailability(account);
	}
	virtual void setupGCTab(QWidget*, int account, const QString&) {
		checkUploadAvailability(account);
	}

	virtual bool appendingChatMessage(int, const QString&, QString&, QDomElement&, bool) {
		return false;
	}
	virtual bool incomingStanza(int account, const QDomElement& xml);
	virtual bool outgoingStanza(int, QDomElement &) {
		return false;
	}
	QString getId(int account) {
		return stanzaSender->uniqueId(account);
	}
	void checkUploadAvailability(int account);

private slots:
	void uploadFile();
	void uploadImage();
	void uploadComplete(QNetworkReply* reply);
	void timeout();

private:
	void upload(bool anything);
	int accountNumber() {
		QString jid = activeTab->getYourJid();
		QString jidToSend = activeTab->getJid();
		int account = 0;
		QString tmpJid("");
		while (jid != (tmpJid = accInfo->getJid(account))) {
			++account;
			if (tmpJid == "-1")
				return -1;
		}
		return account;
	}

	void cancelTimeout() {
		slotTimeout.stop();
		if (dataSource) {
			dataSource->deleteLater();
		}
	}
	void processServices(const QDomElement& query, int account);
	void processOneService(const QDomElement& query, const QString& service, int account);
	void processUploadSlot(const QDomElement& xml);

	IconFactoryAccessingHost* iconHost;
	StanzaSendingHost* stanzaSender;
	ActiveTabAccessingHost* activeTab;
	AccountInfoAccessingHost* accInfo;
	PsiAccountControllingHost *psiController;
	OptionAccessingHost *psiOptions;
	bool enabled;
	QHash<QString, int> accounts_;
	QNetworkAccessManager* manager;
	QMap<QString, UploadService> serviceNames;
	QPointer<QIODevice> dataSource;
	CurrentUpload currentUpload;
	QTimer slotTimeout;
};

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN(HttpUploadPlugin)
#endif

HttpUploadPlugin::HttpUploadPlugin() :
		iconHost(0), stanzaSender(0), activeTab(0), accInfo(0), psiController(0), psiOptions(0), enabled(false), manager(
				new QNetworkAccessManager(this)) {
	connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(uploadComplete(QNetworkReply*)));
	connect(&slotTimeout, SIGNAL(timeout()), this, SLOT(timeout()));
	slotTimeout.setSingleShot(true);
}

QString HttpUploadPlugin::name() const {
	return "HTTP Upload Plugin";
}

QString HttpUploadPlugin::shortName() const {
	return "httpupload";
}
QString HttpUploadPlugin::version() const {
	return constVersion;
}

bool HttpUploadPlugin::enable() {
	QFile file(":/httpuploadplugin/httpuploadplugin.gif");
	if (file.open(QIODevice::ReadOnly)) {
		QByteArray image = file.readAll();
		iconHost->addIcon("httpuploadplugin/icon", image);
		file.close();
		enabled = true;
	} else {
		enabled = false;
	}
	return enabled;
}

bool HttpUploadPlugin::disable() {
	enabled = false;
	return true;
}

QWidget* HttpUploadPlugin::options() {
	if (!enabled) {
		return 0;
	}
	QWidget *optionsWid = new QWidget();
	QVBoxLayout *vbox = new QVBoxLayout(optionsWid);
	QLabel *wikiLink = new QLabel(tr("<a href=\"http://psi-plus.com/wiki/plugins#image_plugin\">Wiki (Online)</a>"),
			optionsWid);
	wikiLink->setOpenExternalLinks(true);
	vbox->addWidget(wikiLink);
	vbox->addStretch();
	return optionsWid;
}

QList<QVariantHash> HttpUploadPlugin::getButtonParam() {
	QList<QVariantHash> l;
	QVariantHash uploadImg;
	uploadImg["tooltip"] = tr("Upload Image");
	uploadImg["icon"] = QString("httpuploadplugin/icon");
	uploadImg["reciver"] = qVariantFromValue(qobject_cast<QObject *>(this));
	uploadImg["slot"] = QVariant(SLOT(uploadImage()));
	l.push_back(uploadImg);
	QVariantHash uploadFile;
	uploadFile["tooltip"] = tr("Upload File");
	uploadFile["icon"] = QString("httpuploadplugin/icon");
	uploadFile["reciver"] = qVariantFromValue(qobject_cast<QObject *>(this));
	uploadFile["slot"] = QVariant(SLOT(uploadFile()));
	l.push_back(uploadFile);
	return l;
}

QList<QVariantHash> HttpUploadPlugin::getGCButtonParam() {
	return getButtonParam();
}

void HttpUploadPlugin::setAccountInfoAccessingHost(AccountInfoAccessingHost* host) {
	accInfo = host;
}

void HttpUploadPlugin::setIconFactoryAccessingHost(IconFactoryAccessingHost* host) {
	iconHost = host;
}

void HttpUploadPlugin::setPsiAccountControllingHost(PsiAccountControllingHost *host) {
	psiController = host;
}

void HttpUploadPlugin::setOptionAccessingHost(OptionAccessingHost *host) {
	psiOptions = host;
}

void HttpUploadPlugin::setStanzaSendingHost(StanzaSendingHost *host) {
	stanzaSender = host;
}

void HttpUploadPlugin::setActiveTabAccessingHost(ActiveTabAccessingHost* host) {
	activeTab = host;
}

void HttpUploadPlugin::uploadFile() {
	upload(true);
}

void HttpUploadPlugin::uploadImage() {
	upload(false);
}

void HttpUploadPlugin::upload(bool anything) {
	if (!enabled)
		return;
	if (dataSource) {
		QMessageBox::warning(0, tr("Please wait"),
				tr(
						"Another upload operation is already in progress. Please wait up to %1 sec for it to complete or fail.").arg(
				SLOT_TIMEOUT / 1000));
		return;
	}
	QString serviceName;
	int sizeLimit = -1;
	int account = accountNumber();
	QString curJid = accInfo->getJid(account);
	auto iter = serviceNames.find(curJid);
	if (iter == serviceNames.end()) {
		QMessageBox::critical(0, tr("Not supported"),
				tr("Server for account %1 does not support HTTP Upload (XEP-363)").arg(curJid));
		return;
	}
	serviceName = iter->serviceName();
	sizeLimit = iter->sizeLimit();
	QString fileName;
	QString jid = activeTab->getYourJid();
	QString jidToSend = activeTab->getJid();

	if ("offline" == accInfo->getStatus(account)) {
		return;
	}

	QString imageName;
	const QString lastPath = psiOptions->getPluginOption(CONST_LAST_FOLDER, QDir::homePath()).toString();
	fileName = QFileDialog::getOpenFileName(0, anything ? tr("Upload file") : tr("Upload image"), lastPath,
			anything ? "" : tr("Images (*.png *.gif *.jpg *.jpeg)"));
	if (fileName.isEmpty())
		return;

	QFile file(fileName);
	QFileInfo fileInfo(file);
	QPixmap pix(fileName);
	imageName = fileInfo.fileName();
	psiOptions->setPluginOption(CONST_LAST_FOLDER, fileInfo.path());
	QString mimeType("application/octet-stream");
	int length;
	if (!anything && (pix.width() > 1024 || pix.height() > 1024)) {
		auto image = new QByteArray();
		dataSource = new QBuffer(image);
		pix.scaled(1024, 1024, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(dataSource, "jpg");
		length = image->length();
		qDebug() << "Resized length:" << length;
		dataSource->open(QIODevice::ReadOnly);
	} else {
		length = fileInfo.size();
		dataSource = new QFile(fileName, this);
		if (!dataSource->open(QIODevice::ReadOnly)) {
			dataSource->deleteLater();
			QMessageBox::critical(0, tr("Error"), tr("Error opening file %1").arg(fileName));
			return;
		}
	}
	if (length > sizeLimit) {
		QMessageBox::critical(0, tr("The file size is too large."),
				tr("File size must be less than %1 bytes").arg(sizeLimit));
		return;
	}
	currentUpload.account = account;
	currentUpload.from = jid;
	currentUpload.to = jidToSend;
	currentUpload.type =
			QLatin1String(sender()->parent()->metaObject()->className()) == "PsiChatDlg" ? "chat" : "groupchat";

	QString slotRequestStanza = QString("<iq from='%1' id='%2' to='%3' type='get'>"
			"<request xmlns='urn:xmpp:http:upload'>"
			"<filename>%4</filename>"
			"<size>%5</size>"
			"<content-type>%6</content-type>"
			"</request>"
			"</iq>").arg(jid).arg(getId(account)).arg(serviceName).arg(Qt::escape(imageName)).arg(length).arg(mimeType);
	qDebug() << "Requesting slot:" << slotRequestStanza;
	slotTimeout.start(SLOT_TIMEOUT);
	stanzaSender->sendStanza(account, slotRequestStanza);
}

QString HttpUploadPlugin::pluginInfo() {
	return tr("Authors: ") + "rkfg\n\n" + trUtf8("This plugin allows uploading images and other files via XEP-0363.");
}

QPixmap HttpUploadPlugin::icon() const {
	return QPixmap(":/httpuploadplugin/httpuploadplugin.gif");
}

void HttpUploadPlugin::checkUploadAvailability(int account) {
	QString curJid = accInfo->getJid(account);
	if (serviceNames.find(curJid) != serviceNames.end()) {
		return;
	}
	QRegExp jidRE("^([^@]*)@([^/]*)$");
	if (jidRE.indexIn(curJid) == 0) {
		QString domain = jidRE.cap(2);
		QString id = getId(account);
		QString disco = QString("<iq from='%1' id='%2' to='%3' type='get'>"
				"<query xmlns='http://jabber.org/protocol/disco#items'/>"
				"</iq>").arg(curJid).arg(id).arg(domain);
		stanzaSender->sendStanza(account, disco);
	}
}

void HttpUploadPlugin::processServices(const QDomElement& query, int account) {
	QString curJid = accInfo->getJid(account);
	auto nodes = query.childNodes();
	for (int i = 0; i < nodes.count(); i++) {
		auto elem = nodes.item(i).toElement();
		if (elem.tagName() == "item") {
			QString serviceJid = elem.attribute("jid");
			QString serviceDiscoStanza = QString("<iq from='%1' id='%2' to='%3' type='get'>"
					"<query xmlns='http://jabber.org/protocol/disco#info'/>"
					"</iq>").arg(curJid).arg(getId(account)).arg(serviceJid);
			qDebug() << "Discovering service" << serviceJid;
			stanzaSender->sendStanza(account, serviceDiscoStanza);
		}
	}
}

void HttpUploadPlugin::processOneService(const QDomElement& query, const QString& service, int account) {
	QString curJid = accInfo->getJid(account);
	int sizeLimit = -1;
	auto feature = query.firstChildElement("feature");
	bool ok = false;
	while (!feature.isNull()) {
		if (feature.attribute("var") == "urn:xmpp:http:upload") {
			qDebug() << "Service" << service << "looks like http upload";
			auto x = query.firstChildElement("x");
			while (!x.isNull()) {
				auto field = x.firstChildElement("field");
				while (!field.isNull()) {
					if (field.attribute("var") == "max-file-size") {
						auto sizeNode = field.firstChildElement("value");
						int foundSizeLimit = sizeNode.text().toInt(&ok);
						if (ok) {
							qDebug() << "Discovered size limit:" << foundSizeLimit;
							sizeLimit = foundSizeLimit;
							break;
						}
					}
					field = field.nextSiblingElement("field");
				}
				x = x.nextSiblingElement("x");
			}
		}
		feature = feature.nextSiblingElement("feature");
	}
	if (sizeLimit > 0) {
		serviceNames.insert(curJid, UploadService(service, sizeLimit));
	}
}

void HttpUploadPlugin::processUploadSlot(const QDomElement& xml) {
	if (xml.firstChildElement("request").attribute("xmlns") == "urn:xmpp:http:upload") {
		QDomElement error = xml.firstChildElement("error");
		if (!error.isNull()) {
			QString errorText = error.firstChildElement("text").text();
			if (!errorText.isNull()) {
				QMessageBox::critical(0, tr("Error requesting slot"), errorText);
				cancelTimeout();
				return;
			}
		}
	}
	QDomElement slot = xml.firstChildElement("slot");
	if (slot.attribute("xmlns") == "urn:xmpp:http:upload") {
		slotTimeout.stop();
		QString put = slot.firstChildElement("put").text();
		qDebug() << "PUT:" << put;
		QString get = slot.firstChildElement("get").text();
		qDebug() << "GET:" << get;
		if (get.isEmpty() || put.isEmpty()) {
			QMessageBox::critical(0, tr("Error requesting slot"),
					tr("Either put or get URL is missing in the server's reply."));
			cancelTimeout();
			return;
		}
		currentUpload.getUrl = get;
		QNetworkRequest req;
		req.setUrl(QUrl(put));
		if (!dataSource) {
			QMessageBox::critical(0, tr("Error uploading"),
					tr("No data to upload, this maybe a result of timeout or other error."));
			cancelTimeout();
			return;
		}
		qint64 size = dataSource->size();
		req.setHeader(QNetworkRequest::ContentLengthHeader, size);
		manager->put(req, dataSource);
	}
}

bool HttpUploadPlugin::incomingStanza(int account, const QDomElement& xml) {
	/*QString s;
	 QTextStream str(&s, QIODevice::WriteOnly);
	 xml.save(str, 2);
	 qDebug() << "DUMP:" << s;*/
	if (xml.nodeName() == "iq" && xml.attribute("type") == "result") {
		QDomElement query = xml.firstChildElement("query");
		if (!query.isNull()) {
			if (query.attribute("xmlns") == "http://jabber.org/protocol/disco#items") {
				processServices(query, account);
			}
			if (query.attribute("xmlns") == "http://jabber.org/protocol/disco#info") {
				processOneService(query, xml.attribute("from"), account);
			}
		} else {
			processUploadSlot(xml);
		}
	}
	return false;
}

void HttpUploadPlugin::uploadComplete(QNetworkReply* reply) {
	bool ok;
	int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(&ok);
	if (ok && statusCode == 201) {
		QString message = QString("<message type=\"%1\" to=\"%2\" id=\"%3\" >"
				"<body>%4</body>"
				"</message>").arg(currentUpload.type).arg(currentUpload.to).arg(getId(currentUpload.account)).arg(
				currentUpload.getUrl);
		stanzaSender->sendStanza(currentUpload.account, message);
	} else {
		QMessageBox::critical(0, tr("Error uploading"),
				tr("Upload error code %1, message: %2").arg(statusCode).arg(
						reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()));
	}
}

void HttpUploadPlugin::timeout() {
	if (dataSource) {
		dataSource->deleteLater();
	}
	QMessageBox::critical(0, tr("Error requesting slot"), tr("Timeout waiting for an upload slot"));
}

#include "httpuploadplugin.moc"
