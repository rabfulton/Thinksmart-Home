#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

// Original, bounded activation tone; playback only, through the echo-reference bus.
class WakeChime:public QObject {
    Q_OBJECT
public:
    WakeChime();
    ~WakeChime() override;
    bool playing()const{return m_process.state()!=QProcess::NotRunning;}
    void play();
    void stop();
private:
    QProcess m_process;
    QTimer m_deadline;
};

// Local playback only: no microphone, API connection or generated speech.
class VoicePreview:public QObject {
    Q_OBJECT
public:
    VoicePreview();
    ~VoicePreview() override;
    bool playing()const{return !m_next.isEmpty()||m_process.state()!=QProcess::NotRunning;}
    QString problem()const{return m_problem;}
    void play(const QString &path);
    void stop();
signals:
    void changed();
private:
    void launch();
    QProcess m_process;
    QTimer m_kill,m_deadline;
    QString m_next,m_problem;
    bool m_stopping=false;
};
