#pragma once
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUuid>
class RadioServer:public QObject {
public:
    QTcpServer server;
    QList<QUrl> requests;
    QList<QByteArray> agents;
    QJsonArray rows;
    int status=200,delay=0;
    bool malformed=false;
    RadioServer(){
        server.listen(QHostAddress::LocalHost);
        connect(&server,&QTcpServer::newConnection,this,[this](){while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
            connect(socket,&QTcpSocket::readyRead,this,[this,socket](){
                auto data=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",data);
                if(socket->property("handled").toBool()||!data.contains("\r\n\r\n"))return;socket->setProperty("handled",true);
                const auto target=data.split('\n').first().split(' ').value(1);requests.append(QUrl::fromEncoded(target));
                QByteArray agent;for(const auto &line:data.split('\n'))if(line.toLower().startsWith("user-agent:"))agent=line.mid(11).trimmed();agents.append(agent);
                const bool search=target.startsWith("/json/stations/search");
                const auto body=malformed?QByteArray("invalid"):search?QJsonDocument(rows).toJson(QJsonDocument::Compact):QByteArray("{\"ok\":true}");
                const auto response="HTTP/1.1 "+QByteArray::number(status)+" Status\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body;
                QTimer::singleShot(delay,socket,[socket,response](){socket->write(response);socket->disconnectFromHost();});
            });
        }});
    }
    QString url()const{return "http://127.0.0.1:"+QString::number(server.serverPort());}
    int searches()const{int n=0;for(const auto &url:requests)if(url.path()=="/json/stations/search")n++;return n;}
    static QJsonObject row(const QString &name="Test Cumbia",const QString &url="https://stream.example.test/radio",QString id={}){
        if(id.isEmpty())id=QUuid::createUuid().toString(QUuid::WithoutBraces);
        return {{"stationuuid",id},{"name",name},{"url",url},{"url_resolved",url},{"country","Colombia"},{"countrycode","CO"},{"language","spanish"},{"tags","cumbia,vallenato"},{"lastcheckok",1},{"codec","MP3"},{"bitrate",128},{"lastchecktime_iso8601","2026-11-14T01:00:00Z"}};
    }
};
