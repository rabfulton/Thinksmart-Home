#pragma once
#include "streamlistmodel.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <QQueue>
#include <QMap>
#include <QTimer>
#include <functional>

class AudiobooksClient:public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *items READ items CONSTANT)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY changed)
    Q_PROPERTY(QVariantList chapters READ chapters NOTIFY changed)
    Q_PROPERTY(QString tab READ tab NOTIFY changed)
    Q_PROPERTY(QString genre READ genre WRITE setGenre NOTIFY changed)
    Q_PROPERTY(QStringList genres READ genres NOTIFY changed)
    Q_PROPERTY(QVariantList periods READ periods NOTIFY changed)
    Q_PROPERTY(QString period READ period WRITE setPeriod NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool preparing READ preparing NOTIFY changed)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    using Done=std::function<void(QJsonObject)>;
    explicit AudiobooksClient(QObject *parent=nullptr);
    ~AudiobooksClient()override;
    QAbstractItemModel *items(){return &m_items;}
    QVariantMap current()const;
    QVariantMap detail()const;
    QVariantList chapters()const;
    QString tab()const{return m_tab;}
    QString genre()const{return m_genre;}
    QStringList genres()const;
    QVariantList periods()const{return m_periods.toVariantList();}
    QString period()const{return m_period;}
    QString status()const;
    QString problem()const{return m_problem;}
    bool busy()const{return m_running>0||!m_jobs.isEmpty();}
    bool preparing()const{return m_preparing;}
    bool hasMore()const{return m_hasMore;}
    bool active()const{return m_active;}
    void setActive(bool active);
    Q_INVOKABLE void setTab(const QString &tab);
    void setGenre(const QString &genre);
    void setPeriod(const QString &period);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void search(const QString &text,bool author=false);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE bool inspect(const QString &id);
    Q_INVOKABLE void closeDetails();
    Q_INVOKABLE bool play(const QString &id,bool restart=false);
    Q_INVOKABLE bool resume();
    bool knows(const QString &id)const{return !known(id).isEmpty();}
    Q_INVOKABLE bool playChapter(int index);
    Q_INVOKABLE bool saveBook(const QString &id);
    Q_INVOKABLE bool removeBook(const QString &id);
    bool advance(int delta);
    void report(const QString &key,const QString &event,double position,bool paused);
    void completed(const QString &key);
    void cancelPending();
    QJsonObject request(const QJsonObject &request);
    void searchRequest(const QJsonObject &request,Done done);
    static QJsonObject normalize(const QJsonObject &book);
    static QJsonArray parseTop(const QByteArray &bytes,QString *generated=nullptr);
    static QJsonObject parsePicks(const QByteArray &bytes);
    static QJsonObject initialBook();
    static QString libraryPath();
    static QString cacheDirectory();
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
    static QString proxyUrl();
#endif
    static QStringList playerOptions(const QString &source);
signals:
    void changed();
    void activeChanged();
    void interruptPlayback();
    void playRequested(const QUrl &url,const QString &title,const QString &subtitle,const QString &key,double start);
private:
    struct Job {QUrl url;std::function<void(QByteArray)> done;};
    void get(const QUrl &url,std::function<void(QByteArray)> done);
    void pump();
    void resolve(const QString &id,Done done);
    void fetchBook(const QString &id,Done done);
    void loadTop(bool force=false);
    void loadPicks(bool force=false);
    void runSearch(bool append,Done done={});
    bool beginBook(const QJsonObject &book,bool restart,int chapter=-1);
    bool commit(const QMap<QString,QJsonObject> &books,const QString &current);
    void load();
    void cacheDiscovery();
    void remember(const QJsonObject &book);
    void loadCover();
    void updateModel();
    QJsonObject known(const QString &id)const;
    QJsonObject summary(QJsonObject book)const;
    QJsonObject snapshot()const;
    QNetworkAccessManager m_network;
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
    QNetworkAccessManager m_archiveNetwork;
#endif
    StreamListModel m_items;
    QQueue<Job> m_jobs;
    QMap<QString,QJsonObject> m_books,m_known,m_metadata;
    QMap<QString,QList<Done>> m_resolving;
    QJsonArray m_top,m_picks,m_periods,m_search;
    QJsonObject m_detail,m_playing;
    QString m_current="lv:753",m_tab="mine",m_genre,m_period,m_topGenerated,m_problem,m_query,m_key,m_requestError;
    int m_running=0,m_offset=0;
    quint64 m_inspectSerial=0,m_playSerial=0,m_searchSerial=0;
    qint64 m_topAt=0,m_picksAt=0,m_topAttempt=0,m_picksAttempt=0,m_retryAfter=0;
    bool m_started=false,m_active=false,m_preparing=false,m_hasMore=false,m_byAuthor=false,m_storeHealthy=true,m_bundledTop=false;
};
