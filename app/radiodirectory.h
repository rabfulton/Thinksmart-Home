#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QHash>
#include <QUrl>
#include <functional>
#include <memory>

// Small, asynchronous Radio Browser client. Search never changes playback.
class RadioDirectory:public QObject {
public:
    using Done=std::function<void(QJsonObject)>;
    explicit RadioDirectory(QObject *parent=nullptr);
    ~RadioDirectory()override;
    void search(const QJsonObject &filters,Done done);
    QJsonObject station(const QString &id)const;
    void clicked(const QString &id);
    static QJsonObject filters(const QJsonObject &request,QString &error);
    static QJsonArray normalize(const QJsonArray &rows,int limit=5);
private:
    struct Search {QString key;QJsonObject filters,stale;QList<Done> callbacks;};
    void servers(std::function<void()> ready);
    void fetch(const std::shared_ptr<Search> &query,int attempt);
    void finish(const std::shared_ptr<Search> &query,QJsonObject result);
    void remember(const QString &key,const QJsonObject &entry);
    void rebuildStations();
    QString cachePath()const;
    QNetworkAccessManager m_network;
    QList<QUrl> m_servers;
    QList<std::function<void()>> m_waitingServers;
    QHash<QString,std::shared_ptr<Search>> m_pending;
    QHash<QString,QJsonObject> m_stations;
    QHash<QString,qint64> m_stationTimes;
    QJsonObject m_cache;
    bool m_resolving=false;
};
