#pragma once
#include <QObject>
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QVariantList>
#include <QSettings>
#include <functional>

class LibraryItemsModel:public QAbstractListModel {
public:
    int rowCount(const QModelIndex &parent={})const override{return parent.isValid()?0:m_items.size();}
    QVariant data(const QModelIndex &index,int role)const override{return index.isValid()&&index.row()<m_items.size()&&role==Qt::UserRole+1?m_items.at(index.row()):QVariant{};}
    QHash<int,QByteArray> roleNames()const override{return {{Qt::UserRole+1,"entry"}};}
    void replace(const QVariantList &items);
    void append(const QVariantList &items);
private:
    QVariantList m_items;
};
class MediaLibrary:public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *itemModel READ itemModel CONSTANT)
    Q_PROPERTY(int decade READ decade WRITE setDecade NOTIFY changed)
    Q_PROPERTY(QVariantList genres READ genres NOTIFY changed)
    Q_PROPERTY(QVariantList libraries READ libraries NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QString genre READ genre WRITE setGenre NOTIFY changed)
    Q_PROPERTY(QString section READ section WRITE setSection NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(bool configured READ configured CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool cached READ cached NOTIFY changed)
    Q_PROPERTY(bool canBack READ canBack NOTIFY changed)
    Q_PROPERTY(bool canNext READ canNext NOTIFY changed)
    Q_PROPERTY(bool canPrevious READ canPrevious NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
public:
    explicit MediaLibrary(const QString &provider,QObject *parent=nullptr);
    ~MediaLibrary()override;
    QAbstractItemModel *itemModel(){return &m_itemModel;}
    int decade()const{return m_decade;}
    void setDecade(int decade);
    Q_INVOKABLE void loadMore();
    QVariantList genres()const{return m_genres;}
    QVariantList libraries()const{return m_libraries;}
    QVariantMap detail()const{return m_detail;}
    QString genre()const{return m_genre;}
    QString section()const{return m_section;}
    int total()const{return m_total;}
    void setGenre(const QString &genre);
    void setSection(const QString &section);
    Q_INVOKABLE void setLibrary(const QString &id);
    Q_INVOKABLE void inspect(int index);
    Q_INVOKABLE void closeDetails();
    Q_INVOKABLE void playSelection(bool restart=false);
    QJsonObject imageSettings()const;
    QJsonObject snapshot()const;
    bool configured()const{return m_configured;}
    bool busy()const{return m_busy;}
    bool cached()const{return m_cached;}
    bool canBack()const{return !m_history.isEmpty();}
    bool canNext()const{return (m_provider!="jellyfin"||m_problem.isEmpty())&&!(m_provider=="navidrome"&&!m_parent.isEmpty())&&(m_provider=="jellyfin"&&m_total>=0?m_items.size()<m_total:m_items.size()>=60);}
    bool canPrevious()const{return !(m_provider=="navidrome"&&!m_parent.isEmpty())&&m_offset>0;}
    QString title()const{return m_title;}
    QString status()const;
    QString problem()const{return m_problem;}
    QVariantList items()const{return m_items;}
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void open(int index);
    Q_INVOKABLE void back();
    Q_INVOKABLE void page(int delta);
    Q_INVOKABLE void search(const QString &text);
    void advance(int delta);
    void replay();
    void report(const QString &key,const QString &event,double position,bool paused);
    static QUrl authenticatedSubsonicUrl(const QUrl &base,const QJsonObject &credentials,const QString &method,QUrlQuery query={});
    static bool sameOrigin(const QUrl &a,const QUrl &b);
    static QJsonObject videoProfile();
signals:
    void changed();
    void playRequested(const QString &source,const QUrl &url,const QString &title,const QString &subtitle,const QStringList &headers,bool video,const QString &key,double start);
private:
    void browse();
    void loadFilters();
    void request(const QUrl &url,const QJsonObject &body,bool post,std::function<void(const QJsonObject &)> done,bool quiet=false);
    void play(const QVariantMap &item);
    void storeItems(const QVariantList &items);
    QString cachePath()const;
    void loadCache();
    QVariantMap route()const;
    void restoreRoute(const QVariantMap &route);
    QUrl endpoint(const QString &path,QUrlQuery query={})const;
    QByteArray authorization(bool player=false)const;
    QString m_provider,m_title,m_problem,m_parent,m_search,m_deviceId;
    QJsonObject m_config;
    QUrl m_base;
    QNetworkAccessManager m_network;
    QSettings m_settings;
    LibraryItemsModel m_itemModel;
    int m_decade=0;bool m_loadingMore=false;
    QVariantList m_items,m_history,m_queue,m_genres,m_libraries;
    QVariantMap m_detail;
    QString m_genre,m_section="all",m_library;
    int m_total=-1;bool m_filtersRequested=false;
    QVariantMap m_lastItem;
    QMap<QString,QJsonObject> m_sessions;
    QString m_currentSession;
    int m_offset=0,m_queueIndex=-1;
    bool m_configured=false,m_busy=false,m_cached=false;
    qint64 m_savedAt=0;
};
