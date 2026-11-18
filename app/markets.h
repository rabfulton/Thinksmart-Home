#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QQueue>
#include <QSet>
#include <QVariantList>

class MarketsClient:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList quotes READ quotes NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QVariantList points READ points NOTIFY changed)
    Q_PROPERTY(QVariantList alerts READ alerts NOTIFY changed)
    Q_PROPERTY(QStringList symbols READ symbols NOTIFY changed)
    Q_PROPERTY(QString focus READ focus NOTIFY changed)
    Q_PROPERTY(QString range READ range NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QString notification READ notification NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
public:
    explicit MarketsClient(QObject *parent=nullptr);
    ~MarketsClient() override;
    QVariantList quotes()const;
    QVariantMap selected()const;
    QVariantList points()const;
    QVariantList alerts()const{return m_alerts.toVariantList();}
    QStringList symbols()const{return m_symbols;}
    QString focus()const{return m_focus;}
    QString range()const{return m_range;}
    QString mode()const{return m_mode;}
    QString problem()const{return m_problem;}
    QString notification()const{return m_notification;}
    bool busy()const{return m_running>0||!m_queue.isEmpty();}
    bool active()const{return m_active;}
    void setActive(bool active);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool setSymbols(const QString &text);
    Q_INVOKABLE bool select(const QString &symbol);
    Q_INVOKABLE void setRange(const QString &range);
    Q_INVOKABLE void setMode(const QString &mode);
    Q_INVOKABLE bool addAlert(const QString &symbol,const QString &direction,double price,bool chime=false);
    Q_INVOKABLE bool removeAlert(const QString &id);
    Q_INVOKABLE bool rearmAlert(const QString &id);
    Q_INVOKABLE void dismiss(){m_notification.clear();emit changed();}
    QJsonObject request(const QJsonObject &request);
    QJsonObject snapshot()const;
    static QString canonical(QString symbol);
    static QJsonObject normalize(const QJsonObject &body,const QString &symbol,qint64 now);
    static QJsonArray chartPoints(const QJsonObject &body);
    static QString settingsPath();
    static QString cachePath();
signals:
    void changed();
    void activeChanged();
    void alertTriggered(const QString &text,bool chime);
private:
#ifdef THINKSMART_MARKETS_TESTING
    friend class MarketsTests;
#endif
    struct Job {QString symbol,range;};
    void enqueue(const QString &symbol,const QString &range);
    void pump();
    void accept(const QString &symbol,const QJsonObject &quote,qint64 now);
    bool save();
    void load();
    void saveCache();
    QJsonObject displayQuote(const QString &symbol)const;
    QNetworkAccessManager m_network;
    QTimer m_timer;
    QQueue<Job> m_queue;
    QSet<QString> m_pending;
    QStringList m_symbols{"^GSPC","DX-Y.NYB","NVDA","BTC-USD","ETH-USD"};
    QString m_focus="^GSPC",m_range="1D",m_mode="list",m_problem,m_notification;
    QJsonObject m_quotes,m_charts,m_baselines;
    QJsonArray m_alerts;
    int m_running=0;
    qint64 m_lastRefresh=0,m_retryAfter=0;
    bool m_active=false,m_storeHealthy=true;
};
