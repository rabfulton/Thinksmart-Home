#include "rotation.h"
#include <QDir>
#include <QFile>
#include <QDebug>
#include <cmath>

namespace {
double readValue(const QString &path, bool *ok) {
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) { *ok=false; return 0; }
    return file.readAll().trimmed().toDouble(ok);
}
}

Rotation::Rotation(QObject *parent):QObject(parent) {
    QDir iio("/sys/bus/iio/devices");
    for(const auto &entry:iio.entryList({"iio:device*"},QDir::Dirs)) {
        QFile name(iio.filePath(entry+"/name"));
        if(name.open(QIODevice::ReadOnly)&&name.readAll().trimmed()=="bma253") {
            m_sensor=iio.filePath(entry);break;
        }
    }
    bool ok=false;
    m_scale=readValue(m_sensor+"/in_accel_scale",&ok);
    if(m_sensor.isEmpty()||!ok||m_scale<=0) {
        qWarning()<<"Automatic rotation unavailable: BMA253 sensor not found";
        return;
    }
    connect(&m_output,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        m_timeout.stop();
        if(code!=0) {qWarning()<<"Output rotation failed:"<<m_output.readAllStandardError();return;}
        m_current=m_attempt;
        qInfo()<<"Display and mapped touch rotation:"<<m_current;
    });
    connect(&m_output,&QProcess::errorOccurred,this,[this](){qWarning()<<"Cannot start wlr-randr";});
    m_timeout.setSingleShot(true);m_timeout.setInterval(5000);
    connect(&m_timeout,&QTimer::timeout,&m_output,&QProcess::kill);
    m_timer.setInterval(500);
    connect(&m_timer,&QTimer::timeout,this,&Rotation::sample);
    m_timer.start();
}

void Rotation::sample() {
    bool okX=false,okY=false,okZ=false;
    const double x=readValue(m_sensor+"/in_accel_x_raw",&okX)*m_scale;
    const double y=readValue(m_sensor+"/in_accel_y_raw",&okY)*m_scale;
    const double z=readValue(m_sensor+"/in_accel_z_raw",&okZ)*m_scale;
    QString next;
    const double ax=std::abs(x),ay=std::abs(y),az=std::abs(z);
    // Ignore flat placement and intermediate angles. Three agreeing samples
    // prevent a rotation while the device is still being repositioned.
    if(okX&&okY&&okZ) {
        if(ay>7&&ay>ax+1.3&&ay>az) next=y<0?"normal":"180";
        else if(ax>7&&ax>ay+1.3&&ax>az) next=x>0?"90":"270";
    }
    if(next.isEmpty()) {m_pending.clear();m_matches=0;return;}
    if(next!=m_pending) {m_pending=next;m_matches=1;return;}
    m_matches=qMin(m_matches+1,3);
    if(m_matches<3||next==m_current||m_output.state()!=QProcess::NotRunning)return;
    m_attempt=next;
    m_output.start("wlr-randr",{"--output","DSI-1","--transform",next});
    m_timeout.start();
}

