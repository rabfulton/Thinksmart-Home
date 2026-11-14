#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QSettings>
#include <QVariantMap>
#include <QVariantList>

class VrmClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap metrics READ metrics NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString lastReport READ lastReport NOTIFY changed)
    Q_PROPERTY(QString lastCheck READ lastCheck NOTIFY changed)
    Q_PROPERTY(QString siteName READ siteName NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool hasData READ hasData NOTIFY changed)
    Q_PROPERTY(bool stale READ stale NOTIFY changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY changed)
    Q_PROPERTY(QVariantList installations READ installations NOTIFY changed)
    Q_PROPERTY(int installationId READ installationId NOTIFY changed)
public:
    explicit VrmClient(QObject *parent=nullptr);
    ~VrmClient() override;
    QVariantMap metrics() const;
    QString status() const;
    QString lastReport() const;
    QString lastCheck() const;
    QString siteName() const { return m_siteName.isEmpty()?QString("Victron VRM"):m_siteName; }
    QString problem() const { return m_problem; }
    bool configured() const { return !m_token.isEmpty(); }
    bool hasData() const { return !m_readings.isEmpty(); }
    bool stale() const;
    bool refreshing() const { return m_busy; }
    QVariantList installations() const { return m_sites; }
    int installationId() const { return m_site; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectInstallation(int id);
    static QJsonObject normalize(const QJsonArray &records);
signals:
    void changed();
private:
    enum Stage { User, Sites, Diagnostics };
    void get(const QString &path,Stage stage);
    void consume(const QJsonObject &response,Stage stage);
    void loadCache();
    void saveCache();
    QString cachePath() const;
    qint64 reportTime() const;
    QNetworkAccessManager m_network;
    QSettings m_settings;
    QTimer m_poll;
    QString m_token,m_siteName,m_problem;
    QJsonObject m_readings;
    QVariantList m_sites;
    int m_site=0,m_configuredSite=0;
    qint64 m_fetchedAt=0,m_retryAfter=0;
    bool m_busy=false,m_cloudOk=false;
};
