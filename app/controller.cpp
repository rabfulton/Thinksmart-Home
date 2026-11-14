#include "controller.h"
#include <QPointer>
#include "sharedaudio.h"
#include "display.h"
#include "weather.h"
#include "radio.h"
#include "vrm.h"
#include "library.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <linux/input.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <cmath>

QString Controller::socketPath(){return qEnvironmentVariable("THINKSMART_CONTROL_SOCKET","/run/thinksmart-ui/control.sock");}
Controller::Controller(ScreenController *display,QObject *parent):QObject(parent),m_display(display),m_audioLab(this,display),m_assistant(this,display),m_videoPlayer(this){
    connect(&m_videoPlayer,&VideoPlayer::event,this,&Controller::handlePlayerEvent);
    connect(display,&ScreenController::stopping,this,&Controller::stop);
    connect(&m_phone,&BluetoothAudio::changed,this,&Controller::changed);
    m_themeName=m_settings.value("appearance/theme","ember").toString();
    if(!QStringList{"ember","tide","dawn","autumn","spring","dusk"}.contains(m_themeName))m_themeName="ember";
    m_volume=qBound(0,m_settings.value("volume",40).toInt(),100);
    QFile stations(qEnvironmentVariable("THINKSMART_STATIONS","/etc/thinksmart/stations.json"));
    if(stations.open(QIODevice::ReadOnly))m_radioUrl=QJsonDocument::fromJson(stations.readAll()).object().value("radio4").toString();m_url=m_radioUrl;
    m_playerSocket=QFileInfo(socketPath()).absolutePath()+"/player.sock";
    m_player.setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);});
    // Player errors can contain credential-bearing stream URLs. Keep them out of logs.
    connect(&m_player,&QProcess::readyReadStandardError,this,[this](){m_player.readAllStandardError();});
    connect(&m_player,&QProcess::errorOccurred,this,[this](){if(!m_switching&&m_wantPlay){m_connectTimer.stop();m_loadTimer.stop();finishPlayback();m_wantPlay=false;setState("error","Cannot start media playback");}});
    connect(&m_player,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        m_connectTimer.stop();m_loadTimer.stop();m_ipc.abort();if(m_switching)return;
        finishPlayback();if(m_wantPlay){m_wantPlay=false;setState(code==0?"stopped":"error",code==0?"Playback stopped":"Media player stopped. Try again.");}
    });
    connect(&m_ipc,&QLocalSocket::connected,this,[this](){m_connectTimer.stop();command({"observe_property",1,"pause"});command({"observe_property",2,"time-pos"});command({"observe_property",3,"duration"});command({"observe_property",4,"seekable"});if(m_wantPlay)loadMedia();});
    connect(&m_ipc,&QLocalSocket::readyRead,this,&Controller::readPlayer);
    m_connectTimer.setInterval(100);connect(&m_connectTimer,&QTimer::timeout,this,[this](){
        if(++m_connectAttempts>100){m_connectTimer.stop();m_wantPlay=false;m_player.kill();setState("error","Media player connection timed out");return;}
        if(m_ipc.state()==QLocalSocket::UnconnectedState)m_ipc.connectToServer(m_playerSocket);
    });
    m_loadTimer.setSingleShot(true);m_loadTimer.setInterval(30000);connect(&m_loadTimer,&QTimer::timeout,this,[this](){stop();setState("error","Stream timed out. Try again.");});
    m_handoff.setInterval(25);connect(&m_handoff,&QTimer::timeout,this,[this](){
        if(m_player.state()==QProcess::NotRunning&&m_phone.receiverStopped()&&!m_videoPlayer.running()&&m_audioLab.released()&&(sharedAudioEnabled()||m_assistant.released())){
            m_handoff.stop();m_switching=false;
            if(m_source=="bluetooth"){m_phone.startReceiver();setState("stopped","Ready for phone audio");}
            else{m_loadTimer.start();startPlayer();}return;
        }
        if(m_switchClock.elapsed()>1500){m_player.kill();m_phone.killReceiver();m_audioLab.kill();if(!sharedAudioEnabled())m_assistant.kill();}
        if(m_switchClock.elapsed()>2500){m_handoff.stop();m_switching=false;m_wantPlay=false;setState("error","The previous audio source could not stop");}
    });
    m_reportTimer.setInterval(15000);connect(&m_reportTimer,&QTimer::timeout,this,[this](){if(m_reported)emit playbackEvent(m_playbackKey,"progress",m_position,state()=="paused");});m_reportTimer.start();
    m_volumeTimer.setSingleShot(true);m_volumeTimer.setInterval(60);connect(&m_volumeTimer,&QTimer::timeout,this,&Controller::applyVolume);
    connect(&m_mixer,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){if(code)qWarning()<<"Volume control failed";m_volumeTimer.start();});m_mixer.setProperty("applied",-1);m_volumeTimer.start();
    auto updateClock=[this](){auto now=QDateTime::currentDateTime();QString c=now.toString("HH:mm"),d=now.toString("dddd, d MMMM");if(c!=m_clock||d!=m_date){m_clock=c;m_date=d;emit clockChanged();}};
    connect(&m_clockTimer,&QTimer::timeout,this,updateClock);m_clockTimer.start(1000);updateClock();
    watchKeys("/dev/input/by-path/platform-gpio-keys-event");watchKeys("/dev/input/by-path/platform-200f000.spmi-platform-200f000.spmi:pmic@0:pon@800:resin-event");
    QLocalServer::removeServer(socketPath());m_server.setSocketOptions(QLocalServer::UserAccessOption);if(!m_server.listen(socketPath()))qFatal("Cannot listen on local control socket");
    connect(&m_server,&QLocalServer::newConnection,this,[this](){while(m_server.hasPendingConnections())handleClient(m_server.nextPendingConnection());});
}
Controller::~Controller(){
    finishPlayback();m_wantPlay=false;m_player.disconnect(this);m_mixer.disconnect(this);m_ipc.disconnect(this);
    for(auto process:{&m_player,&m_mixer})if(process->state()!=QProcess::NotRunning){process->terminate();if(!process->waitForFinished(500)){process->kill();process->waitForFinished(500);}}
}
void Controller::setState(const QString &s,const QString &message){m_state=s;m_message=message;emit changed();}
void Controller::command(const QJsonArray &cmd){if(m_video&&m_videoPlayer.running()){m_videoPlayer.command(cmd);return;}if(m_ipc.state()==QLocalSocket::ConnectedState)m_ipc.write(QJsonDocument(QJsonObject{{"command",cmd}}).toJson(QJsonDocument::Compact)+'\n');}
void Controller::finishPlayback(){if(!m_playbackKey.isEmpty())emit playbackEvent(m_playbackKey,"stopped",m_position,state()=="paused");m_playbackKey.clear();m_reported=false;}
void Controller::beginPlayback(){
    emit cancelDiscoveryRequested();
    m_videoPlayer.stop();m_audioLab.stop();if(!sharedAudioEnabled())m_assistant.stop();
    m_switching=true;m_wantPlay=true;m_handoff.stop();m_connectTimer.stop();m_loadTimer.stop();m_ipc.abort();m_player.terminate();m_phone.stopReceiver();
    m_switchClock.restart();m_handoff.start();setState("connecting",m_video?"Opening video…":"Switching audio source…");
}
void Controller::playMedia(const QString &source,const QUrl &url,const QString &title,const QString &subtitle,const QStringList &headers,bool video,const QString &key,double start){
    if(!DeviceConfig::enabled(source)||!url.isValid()||(url.scheme()!="http"&&url.scheme()!="https"))return;
    finishPlayback();m_source=source;m_url=url.toString(QUrl::FullyEncoded);m_title=title;m_subtitle=subtitle;m_headers=headers;m_video=video;m_playbackKey=key;m_start=qMax(0.0,start);m_position=m_start;m_duration=0;beginPlayback();
}
void Controller::startPlayer(){
    if(m_video){m_display->wake();emit openAppRequested("video");m_videoPlayer.start(QUrl(m_url),m_headers,m_start);return;}
    QFile::remove(m_playerSocket);m_buffer.clear();m_ipc.abort();
    QStringList args={"--idle=yes","--ao=alsa","--audio-device=alsa/default","--audio-format=s16","--audio-channels=stereo","--volume=100","--no-config","--cache=yes","--demuxer-max-bytes=16777216","--demuxer-max-back-bytes=1048576","--cache-secs=5","--network-timeout=15","--msg-level=all=warn","--terminal=no","--input-terminal=no","--input-ipc-server="+m_playerSocket};
    if(m_video)args<<"--vo=gpu"<<"--gpu-context=wayland"<<"--hwdec=auto-safe"<<"--fullscreen=yes"<<"--osc=yes"<<"--script-opts=osc-windowcontrols=yes";
    else args<<"--no-video";
    m_player.start("mpv",args);m_connectAttempts=0;m_connectTimer.start();
}
void Controller::loadMedia(){
    QJsonObject options{{"start",QString::number(m_start)}};
    if(!m_headers.isEmpty())options.insert("http-header-fields",m_headers.join(','));
    command({"loadfile",m_url,"replace",-1,options});command({"set_property","mute",m_muted||m_volume==0});
    setState("connecting","Connecting…");
}
void Controller::play(){
    if(!DeviceConfig::enabled(m_source))return;
    if(m_switching||state()=="playing")return;
    if(m_source=="bluetooth"){
        if(!m_phone.active())beginPlayback();else m_phone.transport("Play");return;
    }
    if(m_state=="paused"&&(m_videoPlayer.running()||m_ipc.state()==QLocalSocket::ConnectedState)){command({"set_property","pause",false});return;}
    if(m_source=="jellyfin"){emit replayRequested(m_source);return;}
    if(m_source=="radio"&&m_radio&&(m_radioId=="commons"||m_radioId=="lords")){emit radioRequested(m_radioId);return;}
    if(m_url.isEmpty()){setState("error","No stream is configured");return;}
    finishPlayback();m_start=0;beginPlayback();
}
void Controller::pause(){if(m_source=="bluetooth")m_phone.transport("Pause");else if(m_state=="playing")command({"set_property","pause",true});}
void Controller::stop(){
    m_audioLab.stop();if(!sharedAudioEnabled())m_assistant.stop();
    emit cancelDiscoveryRequested();
    finishPlayback();m_wantPlay=false;m_switching=false;m_handoff.stop();m_loadTimer.stop();m_connectTimer.stop();
    if(m_source=="bluetooth"&&m_phone.connected())m_phone.transport("Pause");m_phone.stopReceiver();
    command({"stop"});m_videoPlayer.stop();if(m_video)m_player.terminate();setState("stopped","Ready when you are");
}
void Controller::toggle(){if(state()=="playing")pause();else if(m_switching||m_state=="connecting")stop();else play();}
void Controller::useRadio(){if(!DeviceConfig::enabled("radio"))return;stop();m_source="radio";m_radioId="radio4";m_url=m_radioUrl;m_title="BBC Radio 4";m_subtitle="Live radio";m_headers.clear();m_video=false;m_playbackKey.clear();m_start=0;m_position=0;m_duration=0;emit changed();}
void Controller::useBluetooth(){if(!DeviceConfig::enabled("bluetooth"))return;if(m_source=="bluetooth"&&(m_phone.active()||m_switching))return;finishPlayback();m_source="bluetooth";m_video=false;m_headers.clear();m_playbackKey.clear();m_position=0;m_duration=0;beginPlayback();}
void Controller::next(){if(m_source=="bluetooth")m_phone.transport("Next");else if(m_source=="navidrome")emit advanceRequested(1);}
void Controller::previous(){if(m_source=="bluetooth")m_phone.transport("Previous");else if(m_source=="navidrome")emit advanceRequested(-1);}
void Controller::seek(double seconds){if(m_seekable&&m_duration>0)command({"seek",qBound(0.0,seconds,m_duration),"absolute"});}
void Controller::setVolume(int value){m_volume=qBound(0,value,100);m_muted=false;m_settings.setValue("volume",m_volume);command({"set_property","mute",m_volume==0});m_volumeTimer.start();emit changed();}
void Controller::toggleMute(){m_muted=!m_muted;command({"set_property","mute",m_muted||m_volume==0});m_volumeTimer.start();emit changed();}
void Controller::applyVolume(){int percent=m_muted?0:m_volume;int value=percent==0?0:45+qRound(percent*0.4);if(m_mixer.state()!=QProcess::NotRunning||m_mixer.property("applied").toInt()==value)return;m_mixer.setProperty("applied",value);m_mixer.start("amixer",{"-q","-c","0","sset","Speaker Amp Master",QString::number(value)});}
void Controller::readPlayer(){
    m_buffer+=m_ipc.readAll();if(m_buffer.size()>1024*1024){m_buffer.clear();return;}
    while(m_buffer.contains('\n')){
        auto end=m_buffer.indexOf('\n');auto o=QJsonDocument::fromJson(m_buffer.left(end)).object();m_buffer.remove(0,end+1);QString event=o.value("event").toString();
        handlePlayerEvent(o);
    }
}
void Controller::handlePlayerEvent(const QJsonObject &o){
    const QString event=o.value("event").toString();
        if(event=="playback-restart"&&m_wantPlay&&!m_switching){m_loadTimer.stop();setState("playing",m_subtitle.isEmpty()?"Playing":m_subtitle);if(!m_reported&&!m_playbackKey.isEmpty()){m_reported=true;emit playbackEvent(m_playbackKey,"start",m_position,false);}}
        if(event=="property-change"){
            QString name=o.value("name").toString();auto data=o.value("data");
            if(name=="pause"&&m_wantPlay&&m_state!="connecting"&&!m_switching)setState(data.toBool()?"paused":"playing",data.toBool()?"Paused":m_subtitle);
            if(name=="time-pos"&&data.isDouble()){m_position=data.toDouble();if(m_video&&m_state=="playing")m_display->activity();emit changed();}
            if(name=="duration"&&data.isDouble()){m_duration=data.toDouble();emit changed();}
            if(name=="seekable")m_seekable=data.toBool();
        }
        if(event=="end-file"&&m_wantPlay&&!m_switching){
            finishPlayback();m_wantPlay=false;m_loadTimer.stop();QString reason=o.value("reason").toString();
            setState(reason=="error"?"error":"stopped",reason=="error"?"Stream unavailable. Try again.":"Playback finished");
            if(m_video)m_player.terminate();if(reason=="eof"&&m_source=="navidrome")emit advanceRequested(1);
        }
}

void Controller::watchKeys(const char *path){
    int fd=::open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
    if(fd<0){qWarning()<<"Cannot open physical keys"<<path;return;}
    auto notifier=new QSocketNotifier(fd,QSocketNotifier::Read,this);
    connect(notifier,&QSocketNotifier::activated,this,[this,fd](){
        input_event ev;
        while(::read(fd,&ev,sizeof(ev))==sizeof(ev)) {
            if(ev.type!=EV_KEY||(ev.code!=KEY_VOLUMEUP&&ev.code!=KEY_VOLUMEDOWN))continue;
            if(m_display->volumeKey(ev.code,ev.value)||ev.value==0)continue;
            if(ev.code==KEY_VOLUMEUP)setVolume(m_volume+5);
            if(ev.code==KEY_VOLUMEDOWN)setVolume(m_volume-5);
        }
    });
    connect(this,&QObject::destroyed,[fd](){::close(fd);});
}
void Controller::handleClient(QLocalSocket *client){
    client->setParent(this);
    connect(client,&QLocalSocket::disconnected,client,&QObject::deleteLater);
    QTimer::singleShot(3000,client,[client](){if(!client->property("requestParsed").toBool())client->disconnectFromServer();});
    connect(client,&QLocalSocket::readyRead,this,[this,client](){
        if(client->property("requestParsed").toBool())return;
        QByteArray buffer=client->property("request").toByteArray()+client->readAll();
        if(buffer.size()>4096){client->disconnectFromServer();return;}
        client->setProperty("request",buffer);if(!buffer.contains('\n'))return;
        client->setProperty("requestParsed",true);
        QString request=QString::fromUtf8(buffer.left(buffer.indexOf('\n'))).trimmed();
        if(request.startsWith('{')){
            QJsonParseError parse;const auto json=QJsonDocument::fromJson(request.toUtf8(),&parse);
            if(parse.error!=QJsonParseError::NoError||!json.isObject()){
                client->write("{\"ok\":false,\"error\":\"Expected one JSON request object\"}\n");client->disconnectFromServer();return;
            }
            const QPointer<QLocalSocket> socket(client);
            toolRequestAsync(json.object(),[socket](QJsonObject result){if(!socket)return;socket->write(QJsonDocument(result).toJson(QJsonDocument::Compact)+'\n');socket->disconnectFromServer();});return;
        }
        bool valid=true;
        if(request=="play")play();else if(request=="pause")pause();else if(request=="stop")stop();else if(request=="toggle")toggle();
        else if(request=="volume-up")setVolume(m_volume+5);else if(request=="volume-down")setVolume(m_volume-5);
        else if(request=="mute")toggleMute();
        else if(request.startsWith("seek ")){bool ok=false;double seconds=request.mid(5).toDouble(&ok);if(ok&&std::isfinite(seconds))seek(seconds);else valid=false;}
        else if(request.startsWith("set-volume ")){bool ok=false;int value=request.mid(11).toInt(&ok);if(ok)setVolume(value);else valid=false;}
        else if(request=="source-radio")useRadio();
        else if(request=="source-bluetooth")useBluetooth();
        else if(request=="pair-phone")m_phone.startPairing();
        else if(request=="cancel-pairing")m_phone.endPairing();
        else if(request=="assistant-start"){m_display->wake();emit openAppRequested("assistant");m_assistant.start();}
        else if(request=="assistant-stop")m_assistant.stop();
        else if(request=="assistant-enable")m_assistant.setEnabled(true);
        else if(request=="assistant-disable")m_assistant.setEnabled(false);
        else if(request.startsWith("assistant-model ")){const auto model=request.mid(16);m_assistant.setModel(model);valid=m_assistant.model()==model;}
        else if(request.startsWith("assistant-backend ")){const auto model=request.mid(18);m_assistant.setBackendModel(model);valid=m_assistant.backendModel()==model;}
        else if(request=="audio-lab-start"){m_display->wake();emit openAppRequested("assistant");emit audioSetupRequested();m_audioLab.start(false);}
        else if(request=="audio-lab-record"){m_display->wake();emit openAppRequested("assistant");emit audioSetupRequested();m_audioLab.start(true);}
        else if(request=="audio-lab-stop")m_audioLab.stop();
        else if(request=="next")next();else if(request=="previous")previous();
        else if(request.startsWith("open-app ")) {
            const QString app=request.mid(9);
            if(QStringList{"assistant","weather","solar","radio","music","video","news","home","chess"}.contains(app)&&DeviceConfig::appEnabled(app)){m_display->wake();emit openAppRequested(app);}else valid=false;
        }
        else if(request=="screen-off")m_display->sleep();
        else if(request=="wake")m_display->wake();
        else if(request=="power-menu")m_display->showPowerMenu();
        else if(request=="display-settings")m_display->showSettings();
        else if(request=="shutdown")m_display->shutdown();
        else if(request=="restart")m_display->restart();
        else if(request.startsWith("set-brightness ")){bool ok=false;int value=request.mid(15).toInt(&ok);if(ok)m_display->setBrightness(value);else valid=false;}
        else if(request.startsWith("set-idle-timers ")) {
            const auto values=request.mid(16).split(' ',Qt::SkipEmptyParts);
            bool dimOk=false,sleepOk=false;
            int dim=values.value(0).toInt(&dimOk),sleep=values.value(1).toInt(&sleepOk);
            valid=values.size()==2&&dimOk&&sleepOk&&m_display->setIdleTimers(dim,sleep);
        }
        else if(request!="status")valid=false;
        QJsonObject result{{"video",QJsonObject{{"visible",m_videoPlayer.visible()},{"decoder",m_videoPlayer.decoder()},{"controlError",m_videoPlayer.controlError()},{"diagnostics",m_videoPlayer.diagnostics()}}},{"position",m_position},{"duration",m_duration},{"state",state()},{"message",message()},{"source",m_source},{"radioId",m_radioId},{"title",title()},{"bluetooth",m_phone.snapshot()},{"audioLab",m_audioLab.snapshot()},{"assistant",m_assistant.snapshot()},{"volume",m_volume},{"muted",m_muted},{"display",m_display->status()}};
        result.insert("system",m_systemStats.snapshot());
        if(!valid)result.insert("error","Unknown command or invalid argument");
        client->write(QJsonDocument(result).toJson(QJsonDocument::Compact)+'\n');client->disconnectFromServer();
    });
}

void Controller::setTheme(const QString &name){
    if(!QStringList{"ember","tide","dawn","autumn","spring","dusk"}.contains(name)||name==m_themeName)return;
    m_themeName=name;m_settings.setValue("appearance/theme",name);emit themeChanged();
}

void Controller::playRadio(const QString &id,const QUrl &url,const QString &name,const QString &subtitle){
    m_radioId=id;playMedia("radio",url,name,subtitle,{},false,{},0);
}
void Controller::setDataSources(WeatherClient *weather,RadioCatalog *radio,VrmClient *energy){
    m_weather=weather;m_radio=radio;m_energy=energy;
}
bool Controller::hasRadioStation(const QString &id)const{return m_radio&&m_radio->knows(id);}
void Controller::toolRequestAsync(const QJsonObject &request,std::function<void(QJsonObject)> done){
    if(!DeviceConfig::actionEnabled(request.value("action").toString())||request.value("action")!="radio.search"||!m_radio){done(toolRequest(request));return;}
    const QPointer<Controller> self(this);
    m_radio->search(request,[self,request,done=std::move(done)](QJsonObject result){
        if(!self)return;
        result.insert("version",1);result.insert("action","radio.search");
        if(!result.contains("asOf"))result.insert("asOf",QDateTime::currentSecsSinceEpoch());
        if(request.contains("requestId"))result.insert("requestId",request.value("requestId"));
        result.insert("media",self->toolRequest({{"action","media.status"}}).value("media"));done(result);
    });
}
QJsonObject Controller::toolRequest(const QJsonObject &request){
    const QString action=request.value("action").toString();
    QJsonObject result{{"ok",true},{"version",1},{"action",action},{"asOf",QDateTime::currentSecsSinceEpoch()}};
    if(request.contains("requestId"))result.insert("requestId",request.value("requestId"));
    auto error=[&](const QString &message){result.insert("ok",false);result.insert("error",message);};
    if(!DeviceConfig::actionEnabled(action)){error("This module is disabled in device.json");return result;}
    if(action=="system.capabilities"){
        result.insert("actions",QJsonArray{
            QJsonObject{{"action","radio.list"},{"optional",QJsonArray{"genre","search"}}},
            QJsonObject{{"action","radio.search"},{"optional",QJsonArray{"country","language","tag","search"}},{"limit",5}},
            QJsonObject{{"action","radio.play"},{"required",QJsonArray{"id"}}},
            QJsonObject{{"action","radio.random"},{"required",QJsonArray{"genre"}}},
            QJsonObject{{"action","radio.status"}},QJsonObject{{"action","weather.get"}},
            QJsonObject{{"action","weather.refresh"}},QJsonObject{{"action","battery.get"}},
            QJsonObject{{"action","media.status"}},QJsonObject{{"action","media.stop"}},
            QJsonObject{{"action","media.pause"}},QJsonObject{{"action","media.play"}},
            QJsonObject{{"action","media.volume"},{"required",QJsonArray{"percent"}}}
        });
        QJsonArray available;for(const auto &value:result.value("actions").toArray())if(DeviceConfig::actionEnabled(value.toObject().value("action").toString()))available.append(value);
        result.insert("actions",available);result.insert("modules",QJsonObject::fromVariantMap(DeviceConfig::modules()));
    }else if(action.startsWith("video.")&&m_videoLibrary){
        if(action=="video.browse"){if(request.contains("decade"))m_videoLibrary->setDecade(request.value("decade").toInt());else if(request.contains("genre"))m_videoLibrary->setGenre(request.value("genre").toString());else if(request.contains("section"))m_videoLibrary->setSection(request.value("section").toString());m_videoLibrary->refresh();}
        else if(action=="video.more"){m_videoLibrary->loadMore();}
        else if(action=="video.inspect"){
            const auto index=request.value("index");if(!index.isDouble()||index.toDouble()!=index.toInt()||index.toInt()<0||index.toInt()>=m_videoLibrary->items().size())error("Invalid video index");else m_videoLibrary->inspect(index.toInt());
        }else if(action=="video.play"){
            if(m_videoLibrary->detail().isEmpty())error("Select a title first");else m_videoLibrary->playSelection(request.value("restart").toBool());
        }else if(action=="video.back"){if(!m_videoLibrary->detail().isEmpty())m_videoLibrary->closeDetails();else m_videoLibrary->back();}
        else if(action!="video.status")error("Unknown video action");
        result.insert("video",m_videoLibrary->snapshot());
    }else if(action=="radio.list"&&m_radio){
        result.insert("stations",m_radio->list(request.value("genre").toString(),request.value("search").toString()));
    }else if(action=="radio.play"&&m_radio){
        if(!m_radio->play(request.value("id").toString()))error(m_radio->status());
        result.insert("radio",m_radio->snapshot());result.insert("accepted",result.value("ok"));
    }else if(action=="radio.random"&&m_radio){
        const QString genre=request.value("genre").toString();
        if(genre.isEmpty())error("A genre is required");else if(!m_radio->random(genre))error(m_radio->status());
        result.insert("radio",m_radio->snapshot());result.insert("accepted",result.value("ok"));
    }else if(action=="radio.status"&&m_radio)result.insert("radio",m_radio->snapshot());
    else if((action=="weather.get"||action=="weather.refresh")&&m_weather){
        if(action=="weather.refresh")m_weather->refresh();result.insert("weather",m_weather->snapshot());
    }else if(action=="battery.get"&&m_energy){
        result.insert("battery",QJsonObject{{"available",m_energy->hasData()},{"stale",m_energy->stale()},
            {"status",m_energy->status()},{"site",m_energy->siteName()},{"lastReport",m_energy->lastReport()},
            {"lastCheck",m_energy->lastCheck()},{"error",m_energy->problem()},
            {"metrics",QJsonObject::fromVariantMap(m_energy->metrics())}});
    }else if(action.startsWith("media.")){
        if(action=="media.stop")stop();else if(action=="media.pause")pause();else if(action=="media.play")play();
        else if(action=="media.volume"){
            const auto percent=request.value("percent");
            if(!percent.isDouble()||percent.toDouble()!=percent.toInt()||percent.toInt()<0||percent.toInt()>100)error("percent must be an integer from 0 to 100");else setVolume(percent.toInt());
        }else if(action!="media.status")error("Unknown action");
    }else error("Unknown or unavailable action. Use system.capabilities");
    result.insert("media",QJsonObject{{"source",m_source},{"stationId",m_radioId},{"title",title()},{"state",state()},{"message",message()},{"volume",m_volume},{"muted",m_muted}});
    return result;
}

void Controller::releaseAudio(bool force){
    if(force){m_videoPlayer.stop();m_player.kill();m_phone.killReceiver();m_assistant.kill();return;}
    m_assistant.suspendLocalAudio();stop();m_connectTimer.stop();m_loadTimer.stop();m_ipc.abort();m_player.terminate();
}
bool Controller::audioReleased()const{return m_player.state()==QProcess::NotRunning&&!m_videoPlayer.running()&&m_phone.receiverStopped()&&m_assistant.released();}
