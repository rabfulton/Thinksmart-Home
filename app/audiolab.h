#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
class Controller;
class ScreenController;
class AudioLab:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QVariantMap levels READ levels NOTIFY changed)
    Q_PROPERTY(double testVoiceLevel READ testVoiceLevel WRITE setTestVoiceLevel NOTIFY changed)
    Q_PROPERTY(QVariantMap summary READ summary NOTIFY changed)
    Q_PROPERTY(QString reportPath READ reportPath NOTIFY changed)
    Q_PROPERTY(QString recordingPath READ recordingPath NOTIFY changed)
public:
    AudioLab(Controller *controller,ScreenController *display);
    ~AudioLab() override;
    bool running() const { return m_starting||m_process.state()!=QProcess::NotRunning; }
    bool released() const { return m_process.state()==QProcess::NotRunning; }
    bool available() const;
    QString status() const { return m_status; }
    QString problem() const { return m_problem; }
    QVariantMap levels() const { return m_levels; }
    double testVoiceLevel() const { return m_testVoiceLevel; }
    void setTestVoiceLevel(double value);
    QVariantMap summary()const { return m_summary.toVariantMap(); }
    QString reportPath()const { return m_reportPath; }
    QString recordingPath() const { return m_recordingPath; }
    Q_INVOKABLE void start(bool saveClips=false);
    Q_INVOKABLE void stop();
    void kill();
    QJsonObject snapshot() const;
signals:
    void changed();
private:
    static QString binary();
    static QString reference();
    void consume();
    void saveReport(int exitCode);
    void fail(const QString &message);
    Controller *m_controller;
    ScreenController *m_display;
    QProcess m_process;
    QTimer m_wait,m_deadline,m_killTimer;
    QElapsedTimer m_waitClock;
    QByteArray m_buffer;
    QString m_status="Ready for a local echo test",m_problem,m_recordingPath;
    QVariantMap m_levels;
    QJsonArray m_history;
    QJsonObject m_summary;
    QString m_reportPath,m_startedAt;
    double m_testVoiceLevel=.25;
    bool m_starting=false,m_saveClips=false,m_stopping=false;
};
