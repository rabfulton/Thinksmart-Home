#pragma once
#include <QQuickImageProvider>
#include <QJsonObject>
class JellyfinImages:public QQuickImageProvider {
public:
    explicit JellyfinImages(const QJsonObject &settings);
    QImage requestImage(const QString &id,QSize *size,const QSize &requested)override;
private:
    QJsonObject m_settings;
    QString m_cache;
};
