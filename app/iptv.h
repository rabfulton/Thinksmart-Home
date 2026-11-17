#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <functional>
#include "streamlistmodel.h"

class IptvClient:public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *channelModel READ channelModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *savedModel READ savedModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *suggestedModel READ suggestedModel CONSTANT)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool preparing READ preparing NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(QVariantList favourites READ favourites NOTIFY changed)
    Q_PROPERTY(QVariantList suggestions READ suggestions NOTIFY changed)
    Q_PROPERTY(QVariantMap lastChannel READ lastChannel NOTIFY changed)
    Q_PROPERTY(QVariantList countries READ countries NOTIFY directoryChanged)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY directoryChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
public:
    using Done=std::function<void(QJsonObject)>;
    explicit IptvClient(QObject *parent=nullptr);
    ~IptvClient()override;
    QAbstractItemModel *channelModel(){return &m_channelModel;}
    QAbstractItemModel *savedModel(){return &m_savedModel;}
    QAbstractItemModel *suggestedModel(){return &m_suggestedModel;}
    static bool softwareDecode(const QUrl &url);
    bool active()const{return m_active;}
    void setActive(bool value);
    bool busy()const{return m_busy;}
    bool preparing()const{return !m_playReply.isNull();}
    Q_INVOKABLE void cancelPlayback();
    QString problem()const{return m_problem;}
    QString status()const;
    QVariantList channels()const;
    QVariantList favourites()const;
    QVariantList suggestions()const{return matching({{"suggested",true}},0,30).toVariantList();}
    QVariantMap lastChannel()const;
    QVariantList countries()const;
    QVariantList categories()const;
    bool hasMore()const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void filter(const QString &search,const QString &country,const QString &category);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE bool play(const QString &id);
    Q_INVOKABLE bool favourite(const QString &id,bool saved);
    Q_INVOKABLE bool addCustom(const QString &name,const QString &url);
    QJsonObject request(const QJsonObject &request);
    void search(const QJsonObject &request,Done done);
    bool knows(const QString &id)const{return !resolve(id).isEmpty();}
    static QJsonArray normalize(const QJsonArray &channels,const QJsonArray &streams,const QJsonArray &preferred={});
    static bool validUrl(const QString &value);
    static QUrl selectHlsVariant(const QByteArray &playlist,const QUrl &base);
    static QString storagePath();
signals:
    void changed();
    void directoryChanged();
    void playRequested(const QUrl &url,const QString &name,bool software);
private:
    static QJsonObject summary(const QJsonObject &row,bool saved=false);
    QJsonObject resolve(const QString &id)const;
    QJsonArray matching(const QJsonObject &filters,int offset,int limit)const;
    void fetch(const QString &name,std::function<void(QJsonArray)> done);
    void finish(bool success);
    bool save(const QJsonArray &favourites,const QJsonObject &last);
    void load();
    void refreshModels();
    void cancel();
    void preparePlaylist(const QJsonObject &row);
    QString cachePath()const;
    StreamListModel m_channelModel,m_savedModel,m_suggestedModel;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply,m_playReply;
    QJsonArray m_rows,m_saved,m_seed;
    QJsonObject m_last,m_filters;
    struct Query {QJsonObject request;Done done;};
    QList<Query> m_pending;
    QString m_problem;
    bool m_active=false,m_busy=false,m_storeHealthy=true;
    int m_limit=30,m_generation=0,m_playGeneration=0;
    qint64 m_updated=0;
};
