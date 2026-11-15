#include "wifi.h"
#include <QJsonDocument>
#include <QJsonArray>

WifiController::WifiController(QObject *parent):QObject(parent){
    m_poll.setInterval(3000);connect(&m_poll,&QTimer::timeout,this,&WifiController::refresh);
    m_deadline.setSingleShot(true);m_deadline.setInterval(15000);connect(&m_deadline,&QTimer::timeout,this,&WifiController::fail);
    connect(&m_socket,&QLocalSocket::connected,this,[this](){m_socket.write(m_pending);m_pending.fill('\0');m_pending.clear();});
    connect(&m_socket,&QLocalSocket::errorOccurred,this,[this](){if(m_busy)fail();});
    connect(&m_socket,&QLocalSocket::readyRead,this,[this](){
        m_reply+=m_socket.readAll();if(m_reply.size()>256*1024){fail();return;}if(!m_reply.contains('\n'))return;
        QJsonParseError parse;const auto doc=QJsonDocument::fromJson(m_reply.left(m_reply.indexOf('\n')),&parse);
        if(parse.error!=QJsonParseError::NoError||!doc.isObject()){fail();return;}
        m_deadline.stop();const auto r=doc.object();const auto action=m_action;m_busy=false;m_socket.abort();m_reply.clear();
        if(r.value("ok").toBool()){
            m_problem.clear();if(action=="status"){
                const auto networks=r.value("networks").toArray().toVariantList(),saved=r.value("saved").toArray().toVariantList();
                if(networks!=m_networks||saved!=m_saved){m_networks=networks;m_saved=saved;emit modelsChanged();}
                m_status=r.value("status").toString();m_addresses=r.value("addresses").toString();m_connecting=r.value("connecting").toBool();
            }else{emit completed(true,r.value("message").toString());if(m_active)QTimer::singleShot(150,this,&WifiController::refresh);}
        }else{m_problem=r.value("error").toString("Wi-Fi request failed.");if(action!="status")emit completed(false,m_problem);}
        emit changed();
        if(m_active&&!m_queued.isEmpty()){const auto next=m_queued;m_queued={};request(next);}
    });
}
void WifiController::fail(){
    m_deadline.stop();m_busy=false;m_socket.abort();m_pending.fill('\0');m_pending.clear();m_reply.clear();m_queued={};
    m_problem="Wi-Fi settings are unavailable. Check the network helper service.";emit changed();emit completed(false,m_problem);
}
void WifiController::setActive(bool active){
    if(active==m_active)return;m_active=active;
    if(active){m_poll.start();refresh();}else{m_poll.stop();m_queued={};}emit changed();
}
void WifiController::request(const QJsonObject &r){
    if(m_busy)return;m_action=r.value("action").toString();m_pending=QJsonDocument(r).toJson(QJsonDocument::Compact)+'\n';
    m_reply.clear();m_busy=true;m_deadline.start();m_socket.connectToServer(qEnvironmentVariable("THINKSMART_WIFI_SOCKET","/run/thinksmart-network/control.sock"));emit changed();
}
void WifiController::refresh(){if(m_active&&!m_busy)request({{"action","status"}});}
void WifiController::scan(){if(m_active&&!m_busy)request({{"action","scan"}});}
void WifiController::perform(const QString &action,const QVariantMap &arguments){
    if(!m_active||!QStringList{"connect","connectSaved","preferences","forget","disconnect"}.contains(action))return;
    auto r=QJsonObject::fromVariantMap(arguments);r.insert("action",action);
    if(m_busy){
        if(m_action=="status"&&m_queued.isEmpty())m_queued=r;
        else emit completed(false,"Another Wi-Fi request is still in progress. Try again shortly.");
        return;
    }
    request(r);
}
