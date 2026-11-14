#pragma once
#include <QObject>
#include "assistantprompts.h"
#include "wakeword.h"
#include "voicepreview.h"
#include <QProcess>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantList>
#include <QSet>
#include <QMap>
#include <QStringList>
#include <QWebSocket>
#include <memory>
#include <functional>
class Controller;
class ScreenController;
// UI/configuration and conversation lifecycle stay outside the DSP worker.
// OpenAI Realtime and Live use separate protocol handlers over the same audio path.
class VoiceAssistant:public QObject {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QString mode READ mode NOTIFY changed)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY changed)
    Q_PROPERTY(QVariantList models READ models NOTIFY changed)
    Q_PROPERTY(bool live READ isLive NOTIFY changed)
    Q_PROPERTY(QString backendModel READ backendModel WRITE setBackendModel NOTIFY changed)
    Q_PROPERTY(QVariantList backendModels READ backendModels NOTIFY changed)
    Q_PROPERTY(QStringList voices READ voices NOTIFY changed)
    Q_PROPERTY(QString voice READ voice WRITE setVoice NOTIFY changed)
    Q_PROPERTY(bool previewAvailable READ previewAvailable NOTIFY changed)
    Q_PROPERTY(bool previewPlaying READ previewPlaying NOTIFY changed)
    Q_PROPERTY(QString previewProblem READ previewProblem NOTIFY changed)
    Q_PROPERTY(QString vad READ vad WRITE setVad NOTIFY changed)
    Q_PROPERTY(QVariantList personalities READ personalities NOTIFY changed)
    Q_PROPERTY(QString personality READ personality WRITE setPersonality NOTIFY changed)
    Q_PROPERTY(QString promptDirectory READ promptDirectory CONSTANT)
    Q_PROPERTY(QString promptProblem READ promptProblem NOTIFY changed)
    Q_PROPERTY(bool promptsReady READ promptsReady NOTIFY changed)
    Q_PROPERTY(bool wakeListening READ wakeListening NOTIFY changed)
    Q_PROPERTY(bool wakePreparing READ wakePreparing NOTIFY changed)
    Q_PROPERTY(QString wakeStatus READ wakeStatus NOTIFY changed)
    Q_PROPERTY(QString wakeIndicator READ wakeIndicator NOTIFY changed)
    Q_PROPERTY(QString wakeProblem READ wakeProblem NOTIFY changed)
    Q_PROPERTY(double inputLevel READ inputLevel NOTIFY levelsChanged)
    Q_PROPERTY(double outputLevel READ outputLevel NOTIFY levelsChanged)
    Q_PROPERTY(QVariantList transcript READ transcript NOTIFY transcriptChanged)
public:
    VoiceAssistant(Controller *controller,ScreenController *display);
    ~VoiceAssistant() override;
    bool enabled()const{return m_enabled;}
    void setEnabled(bool value);
    bool configured()const{return m_configured;}
    bool active()const{return m_active;}
    bool released()const{return m_audio.state()==QProcess::NotRunning&&m_wake.released()&&!previewPlaying();}
    QString status()const;
    QString problem()const{return m_problem;}
    QString mode()const{return m_mode;}
    QString model()const{return m_model;}
    void setModel(const QString &value);
    QVariantList models()const;
    bool isLive()const{return m_model=="gpt-live-1";}
    QString backendModel()const{return m_backendModel;}
    void setBackendModel(const QString &value);
    QVariantList backendModels()const;
    QStringList voices()const;
    QString voice()const{return m_voice;}
    void setVoice(const QString &value);
    bool previewAvailable()const;
    bool previewPlaying()const{return m_preview.playing();}
    QString previewProblem()const{return m_previewProblem.isEmpty()?m_preview.problem():m_previewProblem;}
    Q_INVOKABLE void previewVoice();
    Q_INVOKABLE void stopVoicePreview();
    QString vad()const{return m_vad;}
    void setVad(const QString &value);
    QVariantList personalities()const{return m_prompts.personalities();}
    QString personality()const{return m_prompts.selected();}
    void setPersonality(const QString &id);
    QString promptDirectory()const{return AssistantPrompts::directory();}
    QString promptProblem()const{return m_prompts.problem();}
    bool promptsReady()const{return m_prompts.ready();}
    bool wakeListening()const{return m_wake.listening();}
    bool wakePreparing()const{return m_wake.preparing();}
    QString wakeStatus()const{return m_wake.status();}
    QString wakeIndicator()const;
    QString wakeProblem()const{return m_wake.problem();}
    double inputLevel()const{return m_inputLevel;}
    double outputLevel()const{return m_outputLevel;}
    QVariantList transcript()const{return m_transcript;}
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void interrupt();
    Q_INVOKABLE void beginTurn();
    Q_INVOKABLE void endTurn();
    Q_INVOKABLE void reloadConfiguration();
    void kill();
    void suspendLocalAudio();
    QJsonObject snapshot()const;
signals:
    void changed();
    void levelsChanged();
    void transcriptChanged();
private:
    static QString binary();
    static QStringList voiceChoices(bool live);
    static QJsonArray liveTools();
    void fail(const QString &message);
    void connectApi();
    void send(QJsonObject event);
    void worker(QJsonObject event);
    void receive(const QString &message);
    void sendMicrophone(const QString &audio);
    QJsonObject liveConfiguration()const;
    void receiveLive(const QJsonObject &event);
    void liveMeter(bool input,double dbfs);
    void updateLiveActivity();
    void interruptLive();
    void closeLiveSocket(QWebSocket *socket);
    void consumeAudio();
    void interruptPlayback(bool cancelResponse=true);
    void dispatchTool(const QJsonObject &call,std::function<void()> completed);
    bool dispatchTools(const QJsonArray &calls,std::function<void()> completed);
    void finishTool(const QJsonObject &call,const QJsonObject &result,bool parsed,std::function<void()> completed);
    void appendTranscript(const QString &id,const QString &role,const QString &text,bool delta=false);
    void updateMode();
    void setMediaDucked(bool ducked);
    void finishHandoff();
    void updateWakeWord();
    void loadConfiguration(bool restartWake);
    void restoreVoice(bool migrateLegacy=false);
    QString previewPath()const;
    Controller *m_controller;
    ScreenController *m_display;
    QProcess m_audio;
    WakeWord m_wake;
    VoicePreview m_preview;
    QString m_previewProblem,m_configVoice="marin";
    std::unique_ptr<QWebSocket> m_socket;
    QTimer m_wait,m_startDeadline,m_killTimer,m_idleTimer,m_limit,m_responseDeadline,m_wakePolicy,m_duckRelease;
    QElapsedTimer m_waitClock,m_activity,m_turnClock;
    QByteArray m_audioBuffer;
    QString m_status="Assistant disabled",m_problem,m_mode="ready";
    QString m_model="gpt-realtime-2.1-mini",m_configModel="gpt-realtime-2.1-mini",m_voice="marin",m_vad="semantic",m_apiKey;
    QString m_backendModel="gpt-6-luna",m_configBackend="gpt-6-luna";
    QElapsedTimer m_liveClock;
    QByteArray m_livePendingAudio;
    qint64 m_liveInputUntil=0,m_liveOutputUntil=0,m_liveDiscardUntil=0;
    QString m_liveTranscriptId[2];qint64 m_liveTranscriptEnd[2]={-1,-1};int m_liveTranscriptSerial=0;
    QSet<QString> m_liveResponses,m_liveCompleted;
    QMap<QString,QJsonArray> m_liveToolCalls;
    QMap<QString,QString> m_liveDelegationResponses;
    QSet<QString> m_liveEvents;
    double m_liveSeconds=0;bool m_liveUsageFinalized=false;quint64 m_liveGeneration=0;
    AssistantPrompts m_prompts;
    QString m_response,m_item;
    int m_contentIndex=0,m_flushId=0;
    struct Truncation {QString item;int index;};
    QMap<int,Truncation> m_truncations;
    QSet<QString> m_ignoredResponses,m_calls;
    QVariantList m_transcript;
    QJsonObject m_pendingAction,m_lastTool;
    bool m_handoffReady=false,m_mediaDucked=false;
    bool m_enabled=false,m_configured=false,m_active=false,m_sessionReady=false,m_responseActive=false,m_playing=false,m_userSpeaking=false,m_manualTurn=false;
    double m_inputLevel=0,m_outputLevel=0;
};
