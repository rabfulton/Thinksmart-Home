#include "videoplayer.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QPointer>
#include <QQmlEngine>
#include <memory>
#include <vector>
#include <cmath>
namespace {
// Own a command tree only until libmpv has copied its async arguments.
struct Node {
    mpv_node node{};mpv_node_list list{};QByteArray text;
    std::vector<std::unique_ptr<Node>> children;
    std::vector<mpv_node> values;std::vector<QByteArray> keys;std::vector<char *> names;
    explicit Node(const QJsonValue &v){
        if(v.isString()){node.format=MPV_FORMAT_STRING;text=v.toString().toUtf8();node.u.string=text.data();}
        else if(v.isBool()){node.format=MPV_FORMAT_FLAG;node.u.flag=v.toBool();}
        else if(v.isDouble()){double d=v.toDouble();if(std::floor(d)==d){node.format=MPV_FORMAT_INT64;node.u.int64=int64_t(d);}else{node.format=MPV_FORMAT_DOUBLE;node.u.double_=d;}}
        else if(v.isArray()||v.isObject()){
            node.format=v.isArray()?MPV_FORMAT_NODE_ARRAY:MPV_FORMAT_NODE_MAP;
            if(v.isArray())for(const auto &item:v.toArray())children.push_back(std::make_unique<Node>(item));
            else{const auto object=v.toObject();for(auto i=object.begin();i!=object.end();++i){keys.push_back(i.key().toUtf8());children.push_back(std::make_unique<Node>(i.value()));}}
            for(const auto &child:children)values.push_back(child->node);
            for(auto &key:keys)names.push_back(key.data());
            list.num=int(values.size());list.values=values.data();list.keys=names.data();node.u.list=&list;
        }else node.format=MPV_FORMAT_NONE;
    }
};
class VideoRenderer:public QQuickFramebufferObject::Renderer {
    QPointer<VideoPlayer> player;
    QPointer<VideoSurface> surface;
    mpv_render_context *context=nullptr;
    bool attempted=false;
    static void *address(void *,const char *name){return reinterpret_cast<void *>(QOpenGLContext::currentContext()->getProcAddress(name));}
    static void repaint(void *data){auto *self=static_cast<VideoRenderer *>(data);if(self->surface)QMetaObject::invokeMethod(self->surface.data(),"update",Qt::QueuedConnection);}
public:
    ~VideoRenderer()override{if(context){mpv_render_context_set_update_callback(context,nullptr,nullptr);mpv_render_context_free(context);}}
    void synchronize(QQuickFramebufferObject *item)override{surface=static_cast<VideoSurface *>(item);player=surface->player();}
    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size)override{return new QOpenGLFramebufferObject(size,QOpenGLFramebufferObject::CombinedDepthStencil);}
    void render()override{
        if(!attempted&&player&&player->handle()){
            attempted=true;mpv_opengl_init_params gl{address,nullptr};
            mpv_render_param params[]={{MPV_RENDER_PARAM_API_TYPE,const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},{MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,&gl},{MPV_RENDER_PARAM_INVALID,nullptr}};
            if(mpv_render_context_create(&context,player->handle(),params)<0){QMetaObject::invokeMethod(player.data(),[p=player](){if(p)p->rendererFailed();},Qt::QueuedConnection);return;}
            mpv_render_context_set_update_callback(context,repaint,this);
            QMetaObject::invokeMethod(player.data(),[p=player](){if(p)p->rendererReady();},Qt::QueuedConnection);
        }
        if(!context)return;
        mpv_render_context_update(context);
        auto *fbo=framebufferObject();mpv_opengl_fbo target{int(fbo->handle()),fbo->width(),fbo->height(),0};
        mpv_render_param params[]={{MPV_RENDER_PARAM_OPENGL_FBO,&target},{MPV_RENDER_PARAM_INVALID,nullptr}};
        QQuickOpenGLUtils::resetOpenGLState();mpv_render_context_render(context,params);QQuickOpenGLUtils::resetOpenGLState();
    }
};
}
void registerVideoTypes(){static bool registered=false;if(!registered){registered=true;qmlRegisterType<VideoSurface>("ThinkSmart",1,0,"VideoSurface");qmlRegisterUncreatableType<VideoPlayer>("ThinkSmart",1,0,"VideoPlayer","Provided by the player controller");}}
void VideoSurface::setPlayer(VideoPlayer *player){
    if(player==m_player)return;if(m_player)disconnect(m_player,nullptr,this,nullptr);m_player=player;
    if(player)connect(player,&VideoPlayer::changed,this,[this](){update();});
    emit playerChanged();update();
}
QQuickFramebufferObject::Renderer *VideoSurface::createRenderer()const{return new VideoRenderer;}
VideoPlayer::~VideoPlayer(){if(m_handle){mpv_set_wakeup_callback(m_handle,nullptr,nullptr);mpv_terminate_destroy(m_handle);}}
bool VideoPlayer::initialize(){
    if(m_handle)return true;m_handle=mpv_create();if(!m_handle)return false;
    for(auto option:{std::pair{"vo","libmpv"},{"ao","alsa"},{"audio-device","alsa/default"},{"audio-format","s16"},{"audio-channels","stereo"},{"hwdec","v4l2m2m-copy"},{"hwdec-codecs","h264,hevc"},{"hwdec-software-fallback","no"},{"terminal","no"},{"msg-level","all=no"},{"input-default-bindings","no"},{"osc","no"},{"idle","yes"},{"cache","yes"},{"demuxer-max-bytes","33554432"},{"network-timeout","15"}})mpv_set_option_string(m_handle,option.first,option.second);
    if(mpv_initialize(m_handle)<0){mpv_terminate_destroy(m_handle);m_handle=nullptr;return false;}
    mpv_request_log_messages(m_handle,"warn");
    mpv_set_wakeup_callback(m_handle,[](void *data){auto *self=static_cast<VideoPlayer *>(data);QMetaObject::invokeMethod(self,[self](){self->drain();},Qt::QueuedConnection);},this);
    for(const char *name:{"pause","seekable","paused-for-cache","core-idle"})mpv_observe_property(m_handle,0,name,MPV_FORMAT_FLAG);
    for(const char *name:{"time-pos","duration","demuxer-cache-duration","container-fps"})mpv_observe_property(m_handle,0,name,MPV_FORMAT_DOUBLE);
    for(const char *name:{"video-params/w","video-params/h"})mpv_observe_property(m_handle,0,name,MPV_FORMAT_INT64);
    mpv_observe_property(m_handle,0,"video-codec",MPV_FORMAT_STRING);
    mpv_observe_property(m_handle,0,"hwdec-current",MPV_FORMAT_STRING);mpv_observe_property(m_handle,0,"track-list",MPV_FORMAT_NODE);return true;
}
void VideoPlayer::start(const QUrl &url,const QStringList &headers,double position,bool software,const QString &proxy,bool archive){
    if(!initialize()){rendererFailed();return;}
    m_controlError.clear();m_decoder.clear();m_diagnostics={{"requestedDecoder",software?"software":"v4l2m2m-copy"}};
    QJsonObject options{{"sid","no"},{"http-header-fields",headers.join(',')},{"hwdec",software?"no":"v4l2m2m-copy"},{"hwdec-software-fallback",software?"yes":"no"},{"vd",software?"lavc:h264":""}};
    options.insert("http-proxy",proxy);options.insert("tls-verify",archive?"yes":"no");options.insert("tls-ca-file",archive?"/etc/ssl/certs/ca-certificates.crt":"");
    if(archive)options.insert("hwdec-software-fallback","yes");
    // An explicit zero seek stalls initial HLS hardware decoding on the tested
    // FFmpeg/Venus stack. Let the demuxer open normally at the beginning.
    // Converted resume offsets are already applied by Jellyfin's stream URL.
    if(position>0)options.insert("start",QString::number(position));
    m_pending={"loadfile",url.toString(QUrl::FullyEncoded),"replace",-1,options};
    m_visible=m_running=true;emit changed();if(m_rendererReady)load();
}
QJsonObject VideoPlayer::diagnostics()const{auto result=m_diagnostics;result.insert("rendererReady",m_rendererReady);result.insert("pendingLoad",!m_pending.isEmpty());return result;}
void VideoPlayer::rendererReady(){m_rendererReady=true;load();}
void VideoPlayer::rendererFailed(){m_running=false;m_visible=false;emit event({{"event","end-file"},{"reason","error"}});emit changed();}
void VideoPlayer::load(){if(m_running&&!m_pending.isEmpty()){command({"set_property","pause",false});command(m_pending);m_pending={};}}
void VideoPlayer::command(const QJsonArray &arguments){
    if(!m_handle)return;int result=0;
    // set_property is an IPC operation, not an mpv input command.
    if(arguments.size()==3&&arguments[0]=="set_property"){
        Node value(arguments[2]);const auto property=arguments[1].toString().toUtf8();result=mpv_set_property_async(m_handle,1,property.constData(),MPV_FORMAT_NODE,&value.node);
    }else{Node node(arguments);result=mpv_command_node_async(m_handle,0,&node.node);}
    if(result<0&&m_running){m_controlError="Playback control unavailable";emit changed();}
}
void VideoPlayer::stop(){if(m_handle)command({"stop"});m_running=false;m_visible=false;m_pending={};emit changed();}
void VideoPlayer::selectTrack(const QString &type,int id){
    if(type!="audio"&&type!="sub")return;const auto &tracks=type=="audio"?m_audioTracks:m_subtitleTracks;
    for(auto v:tracks)if(v.toMap().value("id").toInt()==id){command({"set_property",type=="audio"?"aid":"sid",id==0?QJsonValue("no"):QJsonValue(id)});return;}
}
void VideoPlayer::drain(){
    if(!m_handle)return;
    while(auto *e=mpv_wait_event(m_handle,0)){
        if(e->event_id==MPV_EVENT_NONE)break;
        if(e->event_id==MPV_EVENT_LOG_MESSAGE){
            const auto *log=static_cast<mpv_event_log_message *>(e->data);
            QString message=QString::fromUtf8(log->text).simplified().left(600);
            if(message.contains("http",Qt::CaseInsensitive)||message.contains("authorization",Qt::CaseInsensitive)||message.contains("token",Qt::CaseInsensitive)||message.contains("password",Qt::CaseInsensitive)||message.contains("cookie",Qt::CaseInsensitive))message="Media warning (private details omitted)";
            auto warnings=m_diagnostics.value("warnings").toArray();if(warnings.size()>=12)warnings.removeFirst();warnings.append(QString::fromUtf8(log->prefix)+": "+message);m_diagnostics.insert("warnings",warnings);continue;
        }

        if(e->event_id==MPV_EVENT_FILE_LOADED)m_diagnostics.insert("fileLoaded",true);
        if((e->event_id==MPV_EVENT_COMMAND_REPLY||e->event_id==MPV_EVENT_SET_PROPERTY_REPLY)&&m_running){m_controlError=e->error<0?QString("Playback control unavailable"):QString();emit changed();}
        if(e->event_id==MPV_EVENT_PLAYBACK_RESTART&&m_running)emit event({{"event","playback-restart"}});
        else if(e->event_id==MPV_EVENT_PROPERTY_CHANGE){
            auto *p=static_cast<mpv_event_property *>(e->data);if(!p->data)continue;QJsonValue value;
            if(QByteArray(p->name)=="track-list"&&p->format==MPV_FORMAT_NODE){
                m_audioTracks.clear();m_subtitleTracks.clear();bool selectedSub=false;auto *tracks=static_cast<mpv_node *>(p->data);
                if(tracks->format==MPV_FORMAT_NODE_ARRAY)for(int i=0;i<tracks->u.list->num;i++){
                    auto &track=tracks->u.list->values[i];if(track.format!=MPV_FORMAT_NODE_MAP)continue;
                    QVariantMap fields;for(int j=0;j<track.u.list->num;j++){auto key=QByteArray(track.u.list->keys[j]);auto &v=track.u.list->values[j];if(key!="id"&&key!="type"&&key!="title"&&key!="lang"&&key!="selected")continue;
                        if(v.format==MPV_FORMAT_STRING)fields[QString::fromUtf8(key)]=QString::fromUtf8(v.u.string);
                        else if(v.format==MPV_FORMAT_INT64)fields[QString::fromUtf8(key)]=int(v.u.int64);
                        else if(v.format==MPV_FORMAT_FLAG)fields[QString::fromUtf8(key)]=bool(v.u.flag);
                    }
                    const auto type=fields.value("type").toString();QString label=fields.value("title").toString();if(label.isEmpty())label=fields.value("lang").toString();if(label.isEmpty())label="Track "+fields.value("id").toString();
                    fields["label"]=label.left(100);if(type=="audio")m_audioTracks.append(fields);else if(type=="sub"){selectedSub|=fields.value("selected").toBool();m_subtitleTracks.append(fields);}
                }
                m_subtitleTracks.prepend(QVariantMap{{"id",0},{"label","Off"},{"selected",!selectedSub}});emit changed();continue;
            }
            if(p->format==MPV_FORMAT_FLAG)value=bool(*static_cast<int *>(p->data));
            else if(p->format==MPV_FORMAT_DOUBLE)value=*static_cast<double *>(p->data);
            else if(p->format==MPV_FORMAT_INT64)value=double(*static_cast<int64_t *>(p->data));
            else if(p->format==MPV_FORMAT_STRING)value=QString::fromUtf8(*static_cast<char **>(p->data));
            if(QByteArray(p->name)=="pause"||QByteArray(p->name)=="paused-for-cache"||QByteArray(p->name)=="core-idle"||QByteArray(p->name)=="demuxer-cache-duration"||QByteArray(p->name)=="container-fps"||QByteArray(p->name)=="video-codec"||QByteArray(p->name)=="video-params/w"||QByteArray(p->name)=="video-params/h")m_diagnostics.insert(QString::fromUtf8(p->name),value);
            if(QByteArray(p->name)=="hwdec-current"){m_decoder=value.toString();emit changed();}
            if(m_running)emit event({{"event","property-change"},{"name",QString::fromUtf8(p->name)},{"data",value}});
        }else if(e->event_id==MPV_EVENT_END_FILE&&m_running){
            auto *end=static_cast<mpv_event_end_file *>(e->data);if(end->reason==MPV_END_FILE_REASON_STOP||end->reason==MPV_END_FILE_REASON_REDIRECT)continue;
            m_running=false;m_visible=false;emit event({{"event","end-file"},{"reason",end->reason==MPV_END_FILE_REASON_EOF?"eof":"error"}});emit changed();
        }
    }
}
