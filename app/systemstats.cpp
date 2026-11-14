#include "systemstats.h"
#include <QFile>
#include <QDir>
#include <QStorageInfo>
#include <algorithm>
#include <cmath>

namespace {
int percentage(quint64 used,quint64 total){
    return total?int(std::clamp(std::lround(100.*double(used)/double(total)),0L,100L)):-1;
}
}

SystemStats::SystemStats(QObject *parent):QObject(parent){
    m_timer.setInterval(5000);
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer,&QTimer::timeout,this,&SystemStats::sample);
}
void SystemStats::setActive(bool active){
    if(m_active==active)return;
    m_active=active;
    if(active){
        // Start a fresh CPU interval after leaving sleep or another page.
        m_haveCpu=false;sample();m_timer.start();
        QTimer::singleShot(1000,this,[this](){if(m_active)sample();});
    }else m_timer.stop();
    emit activeChanged();
}
void SystemStats::sample(){
    int cpu=-1,memory=-1,disk=-1;
    QFile stat("/proc/stat");
    if(stat.open(QIODevice::ReadOnly)){
        const auto fields=stat.readLine().simplified().split(' ');
        if(fields.size()>=9&&fields[0]=="cpu"){
            quint64 total=0,idle=0;bool valid=true;
            // guest/guest_nice are already included in user/nice. iowait
            // is idle CPU time, not time executing work.
            for(int i=1;i<=8;i++){
                bool ok=false;const auto ticks=fields[i].toULongLong(&ok);
                valid=valid&&ok;total+=ticks;if(i==4||i==5)idle+=ticks;
            }
            if(valid){
                if(m_haveCpu&&total>m_previousTotal&&idle>=m_previousIdle){
                    const auto elapsed=total-m_previousTotal,rest=idle-m_previousIdle;
                    cpu=percentage(elapsed-std::min(rest,elapsed),elapsed);
                }else if(!m_haveCpu)cpu=m_cpu;
                m_previousTotal=total;m_previousIdle=idle;m_haveCpu=true;
            }else m_haveCpu=false;
        }else m_haveCpu=false;
    }else m_haveCpu=false;
    QFile info("/proc/meminfo");
    if(info.open(QIODevice::ReadOnly)){
        quint64 total=0,available=0;bool haveAvailable=false;
        // procfs reports size zero, so read lines until readLine is empty.
        for(auto line=info.readLine();!line.isEmpty();line=info.readLine()){
            const auto fields=line.simplified().split(' ');if(fields.size()<2)continue;
            bool ok=false;const auto value=fields[1].toULongLong(&ok);if(!ok)continue;
            if(fields[0]=="MemTotal:")total=value;
            else if(fields[0]=="MemAvailable:"){available=value;haveAvailable=true;}
        }
        if(total&&haveAvailable)memory=percentage(total-std::min(total,available),total);
    }
    const QStorageInfo storage(QDir::rootPath());
    if(storage.isValid()&&storage.isReady()&&storage.bytesTotal()>0&&storage.bytesFree()>=0){
        const auto total=quint64(storage.bytesTotal());
        disk=percentage(total-std::min(total,quint64(storage.bytesFree())),total);
    }
    if(cpu==m_cpu&&memory==m_memory&&disk==m_disk)return;
    m_cpu=cpu;m_memory=memory;m_disk=disk;emit changed();
}
QJsonObject SystemStats::snapshot()const{
    return {{"cpuPercent",m_cpu},{"memoryPercent",m_memory},{"diskPercent",m_disk},
            {"sampling",m_active},{"cpuScope","all cores"}};
}
