#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QQuickFramebufferObject>
#include <QUrl>
#include <QVariantList>
struct mpv_handle;
class VideoPlayer:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY changed)
    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY changed)
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(QString controlError READ controlError NOTIFY changed)
    Q_PROPERTY(QString decoder READ decoder NOTIFY changed)
public:
    explicit VideoPlayer(QObject *parent=nullptr):QObject(parent){}
    ~VideoPlayer()override;
    QVariantList audioTracks()const{return m_audioTracks;}
    QVariantList subtitleTracks()const{return m_subtitleTracks;}
    Q_INVOKABLE void selectTrack(const QString &type,int id);
    bool visible()const{return m_visible;}
    bool running()const{return m_running;}
    QString controlError()const{return m_controlError;}
    QString decoder()const{return m_decoder;}
    QJsonObject diagnostics()const;
    mpv_handle *handle()const{return m_handle;}
    void start(const QUrl &url,const QStringList &headers,double position);
    void stop();
    void command(const QJsonArray &arguments);
    void rendererReady();
    void rendererFailed();
signals:
    void event(const QJsonObject &event);
    void changed();
private:
    bool initialize();
    void drain();
    void load();
    mpv_handle *m_handle=nullptr;
    bool m_visible=false,m_running=false,m_rendererReady=false;
    QString m_decoder,m_controlError;
    QJsonArray m_pending;
    QVariantList m_audioTracks,m_subtitleTracks;
    QJsonObject m_diagnostics;
};
class VideoSurface:public QQuickFramebufferObject {
    Q_OBJECT
    Q_PROPERTY(VideoPlayer *player READ player WRITE setPlayer NOTIFY playerChanged)
public:
    explicit VideoSurface(QQuickItem *parent=nullptr):QQuickFramebufferObject(parent){}
    VideoPlayer *player()const{return m_player;}
    void setPlayer(VideoPlayer *player);
    Renderer *createRenderer()const override;
signals:
    void playerChanged();
private:
    VideoPlayer *m_player=nullptr;
};
void registerVideoTypes();
