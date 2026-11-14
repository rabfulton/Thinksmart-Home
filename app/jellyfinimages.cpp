#include "jellyfinimages.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QEventLoop>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QUrlQuery>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
JellyfinImages::JellyfinImages(const QJsonObject &settings):QQuickImageProvider(QQuickImageProvider::Image,QQuickImageProvider::ForceAsynchronousImageLoading),m_settings(settings){
    const auto identity=settings.value("url").toString()+settings.value("accessToken").toString();
    m_cache=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/jellyfin-art-"+QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(),QCryptographicHash::Sha256).toHex().left(16));
    QDir().mkpath(m_cache);QFile::setPermissions(m_cache,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
}
QImage JellyfinImages::requestImage(const QString &id,QSize *size,const QSize &requested){
    if(m_settings.isEmpty())return {};
    auto match=QRegularExpression("^([A-Za-z0-9_-]{1,256})/(Primary)(?:\\?tag=([^&]{0,256}))?$").match(id);if(!match.hasMatch())return {};
    QUrl base(m_settings.value("url").toString()),url=base.resolved(QUrl("Items/"+match.captured(1)+"/Images/Primary"));
    QUrlQuery query;query.addQueryItem("maxWidth",QString::number(qBound(160,requested.width()>0?requested.width():320,640)));query.addQueryItem("maxHeight","960");query.addQueryItem("quality","85");if(!match.captured(3).isEmpty())query.addQueryItem("tag",QUrl::fromPercentEncoding(match.captured(3).toUtf8()));url.setQuery(query);
    QNetworkAccessManager network;auto *cache=new QNetworkDiskCache(&network);cache->setCacheDirectory(m_cache);cache->setMaximumCacheSize(64*1024*1024);network.setCache(cache);
    QNetworkRequest request(url);request.setTransferTimeout(10000);request.setRawHeader("Authorization",("MediaBrowser Token=\""+m_settings.value("accessToken").toString()+"\"").toUtf8());request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,QNetworkRequest::PreferCache);
    auto *reply=network.get(request);QEventLoop loop;QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);QObject::connect(reply,&QNetworkReply::downloadProgress,reply,[reply](qint64 n,qint64 total){if(n>4*1024*1024||total>4*1024*1024)reply->abort();});loop.exec();
    if(reply->error()!=QNetworkReply::NoError||reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()!=200)return {};
    auto bytes=reply->readAll();if(bytes.size()>4*1024*1024)return {};QImage image=QImage::fromData(bytes);if(size)*size=image.size();return image;
}
