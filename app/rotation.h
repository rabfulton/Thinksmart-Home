#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

class Rotation : public QObject {
    Q_OBJECT
public:
    explicit Rotation(QObject *parent=nullptr);
    QString orientation() const { return m_current; }
private:
    void sample();
    QString m_sensor, m_pending, m_current, m_attempt;
    int m_matches=0;
    double m_scale=0;
    QTimer m_timer, m_timeout;
    QProcess m_output;
};
