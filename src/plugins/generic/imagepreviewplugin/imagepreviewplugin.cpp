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
#include "iconfactoryaccessor.h"
#include "iconfactoryaccessinghost.h"
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
#include <QSpinBox>
#include <QComboBox>

#define constVersion "0.1.0"
#define sizeLimitName "imgpreview-size-limit"
#define previewSizeName "imgpreview-preview-size"

class ImagePreviewPlugin: public QObject,
		public PsiPlugin,
		public IconFactoryAccessor,
		public ActiveTabAccessor,
		public PluginInfoProvider,
		public OptionAccessor,
		public ChatTabAccessor {
Q_OBJECT
#ifdef HAVE_QT5
	Q_PLUGIN_METADATA(IID "com.psi-plus.ImagePreviewPlugin")
#endif
Q_INTERFACES(PsiPlugin
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

	virtual void applyOptions();
	virtual void restoreOptions();
	virtual void setIconFactoryAccessingHost(IconFactoryAccessingHost* host);
	virtual void setActiveTabAccessingHost(ActiveTabAccessingHost* host);
	virtual void setAccountInfoAccessingHost(AccountInfoAccessingHost* host);
	virtual void setPsiAccountControllingHost(PsiAccountControllingHost *host);
	virtual void setOptionAccessingHost(OptionAccessingHost *host);
	virtual void optionChanged(const QString &) {
	}
	virtual QString pluginInfo();
	virtual QPixmap icon() const;
	virtual void setupChatTab(QWidget* widget, int, const QString&) {
		connect(widget, SIGNAL(messageAppended(const QString &, QTextEdit*)),
				this, SLOT(messageAppended(const QString &, QTextEdit*)));
	}
	virtual void setupGCTab(QWidget* widget, int, const QString&) {
		connect(widget, SIGNAL(messageAppended(const QString &, QTextEdit*)),
				this, SLOT(messageAppended(const QString &, QTextEdit*)));
	}

	virtual bool appendingChatMessage(int, const QString&, QString&,
			QDomElement&, bool) {
		return false;
	}

private slots:
	void messageAppended(const QString &, QTextEdit*);
	void imageReply(QNetworkReply* reply);
private:
	IconFactoryAccessingHost* iconHost;
	ActiveTabAccessingHost* activeTab;
	AccountInfoAccessingHost* accInfo;
	PsiAccountControllingHost *psiController;
	OptionAccessingHost *psiOptions;
	bool enabled;
	QHash<QString, int> accounts_;
	QNetworkAccessManager* manager;
	QSet<QString> pending;
	QSet<QString> failed;
	int previewSize = 0;
	QPointer<QSpinBox> sb_previewSize;
	int sizeLimit = 0;
	QPointer<QComboBox> cb_sizeLimit;
};

#ifndef HAVE_QT5
Q_EXPORT_PLUGIN(ImagePreviewPlugin)
#endif

ImagePreviewPlugin::ImagePreviewPlugin() :
		iconHost(0), activeTab(0), accInfo(0), psiController(0), psiOptions(0), enabled(
				false), manager(new QNetworkAccessManager(this)) {
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
	sizeLimit = psiOptions->getPluginOption(sizeLimitName, 1024 * 1024).toInt();
	previewSize = psiOptions->getPluginOption(previewSizeName, 150).toInt();
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
	cb_sizeLimit = new QComboBox(optionsWid);
	cb_sizeLimit->addItem(tr("512 Kb"), 512 * 1024);
	cb_sizeLimit->addItem(tr("1 Mb"), 1024 * 1024);
	cb_sizeLimit->addItem(tr("2 Mb"), 2 * 1024 * 1024);
	cb_sizeLimit->addItem(tr("5 Mb"), 5 * 1024 * 1024);
	cb_sizeLimit->addItem(tr("10 Mb"), 10 * 1024 * 1024);
	vbox->addWidget(new QLabel(tr("Maximum image size")));
	vbox->addWidget(cb_sizeLimit);
	sb_previewSize = new QSpinBox(optionsWid);
	sb_previewSize->setMinimum(1);
	sb_previewSize->setMaximum(65535);
	vbox->addWidget(new QLabel(tr("Image preview size in pixels")));
	vbox->addWidget(sb_previewSize);
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

void ImagePreviewPlugin::setActiveTabAccessingHost(
		ActiveTabAccessingHost* host) {
	activeTab = host;
}

QString ImagePreviewPlugin::pluginInfo() {
	return tr("Author: ") + "rkfg\n\n"
			+ trUtf8("This plugin shows the preview image for an image URL.\n");
}

QPixmap ImagePreviewPlugin::icon() const {
	return QPixmap(":/imagepreviewplugin/imagepreviewplugin.gif");
}

void ImagePreviewPlugin::messageAppended(const QString &, QTextEdit* te_log) {
	if (!enabled) {
		return;
	}
	auto cur = te_log->textCursor();
	te_log->moveCursor(QTextCursor::End, QTextCursor::MoveAnchor);
	te_log->moveCursor(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
	QTextCursor found = te_log->textCursor();
	while (!(found = te_log->document()->find(QRegExp("https?://\\S*"), found)).isNull()) {
		auto url = found.selectedText();
		qDebug() << "URL FOUND:" << url;
		if (!pending.contains(url)) {
			pending.insert(url);
			QNetworkRequest req;
			req.setUrl(QUrl(url));
			req.setOriginatingObject(te_log);
			manager->head(req);
		}
	};
	te_log->setTextCursor(cur);
}

void ImagePreviewPlugin::imageReply(QNetworkReply* reply) {
	bool ok;
	int size = 0;
	QString contentType;
	QStringList allowedTypes = { "image/jpeg", "image/png", "image/gif" };
	QTextEdit* te_log = qobject_cast<QTextEdit*>(
			reply->request().originatingObject());
	QUrl url = reply->request().url();
	QString urlStr = url.toEncoded();
	switch (reply->operation()) {
	case QNetworkAccessManager::HeadOperation:
		size = reply->header(QNetworkRequest::ContentLengthHeader).toInt(&ok);
		contentType =
				reply->header(QNetworkRequest::ContentTypeHeader).toString();
		qDebug() << "URL:" << url << "RESULT:" << reply->error() << "SIZE:"
				<< size << "Content-type:" << contentType;
		if (ok && allowedTypes.contains(contentType, Qt::CaseInsensitive)
				&& size < sizeLimit) {
			manager->get(reply->request());
		} else {
			pending.remove(urlStr);
			failed.insert(urlStr);
		}
		break;
	case QNetworkAccessManager::GetOperation:
		try {
			QImageReader imageReader(reply);
			auto image = imageReader.read().scaled(previewSize, previewSize,
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
				QString html = cur.selection().toHtml();
				html.replace(
						QRegExp("(<a href=\"[^\"]*\">)(.*)(</a>)"),
						QString("\\1<img src='%1'/>\\3").arg(urlStr));
				cur.insertHtml(html);
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

void ImagePreviewPlugin::applyOptions() {
	psiOptions->setPluginOption(previewSizeName,
			previewSize = sb_previewSize->value());
	psiOptions->setPluginOption(sizeLimitName,
			sizeLimit =
					cb_sizeLimit->itemData(cb_sizeLimit->currentIndex()).toInt());
}

void ImagePreviewPlugin::restoreOptions() {
	sb_previewSize->setValue(previewSize);
	cb_sizeLimit->setCurrentIndex(cb_sizeLimit->findData(sizeLimit));
}

#include "imagepreviewplugin.moc"
