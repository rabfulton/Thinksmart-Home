#pragma once
#include <QObject>
#include <QtDBus>
#include <QProcess>
#include <QTimer>
#include <QSet>
#include <functional>

using BluezInterfaces=QMap<QString,QVariantMap>;
using BluezObjects=QMap<QDBusObjectPath,BluezInterfaces>;
Q_DECLARE_METATYPE(BluezInterfaces)
Q_DECLARE_METATYPE(BluezObjects)
QDBusArgument &operator<<(QDBusArgument &a,const BluezInterfaces &v);
const QDBusArgument &operator>>(const QDBusArgument &a,BluezInterfaces &v);
QDBusArgument &operator<<(QDBusArgument &a,const BluezObjects &v);
const QDBusArgument &operator>>(const QDBusArgument &a,BluezObjects &v);
class BluetoothAudio;
class PairingAgent:public QObject,protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.bluez.Agent1")
public:
    explicit PairingAgent(BluetoothAudio *owner);
    void answer(bool accept);
    QString pendingDevice() const {return m_device;}
public slots:
    void Release();
    void Cancel();
    QString RequestPinCode(const QDBusObjectPath &device);
    uint RequestPasskey(const QDBusObjectPath &device);
    void DisplayPinCode(const QDBusObjectPath &device,const QString &pin);
    void DisplayPasskey(const QDBusObjectPath &device,uint pin,ushort entered);
    void RequestConfirmation(const QDBusObjectPath &device,uint pin);
    void RequestAuthorization(const QDBusObjectPath &device);
    void AuthorizeService(const QDBusObjectPath &device,const QString &uuid);
private:
    bool legitimate();
    void ask(const QDBusObjectPath &device,const QString &pin);
    BluetoothAudio *m_owner;
    QDBusMessage m_pending;
    QString m_device;
};
class BluetoothAudio:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool pairing READ pairing NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString phoneName READ phoneName NOTIFY changed)
    Q_PROPERTY(QString trackTitle READ trackTitle NOTIFY changed)
    Q_PROPERTY(QString trackArtist READ trackArtist NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QString confirmationName READ confirmationName NOTIFY changed)
    Q_PROPERTY(QString confirmationCode READ confirmationCode NOTIFY changed)
    Q_PROPERTY(bool confirmationPending READ confirmationPending NOTIFY changed)
    Q_PROPERTY(QVariantList devices READ devices NOTIFY changed)
public:
    explicit BluetoothAudio(QObject *parent=nullptr);
    ~BluetoothAudio() override;
    bool available() const{return !m_adapter.isEmpty();}
    bool connected() const{return !m_device.isEmpty();}
    bool pairing() const{return m_pairTimer.isActive();}
    bool active() const{return m_active;}
    bool receiverStopped() const{return m_receiver.state()==QProcess::NotRunning;}
    QString status() const;
    QString state() const;
    QString phoneName() const{return m_phoneName;}
    QString trackTitle() const{return m_trackTitle;}
    QString trackArtist() const{return m_trackArtist;}
    QString problem() const{return m_problem;}
    QString confirmationName() const{return m_confirmationName;}
    QString confirmationCode() const{return m_confirmationCode;}
    bool confirmationPending() const{return !m_confirmationName.isEmpty();}
    QVariantList devices() const{return m_devices;}
    bool allowedDevice(const QString &path) const;
    void askConfirmation(const QString &device,const QString &pin);
    void cancelConfirmation();
    void startReceiver();
    void stopReceiver();
    void killReceiver();
    void transport(const QString &method);
    Q_INVOKABLE void startPairing();
    Q_INVOKABLE void endPairing();
    Q_INVOKABLE void confirmPairing(bool accept);
    Q_INVOKABLE void connectPhone(const QString &path);
    Q_INVOKABLE void disconnectPhone();
    Q_INVOKABLE void forgetPhone(const QString &path);
    Q_INVOKABLE void refresh();
    QJsonObject snapshot() const;
signals:
    void changed();
private:
    friend class PairingAgent;
    void registerAgent();
    void setAdapter(const QString &key,const QVariant &value);
    void call(const QString &path,const QString &interface,const QString &method,const QVariantList &args={},std::function<void(bool)> done={});
    QDBusConnection m_bus;
    PairingAgent m_agent;
    QProcess m_receiver;
    QTimer m_poll,m_pairTimer;
    BluezObjects m_objects;
    QVariantList m_devices;
    QSet<QString> m_approved;
    QString m_adapter,m_device,m_player,m_phoneName,m_trackTitle,m_trackArtist,m_playState,m_problem,m_confirmationName,m_confirmationCode;
    bool m_active=false,m_registered=false,m_refreshing=false,m_streaming=false;
};
