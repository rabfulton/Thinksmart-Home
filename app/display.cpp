#include "display.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QEvent>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QDebug>

ScreenController::ScreenController(QObject *parent):QObject(parent) {
    m_brightness=qBound(35,m_settings.value("display/brightness",50).toInt(),100);
    m_dimSeconds=qBound(0,m_settings.value("display/dimSeconds",60).toInt(),3600);
    m_sleepSeconds=qBound(0,m_settings.value("display/sleepSeconds",300).toInt(),7200);
    m_idle.start();
    m_flush.setSingleShot(true);m_flush.setInterval(35);
    connect(&m_flush,&QTimer::timeout,this,&ScreenController::flush);
    m_deadline.setSingleShot(true);m_deadline.setInterval(3000);
    auto fail=[this](const QString &message) {
        if(!m_busy)return;
        m_busy=false;m_deadline.stop();m_socket.abort();m_applied=-1;
        m_error=message;m_powerPending=false;m_pendingAction.clear();m_powerMessage.clear();
        // Keep the UI available when a screen-control request fails.
        m_sleeping=false;m_dimmed=false;m_target=m_brightness;m_idle.restart();
        emit changed();m_flush.start(1000);
    };
    connect(&m_deadline,&QTimer::timeout,this,[fail](){fail("Screen controls timed out");});
    connect(&m_socket,&QLocalSocket::errorOccurred,this,[fail](){fail("Screen controls are unavailable");});
    connect(&m_socket,&QLocalSocket::connected,this,[this](){
        const QString command=m_sentAction.isEmpty()?"brightness "+QString::number(m_sentBrightness):m_sentAction;
        m_socket.write(command.toUtf8()+'\n');
    });
    connect(&m_socket,&QLocalSocket::readyRead,this,[this,fail](){
        m_reply+=m_socket.readAll();
        if(m_reply.size()>4096) {fail("Invalid screen-control response");return;}
        if(!m_reply.contains('\n'))return;
        QJsonParseError parse;
        const auto result=QJsonDocument::fromJson(m_reply.left(m_reply.indexOf('\n')),&parse).object();
        if(parse.error!=QJsonParseError::NoError||!result.value("ok").toBool()) {fail(result.value("error").toString("Screen control failed"));return;}
        m_busy=false;m_deadline.stop();m_socket.abort();
        if(m_sentAction.isEmpty())m_applied=m_sentBrightness;
        else m_pendingAction.clear();
        if(!m_error.isEmpty()){m_error.clear();emit changed();}
        m_flush.start();
    });
    connect(&m_socket,&QLocalSocket::disconnected,this,[fail](){fail("Screen controls disconnected");});
    m_tick.setInterval(250);
    connect(&m_tick,&QTimer::timeout,this,[this](){
        if(m_sleeping||m_powerPending)return;
        const qint64 elapsed=m_idle.elapsed();
        if(m_sleepSeconds>0&&elapsed>=m_sleepSeconds*1000LL) {sleep();return;}
        const bool dim=m_dimSeconds>0&&elapsed>=m_dimSeconds*1000LL;
        if(dim!=m_dimmed){m_dimmed=dim;updateTarget();emit changed();}
    });
    m_tick.start();
    qApp->installEventFilter(this);
    updateTarget();
}
ScreenController::~ScreenController(){
    qApp->removeEventFilter(this);
    m_socket.disconnect(this);
    m_socket.abort();
}
QJsonObject ScreenController::status() const {
    return {{"sleeping",m_sleeping},{"dimmed",m_dimmed},{"brightness",m_brightness},
            {"dimSeconds",m_dimSeconds},{"sleepSeconds",m_sleepSeconds},{"powerPending",m_powerPending},
            {"backlightPending",m_busy||m_applied!=m_target},{"error",m_error}};
}
void ScreenController::updateTarget() {
    m_target=m_sleeping?0:(m_dimmed?qMax(35,m_brightness/2):m_brightness);
    m_flush.start();
}
void ScreenController::flush() {
    if(m_busy)return;
    if(m_applied==m_target&&m_pendingAction.isEmpty())return;
    m_sentAction=m_pendingAction;
    m_sentBrightness=m_target;
    m_reply.clear();m_busy=true;m_socket.abort();
    m_socket.connectToServer(qEnvironmentVariable("THINKSMART_POWER_SOCKET","/run/thinksmart-power/control.sock"));
    m_deadline.start();
}
void ScreenController::activity() {
    if(m_sleeping||m_powerPending)return;
    m_idle.restart();
    if(m_dimmed){m_dimmed=false;updateTarget();emit changed();}
}
void ScreenController::wake() {
    if(m_powerPending)return;
    const bool changedState=m_sleeping||m_dimmed;
    m_sleeping=false;m_dimmed=false;m_idle.restart();updateTarget();
    if(changedState)emit changed();
}
void ScreenController::sleep() {
    if(m_powerPending)return;
    m_sleeping=true;m_dimmed=false;updateTarget();emit changed();
}
void ScreenController::setBrightness(int percent) {
    m_brightness=qBound(35,percent,100);
    m_settings.setValue("display/brightness",m_brightness);
    activity();updateTarget();emit changed();
}
bool ScreenController::setIdleTimers(int dim,int sleep) {
    if(dim<0||dim>3600||sleep<0||sleep>7200||(dim>0&&sleep>0&&dim>=sleep))return false;
    m_dimSeconds=dim;m_sleepSeconds=sleep;
    m_settings.setValue("display/dimSeconds",dim);
    m_settings.setValue("display/sleepSeconds",sleep);
    activity();emit changed();return true;
}
void ScreenController::requestPower(const QString &action) {
    if(m_powerPending)return;
    m_settings.sync();m_powerPending=true;m_pendingAction=action;
    m_powerMessage=action=="halt"?"Shutting down…":"Restarting…";
    emit stopping();emit changed();m_flush.start();
}
void ScreenController::shutdown(){requestPower("halt");}
void ScreenController::restart(){requestPower("reboot");}
bool ScreenController::volumeKey(int code,int value) {
    if(value==0){m_consumedKeys.remove(code);return true;}
    if(m_powerPending||m_consumedKeys.contains(code))return true;
    if(m_sleeping){m_consumedKeys.insert(code);wake();return true;}
    activity();return false;
}
bool ScreenController::eventFilter(QObject *object,QEvent *event) {
    if(!qobject_cast<QWindow *>(object))return false;
    const auto type=event->type();
    if(type==QEvent::KeyPress||type==QEvent::KeyRelease) {
        const int key=static_cast<QKeyEvent *>(event)->key();
        // Physical volume keys are handled once through evdev, avoiding a race
        // between Qt key delivery and the shared wake/volume controller.
        if(key==Qt::Key_VolumeUp||key==Qt::Key_VolumeDown){event->accept();return true;}
    }
    if(m_powerPending&&(type==QEvent::TouchBegin||type==QEvent::TouchUpdate||type==QEvent::TouchEnd||type==QEvent::MouseButtonPress||type==QEvent::MouseButtonRelease||type==QEvent::KeyPress)){event->accept();return true;}
    if(type==QEvent::TouchBegin) {
        if(m_sleeping){m_swallowTouch=true;wake();event->accept();return true;}
        activity();
    }else if(type==QEvent::TouchUpdate||type==QEvent::TouchEnd||type==QEvent::TouchCancel) {
        if(m_swallowTouch){if(type!=QEvent::TouchUpdate)m_swallowTouch=false;event->accept();return true;}
        activity();
    }else if(type==QEvent::MouseButtonPress||type==QEvent::MouseButtonDblClick) {
        if(m_sleeping){m_swallowMouse=true;wake();event->accept();return true;}
        activity();
    }else if(type==QEvent::MouseButtonRelease) {
        if(m_swallowMouse){m_swallowMouse=false;event->accept();return true;}
    }else if(type==QEvent::MouseMove) {
        if(m_swallowMouse){event->accept();return true;}
        activity();
    }else if(type==QEvent::KeyPress) {
        if(m_sleeping){wake();event->accept();return true;}
        activity();
    }else if(type==QEvent::Wheel) {
        if(m_sleeping){wake();event->accept();return true;}
        activity();
    }
    return false;
}
