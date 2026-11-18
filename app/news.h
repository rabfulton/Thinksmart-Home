#pragma once
#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QVariantMap>
#include <QJsonObject>
#include <QQueue>
#include <QSettings>
#include <QSet>
#include <functional>

class NewsEntries : public QAbstractListModel {
    Q_OBJECT
public:
    explicit NewsEntries(QObject *parent=nullptr):QAbstractListModel(parent){}
    int rowCount(const QModelIndex &parent={})const override{return parent.isValid()?0:m_rows.size();}
    QVariant data(const QModelIndex &index,int role)const override;
    QHash<int,QByteArray> roleNames()const override{return {{Qt::UserRole,"entry"}};}
    void update(const QVariantList &rows);
    QVariantList rows()const{return m_rows;}
private:
    QVariantList m_rows;
};

class HeadlinesClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *items READ items CONSTANT)
    Q_PROPERTY(QVariantList sections READ sections CONSTANT)
    Q_PROPERTY(QString section READ section WRITE setSection NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString provider READ provider NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
public:
    explicit HeadlinesClient(const QUrl &base=QUrl("https://news.google.com/"),QObject *parent=nullptr,const QString &settingsKey="news/section",bool selected=true);
    ~HeadlinesClient()override;
    QAbstractItemModel *items(){return &m_items;}
    QVariantList sections()const;
    QString section()const{return m_section;}
    QString title()const;
    QString provider()const;
    void setSelected(bool selected);
    bool selectedFeed()const{return m_selected;}
    void setSection(const QString &section);
    QString status()const;
    QString problem()const{return m_problem;}
    bool busy()const{return m_busy;}
    bool active()const{return m_active;}
    void setActive(bool active);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QVariantMap story(int index)const;
    static QVariantList parseRss(const QByteArray &bytes);
signals:
    void changed();
private:
    void load();
    void fetchPart(int part,quint64 generation);
    QUrl feedUrl(const QString &section)const;
    QNetworkAccessManager m_network;
    NewsEntries m_items;
    QTimer m_poll;
    QSettings m_settings;
    QUrl m_base;
    QString m_section,m_problem,m_settingsKey;
    QStringList m_parts;
    QList<QVariantList> m_results;
    qint64 m_updated=0,m_attempt=0;
    quint64 m_generation=0;
    bool m_active=false,m_busy=false,m_selected=true;
};

class HackerNewsClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *items READ items CONSTANT)
    Q_PROPERTY(QAbstractItemModel *comments READ comments CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY discussionChanged)
    Q_PROPERTY(bool commentsBusy READ commentsBusy NOTIFY discussionChanged)
    Q_PROPERTY(QString commentsProblem READ commentsProblem NOTIFY discussionChanged)
    Q_PROPERTY(bool moreComments READ moreComments NOTIFY discussionChanged)
public:
    explicit HackerNewsClient(const QUrl &base=QUrl("https://hacker-news.firebaseio.com/v0/"),QObject *parent=nullptr);
    ~HackerNewsClient()override;
    QAbstractItemModel *items(){return &m_items;}
    QAbstractItemModel *comments(){return &m_comments;}
    QString status()const;
    QString problem()const{return m_problem;}
    bool busy()const{return m_busy;}
    bool active()const{return m_active;}
    void setActive(bool active);
    void setSelected(bool selected);
    bool selectedFeed()const{return m_feedSelected;}
    QVariantMap selected()const{return m_selected;}
    bool commentsBusy()const{return m_pending>0;}
    QString commentsProblem()const{return m_commentsProblem;}
    bool moreComments()const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openStory(int index);
    Q_INVOKABLE void closeStory();
    Q_INVOKABLE void loadMoreComments();
    Q_INVOKABLE void toggleReplies(const QString &id);
    Q_INVOKABLE void moreReplies(const QString &id);
    Q_INVOKABLE void reloadComments();
signals:
    void changed();
    void discussionChanged();
private:
    struct Job {QUrl url;std::function<void(const QByteArray &)> done;quint64 thread=0;};
    void get(const QString &path,std::function<void(const QByteArray &)> done,quint64 thread=0);
    void pump();
    void loadNodes(const QVariantList &ids,int count);
    void rebuildComments();
    void saveThread();
    QNetworkAccessManager m_network;
    NewsEntries m_items,m_comments;
    QTimer m_poll;
    QUrl m_base;
    QQueue<Job> m_jobs;
    int m_inflight=0,m_feedPending=0,m_pending=0,m_rootCount=0;
    QStringList m_order;
    QHash<QString,QVariantMap> m_stories,m_nodes;
    QHash<QString,int> m_expanded;
    QSet<QString> m_loading;
    QVariantMap m_selected;
    QString m_problem,m_commentsProblem;
    quint64 m_thread=1;
    qint64 m_updated=0,m_attempt=0,m_threadUpdated=0;
    bool m_active=false,m_busy=false,m_feedSelected=true;
};

class NewsSettings:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList sources READ sources CONSTANT)
    Q_PROPERTY(QStringList columns READ columns NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(HeadlinesClient *secondary READ secondary CONSTANT)
public:
    explicit NewsSettings(QObject *parent=nullptr,const QUrl &base=QUrl("https://news.google.com/"));
    static QString path();
    static QString validate(const QStringList &columns);
    QVariantList sources()const;
    QStringList columns()const{return m_columns;}
    QString problem()const{return m_problem;}
    bool active()const{return m_active;}
    void setActive(bool active);
    void setClients(HeadlinesClient *headlines,HackerNewsClient *hackerNews);
    QObject *client(int column);
    HeadlinesClient *secondary(){return &m_second;}
    Q_INVOKABLE bool saveColumns(const QStringList &columns);
signals:
    void changed();
    void activeChanged();
private:
    void configure();
    HeadlinesClient m_second;
    HeadlinesClient *m_first=nullptr;
    HackerNewsClient *m_hackerNews=nullptr;
    QStringList m_columns{"WORLD_FINANCE","HN"};
    QString m_problem;
    bool m_active=false,m_healthy=true;
};
