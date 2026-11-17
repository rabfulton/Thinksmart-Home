#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QVariantList>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include "radiodirectory.h"
#include "streamlistmodel.h"
class RadioCatalog : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *favouriteModel READ favouriteModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *resultModel READ resultModel CONSTANT)
    Q_PROPERTY(bool searchBusy READ searchBusy NOTIFY searchChanged)
    Q_PROPERTY(QString searchProblem READ searchProblem NOTIFY searchChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY searchChanged)
    Q_PROPERTY(int favouriteCount READ favouriteCount NOTIFY searchChanged)
    Q_PROPERTY(QVariantList stations READ stations CONSTANT)
    Q_PROPERTY(QString selectedId READ selectedId NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit RadioCatalog(QObject *parent=nullptr);
    ~RadioCatalog() override;
    QVariantList stations() const;
    QString selectedId() const { return m_selected; }
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    QAbstractItemModel *favouriteModel(){return &m_favouriteModel;}
    QAbstractItemModel *resultModel(){return &m_resultModel;}
    bool searchBusy()const{return m_searchBusy;}
    QString searchProblem()const{return m_searchProblem;}
    bool hasMore()const{return m_hasMore;}
    int favouriteCount()const{return m_favourites.size();}
    QJsonArray favourites()const;
    Q_INVOKABLE bool favourite(const QString &id,bool saved);
    Q_INVOKABLE void searchStations(const QString &text,const QString &tag={});
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void cancelSearch();
    static QString favouritesPath();
    QJsonArray list(const QString &genre={},const QString &search={}) const;
    QJsonObject snapshot() const;
    bool knows(const QString &id)const;
    void search(const QJsonObject &request,RadioDirectory::Done done){m_directory.search(request,std::move(done));}
    Q_INVOKABLE bool play(const QString &id);
    Q_INVOKABLE bool random(const QString &genre);
    void cancel();
    static QJsonArray parliamentEvents(const QString &html,const QString &house);
signals:
    void changed();
    void searchChanged();
    void interruptPlayback();
    void playRequested(const QString &id,const QUrl &url,const QString &name,const QString &subtitle);
private:
    void request(const QUrl &url,std::function<void(const QByteArray &)> done,const QJsonObject &post={},const QByteArray &bearer={});
    void probe(const QJsonArray &events,int index,const QString &house);
    void resolve(const QJsonObject &event,const QString &state,const QString &house);
    void fail(const QString &message);
    void loadFavourites();
    void updateModels();
    void fetchStations(bool append);
    QJsonObject station(const QString &id)const;
    static QJsonObject summary(QJsonObject row,bool saved);
    QJsonArray m_stations,m_favourites,m_results;
    StreamListModel m_favouriteModel,m_resultModel;
    QString m_searchText,m_searchTag,m_searchProblem;
    int m_searchSerial=0,m_searchOffset=0;
    bool m_searchBusy=false,m_hasMore=false,m_storeHealthy=true;

    QNetworkAccessManager m_network;
    RadioDirectory m_directory;
    QString m_selected,m_status="Choose a station or explore a genre";
    bool m_busy=false;
    int m_serial=0;
};
