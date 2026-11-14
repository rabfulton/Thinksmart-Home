#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QJsonObject>
#include <QVariantMap>
#include <QVariantList>
#include "deviceconfig.h"

class WeatherClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantList hourly READ hourly NOTIFY changed)
    Q_PROPERTY(QVariantList daily READ daily NOTIFY changed)
    Q_PROPERTY(QString location READ location CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool stale READ stale NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit WeatherClient(QObject *parent=nullptr);
    ~WeatherClient() override;
    QVariantMap current() const;
    QVariantList hourly() const;
    QVariantList daily() const;
    QString location() const { return DeviceConfig::weather().value("name").toString(); }
    QString status() const;
    QString problem() const { return m_problem; }
    bool stale() const;
    bool busy() const { return m_busy; }
    QJsonObject snapshot() const;
    Q_INVOKABLE void refresh();
    static QJsonObject normalize(const QJsonObject &response,qint64 now);
    static QString condition(int code);
    static QUrl forecastUrl();
signals:
    void changed();
private:
    QString cachePath() const;
    QNetworkAccessManager m_network;
    QTimer m_poll;
    QJsonObject m_data;
    QString m_problem;
    qint64 m_attempt=0,m_retryAfter=0;
    bool m_busy=false;
};
