#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
class WakeWord:public QObject {
    Q_OBJECT
public:
    explicit WakeWord(QObject *parent=nullptr);
    ~WakeWord() override;
    bool available()const;
    bool released()const{return m_process.state()==QProcess::NotRunning;}
    bool listening()const{return m_listening;}
    bool preparing()const{return m_allowed&&!m_listening&&m_problem.isEmpty()&&available();}
    QString status()const;
    QString problem()const{return m_problem;}
    void setAllowed(bool allowed,const QString &reason);
    void retry();
    void kill();
    void command(const QString &type);
signals:
    void changed();
    void detected();
    void microphone(const QString &base64);
    void meter(double dbfs);
private:
    static QString binary();
    static QString modelDirectory();
    static QString keywords();
    void halt();
    void consume();
    void fail(const QString &message);
    void launch();
    QProcess m_process;
    QTimer m_killTimer,m_startDeadline;
    QElapsedTimer m_clock;
    QByteArray m_buffer;
    QString m_reason="Assistant disabled",m_problem;
    bool m_allowed=false,m_listening=false,m_detected=false,m_stopping=false;
    qint64 m_nextStart=0,m_runningSince=0;
    int m_restartCount=0;
};
