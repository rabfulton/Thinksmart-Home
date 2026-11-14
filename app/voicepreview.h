#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>

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
