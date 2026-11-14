#pragma once
#include "sharedaudio.h"
#include "bluetooth.h"
#include "audiolab.h"
#include "assistant.h"
#include "videoplayer.h"
#include "systemstats.h"
#include "deviceconfig.h"
#include <QObject>
#include <QProcess>
#include <QLocalSocket>
#include <QLocalServer>
#include <QTimer>
#include <QSettings>
#include <QJsonArray>
#include <QElapsedTimer>
#include <functional>
class ScreenController;
class WeatherClient;
class RadioCatalog;
class VrmClient;
class MediaLibrary;
class Controller:public QObject {
    Q_OBJECT
    Q_PROPERTY(VideoPlayer *video READ video CONSTANT)
    Q_PROPERTY(SystemStats *systemStats READ systemStats CONSTANT)
    Q_PROPERTY(QString themeName READ themeName NOTIFY themeChanged)
    Q_PROPERTY(int volume READ volume NOTIFY changed)
    Q_PROPERTY(bool muted READ muted NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString source READ source NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY changed)
    Q_PROPERTY(double position READ position NOTIFY changed)
    Q_PROPERTY(double duration READ duration NOTIFY changed)
    Q_PROPERTY(QString radioId READ radioId NOTIFY changed)
    Q_PROPERTY(QString clock READ clock NOTIFY clockChanged)
    Q_PROPERTY(QString date READ date NOTIFY clockChanged)
    Q_PROPERTY(QVariantMap modules READ modules CONSTANT)
    Q_PROPERTY(QStringList dashboardPages READ dashboardPages CONSTANT)
    Q_PROPERTY(QString configProblem READ configProblem CONSTANT)
public:
    explicit Controller(ScreenController *display,QObject *parent=nullptr);
    ~Controller();
    QString themeName()const{return m_themeName;}
    Q_INVOKABLE void setTheme(const QString &name);
    int volume()const{return m_volume;}
    bool muted()const{return m_muted;}
    QString state()const{return m_source=="bluetooth"&&m_wantPlay&&!m_switching?m_phone.state():m_state;}
    QString message()const{return m_source=="bluetooth"&&m_wantPlay&&!m_switching?m_phone.status():m_message;}
    QString source()const{return m_source;}
    QString title()const{return m_source=="bluetooth"?(m_phone.trackTitle().isEmpty()?QString("Phone audio"):m_phone.trackTitle()):m_title;}
    QString subtitle()const{return m_source=="bluetooth"?(m_phone.trackArtist().isEmpty()?m_phone.phoneName():m_phone.trackArtist()):m_subtitle;}
    QString radioId()const{return m_radioId;}
    QString clock()const{return m_clock;}
    QString date()const{return m_date;}
    QVariantMap modules()const{return DeviceConfig::modules();}
    QStringList dashboardPages()const{return DeviceConfig::pages();}
    QString configProblem()const{return DeviceConfig::problem();}
    double position()const{return m_position;}
    double duration()const{return m_duration;}
    BluetoothAudio *phone(){return &m_phone;}
    VideoPlayer *video(){return &m_videoPlayer;}
    SystemStats *systemStats(){return &m_systemStats;}
    AudioLab *audioLab(){return &m_audioLab;}
    VoiceAssistant *assistant(){return &m_assistant;}
    void releaseAudio(bool force=false);
    bool audioReleased()const;
    bool wakeWordAllowed()const{return !m_audioLab.running()&&(sharedAudioEnabled()||(!m_wantPlay&&!m_switching));}
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void useRadio();
    Q_INVOKABLE void useBluetooth();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void setVolume(int value);
    Q_INVOKABLE void toggleMute();
    void playMedia(const QString &source,const QUrl &url,const QString &title,const QString &subtitle,const QStringList &headers,bool video,const QString &key,double start=0);
    void playRadio(const QString &id,const QUrl &url,const QString &name,const QString &subtitle);
    void setDataSources(WeatherClient *weather,RadioCatalog *radio,VrmClient *energy);
    bool hasRadioStation(const QString &id)const;
    void setVideoLibrary(MediaLibrary *library){m_videoLibrary=library;}
    QJsonObject toolRequest(const QJsonObject &request);
    void toolRequestAsync(const QJsonObject &request,std::function<void(QJsonObject)> done);
    static QString socketPath();
signals:
    void cancelDiscoveryRequested();
    void radioRequested(const QString &id);
    void themeChanged();
    void changed();
    void clockChanged();
    void advanceRequested(int delta);
    void replayRequested(const QString &source);
    void playbackEvent(const QString &key,const QString &event,double position,bool paused);
    void openAppRequested(const QString &app);
    void audioSetupRequested();
private:
    SystemStats m_systemStats;
    void beginPlayback();
    void startPlayer();
    void loadMedia();
    void finishPlayback();
    void command(const QJsonArray &cmd);
    void readPlayer();
    void handlePlayerEvent(const QJsonObject &event);
    void setState(const QString &s,const QString &message);
    void applyVolume();
    void watchKeys(const char *path);
    void handleClient(QLocalSocket *client);
    ScreenController *m_display;
    WeatherClient *m_weather=nullptr;
    RadioCatalog *m_radio=nullptr;
    VrmClient *m_energy=nullptr;
    MediaLibrary *m_videoLibrary=nullptr;
    QString m_radioId="radio4";
    AudioLab m_audioLab;
    VoiceAssistant m_assistant;
    BluetoothAudio m_phone;
    VideoPlayer m_videoPlayer;
    QProcess m_player,m_mixer;
    QLocalSocket m_ipc;
    QLocalServer m_server;
    QTimer m_connectTimer,m_volumeTimer,m_clockTimer,m_loadTimer,m_handoff,m_reportTimer;
    QElapsedTimer m_switchClock;
    QSettings m_settings;
    QByteArray m_buffer;
    QString m_themeName="ember",m_source="radio",m_radioUrl,m_url,m_title="BBC Radio 4",m_subtitle="Live radio",m_playbackKey;
    QString m_state="stopped",m_message="Ready when you are",m_clock,m_date,m_playerSocket;
    QStringList m_headers;
    int m_volume=40,m_connectAttempts=0;
    bool m_muted=false,m_wantPlay=false,m_switching=false,m_video=false,m_reported=false,m_seekable=false;
    double m_position=0,m_duration=0,m_start=0;
};
