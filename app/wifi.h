#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QVariantList>
#include <QJsonObject>

class WifiController:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool connecting READ connecting NOTIFY changed)
    Q_PROPERTY(QVariantList networks READ networks NOTIFY modelsChanged)
    Q_PROPERTY(QVariantList saved READ saved NOTIFY modelsChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString addresses READ addresses NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
public:
    explicit WifiController(QObject *parent=nullptr);
    bool active()const{return m_active;}
    void setActive(bool active);
    bool busy()const{return m_busy;}
    bool connecting()const{return m_connecting;}
    QVariantList networks()const{return m_networks;}
    QVariantList saved()const{return m_saved;}
    QString status()const{return m_status;}
    QString addresses()const{return m_addresses;}
    QString problem()const{return m_problem;}
    Q_INVOKABLE void scan();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void perform(const QString &action,const QVariantMap &arguments={});
    void show(){emit openRequested();}
signals:
    void openRequested();
    void changed();
    void modelsChanged();
    void completed(bool ok,const QString &message);
private:
    void request(const QJsonObject &request);
    void fail();
    QLocalSocket m_socket;
    QTimer m_poll,m_deadline;
    QByteArray m_pending,m_reply;
    QJsonObject m_queued;
    QString m_action,m_problem,m_status="Open Wi-Fi settings to scan",m_addresses;
    QVariantList m_networks,m_saved;
    bool m_active=false,m_busy=false,m_connecting=false;
};
