#pragma once
#include <QObject>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QJsonObject>
#include <QTimer>
#include <QSettings>
#include <QSet>

class ScreenController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool sleeping READ sleeping NOTIFY changed)
    Q_PROPERTY(bool dimmed READ dimmed NOTIFY changed)
    Q_PROPERTY(int brightness READ brightness NOTIFY changed)
    Q_PROPERTY(int dimSeconds READ dimSeconds NOTIFY changed)
    Q_PROPERTY(int sleepSeconds READ sleepSeconds NOTIFY changed)
    Q_PROPERTY(bool powerPending READ powerPending NOTIFY changed)
    Q_PROPERTY(QString powerMessage READ powerMessage NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit ScreenController(QObject *parent=nullptr);
    ~ScreenController() override;
    bool sleeping() const { return m_sleeping; }
    bool dimmed() const { return m_dimmed; }
    int brightness() const { return m_brightness; }
    int dimSeconds() const { return m_dimSeconds; }
    int sleepSeconds() const { return m_sleepSeconds; }
    bool powerPending() const { return m_powerPending; }
    QString powerMessage() const { return m_powerMessage; }
    QString error() const { return m_error; }
    QJsonObject status() const;
    bool volumeKey(int code,int value);
    Q_INVOKABLE void wake();
    Q_INVOKABLE void sleep();
    Q_INVOKABLE void activity();
    Q_INVOKABLE void setBrightness(int percent);
    Q_INVOKABLE bool setIdleTimers(int dimSeconds,int sleepSeconds);
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void showPowerMenu() { wake();emit powerMenuRequested(); }
    Q_INVOKABLE void showSettings() { wake();emit settingsRequested(); }
signals:
    void changed();
    void stopping();
    void powerMenuRequested();
    void settingsRequested();
protected:
    bool eventFilter(QObject *object,QEvent *event) override;
private:
    void updateTarget();
    void flush();
    void requestPower(const QString &action);
    QSettings m_settings;
    QElapsedTimer m_idle;
    QTimer m_tick,m_flush,m_deadline;
    QLocalSocket m_socket;
    QSet<int> m_consumedKeys;
    QByteArray m_reply;
    QString m_error,m_pendingAction,m_sentAction,m_powerMessage;
    bool m_sleeping=true,m_dimmed=false,m_swallowTouch=false,m_swallowMouse=false,m_powerPending=false,m_busy=false;
    int m_brightness=50,m_dimSeconds=60,m_sleepSeconds=300,m_target=0,m_applied=-1,m_sentBrightness=-1;
};
