#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QHash>
#include <QNetworkReply>
#include "streamlistmodel.h"

class TvArchiveClient:public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *series READ series CONSTANT)
    Q_PROPERTY(QAbstractItemModel *episodes READ episodes CONSTANT)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(bool autoplay READ autoplay WRITE setAutoplay NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY changed)
    Q_PROPERTY(QString season READ season WRITE setSeason NOTIFY changed)
    Q_PROPERTY(QStringList seasons READ seasons NOTIFY changed)
public:
    explicit TvArchiveClient(QObject *parent=nullptr);
    ~TvArchiveClient()override;
    QAbstractItemModel *series(){return &m_series;}
    QAbstractItemModel *episodes(){return &m_episodes;}
    QVariantMap selected()const;
    QVariantMap current()const;
    bool autoplay()const{return m_state.value("autoplay").toBool();}
    void setAutoplay(bool value);
    bool active()const{return m_active;}
    void setActive(bool value);
    bool busy()const{return !m_reply.isNull()||m_parsing;}
    bool ready()const{return !m_catalogue.isEmpty();}
    QString problem()const{return m_problem;}
    QString status()const;
    QString query()const{return m_query;}
    void setQuery(const QString &value);
    QString season()const{return m_season;}
    void setSeason(const QString &value);
    QStringList seasons()const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void selectSeries(const QString &id);
    Q_INVOKABLE bool play(const QString &id,bool restart=false);
    Q_INVOKABLE bool resume(const QString &seriesId=QString());
    Q_INVOKABLE bool random(const QString &seriesId=QString());
    bool knows(const QString &id)const;
    void report(const QString &key,const QString &event,double position,double duration,bool paused);
    void completed(const QString &key);
    QJsonObject request(const QJsonObject &request);
    static QJsonArray parseCatalogue(const QJsonObject &metadata);
    static QString storagePath();
    static QString cachePath();
    static QString proxyUrl();
    static QString collection(){return "classic-british-tv-pack";}
signals:
    void changed();
    void interruptPlayback();
    void playRequested(const QUrl &url,const QString &title,const QString &key,double position);
private:
    void updateModels();
    void indexCatalogue();
    void load();
    bool commit(const QJsonObject &value);
    bool start(const QString &id,bool restart,bool shuffle,const QString &scope={});
    QJsonObject episode(const QString &id)const;
    QJsonObject summary(const QJsonObject &episode)const;
    QString continueId(const QString &seriesId)const;
    QString nextId(const QString &id)const;
    StreamListModel m_series,m_episodes;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QJsonArray m_catalogue;
    QHash<QString,QJsonObject> m_byId;
    QHash<QString,QString> m_firstBySeries;
    QJsonObject m_state;
    QString m_selected,m_query,m_season,m_problem,m_playing,m_randomScope;
    bool m_active=false,m_healthy=true,m_started=false,m_shuffle=false,m_parsing=false;
    qint64 m_updated=0,m_attempt=0;
};
