#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QVariantList>
#include <QJsonObject>
#include <QJsonArray>
#include <functional>
#include "radiodirectory.h"
class RadioCatalog : public QObject {
    Q_OBJECT
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
    void interruptPlayback();
    void playRequested(const QString &id,const QUrl &url,const QString &name,const QString &subtitle);
private:
    void request(const QUrl &url,std::function<void(const QByteArray &)> done,const QJsonObject &post={},const QByteArray &bearer={});
    void probe(const QJsonArray &events,int index,const QString &house);
    void resolve(const QJsonObject &event,const QString &state,const QString &house);
    void fail(const QString &message);
    QJsonArray m_stations;
    QNetworkAccessManager m_network;
    RadioDirectory m_directory;
    QString m_selected,m_status="Choose a station or explore a genre";
    bool m_busy=false;
    int m_serial=0;
};
