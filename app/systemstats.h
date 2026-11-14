#pragma once
#include <QObject>
#include <QJsonObject>
#include <QTimer>

class SystemStats : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(int cpu READ cpu NOTIFY changed)
    Q_PROPERTY(int memory READ memory NOTIFY changed)
    Q_PROPERTY(int disk READ disk NOTIFY changed)
public:
    explicit SystemStats(QObject *parent=nullptr);
    bool active()const{return m_active;}
    void setActive(bool active);
    int cpu()const{return m_cpu;}
    int memory()const{return m_memory;}
    int disk()const{return m_disk;}
    QJsonObject snapshot()const;
signals:
    void changed();
    void activeChanged();
private:
    void sample();
    QTimer m_timer;
    bool m_active=false,m_haveCpu=false;
    quint64 m_previousTotal=0,m_previousIdle=0;
    int m_cpu=-1,m_memory=-1,m_disk=-1;
};
