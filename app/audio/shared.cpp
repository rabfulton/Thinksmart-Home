// Resident local microphone/AEC/KWS owner. Provider audio leaves only on stream.
#include "aec.h"
#include "ducking.h"
#include "detector.h"
#include "idle.h"
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QSocketNotifier>
#include <QTimer>
#include <QElapsedTimer>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <map>
#include <atomic>
#include <fcntl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace audio;
namespace {
struct Block {std::array<int16_t,240> mic{};std::array<int16_t,160> keyword{};double raw=0,clean=0,far=0,keywordPower=0;bool echoActive=false;};
class Graph {
    pw_thread_loop *loop=nullptr;pw_context *context=nullptr;pw_core *core=nullptr;
    pw_registry *registry=nullptr;pw_filter *filter=nullptr;pw_proxy *microphone=nullptr;
    pw_node *media=nullptr;spa_hook registryHook{},filterHook{},coreHook{};
    void *nearPort=nullptr,*farPort=nullptr;
    std::vector<pw_proxy *> links;
    std::map<uint32_t,std::string> nodes;
    struct Port {uint32_t id,node;std::string name;};std::vector<Port> ports;
    bool linked=false;
    EchoProcessor echo;
    IdleEchoPolicy echoPolicy;std::atomic_bool conversation{false};bool continuous=false;
    EchoProcessor::Block nearBlock{},farBlock{};size_t filled=0;
    std::unique_ptr<SpeexResamplerState,decltype(&speex_resampler_destroy)> down{nullptr,speex_resampler_destroy};
    std::mutex mutex;std::deque<Block> queue;std::string failure;
    static const char *property(const spa_dict *props,const char *key){auto *v=spa_dict_lookup(props,key);return v?v:"";}
    static void global(void *data,uint32_t id,uint32_t,const char *type,uint32_t,const spa_dict *props){
        auto &s=*static_cast<Graph *>(data);if(!props)return;
        if(std::strcmp(type,PW_TYPE_INTERFACE_Node)==0){s.nodes[id]=property(props,PW_KEY_NODE_NAME);
            if(s.nodes[id]=="thinksmart.media")s.media=static_cast<pw_node *>(pw_registry_bind(s.registry,id,type,PW_VERSION_NODE,0));
        }else if(std::strcmp(type,PW_TYPE_INTERFACE_Port)==0)s.ports.push_back({id,uint32_t(std::strtoul(property(props,PW_KEY_NODE_ID),nullptr,10)),property(props,PW_KEY_PORT_NAME)});
        s.link();
    }
    static void removed(void *data,uint32_t id){
        auto &s=*static_cast<Graph *>(data);bool affected=false;
        s.ports.erase(std::remove_if(s.ports.begin(),s.ports.end(),[&](const Port &p){if(p.id!=id)return false;affected=true;return true;}),s.ports.end());
        if(s.nodes.erase(id))affected=true;
        if(affected&&s.linked){s.linked=false;for(auto *p:s.links)pw_proxy_destroy(p);s.links.clear();s.link();}
    }
    void link(){
        if(linked)return;
        uint32_t mic=SPA_ID_INVALID,far=SPA_ID_INVALID,nearInput=SPA_ID_INVALID,farInput=SPA_ID_INVALID;
        for(auto &p:ports){const auto &name=nodes[p.node];
            if(name=="thinksmart.microphone"&&p.name=="capture_FL")mic=p.id;
            if(name=="thinksmart.mix"&&p.name=="monitor_MONO")far=p.id;
            if(name=="thinksmart.echo"&&p.name=="microphone")nearInput=p.id;
            if(name=="thinksmart.echo"&&p.name=="reference")farInput=p.id;
        }
        if(mic==SPA_ID_INVALID||far==SPA_ID_INVALID||nearInput==SPA_ID_INVALID||farInput==SPA_ID_INVALID)return;
        for(auto pair:{std::pair{mic,nearInput},std::pair{far,farInput}}){
            auto nodeFor=[this](uint32_t id){for(auto &p:ports)if(p.id==id)return p.node;return SPA_ID_INVALID;};
            auto *props=pw_properties_new("link.output.node",std::to_string(nodeFor(pair.first)).c_str(),"link.input.node",std::to_string(nodeFor(pair.second)).c_str(),"link.output.port",std::to_string(pair.first).c_str(),"link.input.port",std::to_string(pair.second).c_str(),"object.linger","false",nullptr);
            auto *proxy=static_cast<pw_proxy *>(pw_core_create_object(core,"link-factory",PW_TYPE_INTERFACE_Link,PW_VERSION_LINK,&props->dict,0));pw_properties_free(props);
            if(proxy)links.push_back(proxy);else setError("Cannot link echo reference");
        }linked=true;
    }
    void setError(const char *message){std::lock_guard<std::mutex> lock(mutex);failure=message;}
    static void coreError(void *data,uint32_t,int,int,const char *message){static_cast<Graph *>(data)->setError(message?message:"Audio server failed");}
    static void process(void *data,spa_io_position *position){
        auto &s=*static_cast<Graph *>(data);auto n=position->clock.duration;
        if(n==0)return;
        if(n>8192){s.setError("Invalid shared graph quantum");return;}
        auto *near=static_cast<float *>(pw_filter_get_dsp_buffer(s.nearPort,n));auto *far=static_cast<float *>(pw_filter_get_dsp_buffer(s.farPort,n));
        if(!near||!far)return;
        try {
            for(size_t frame=0;frame<n;frame++){
            s.nearBlock[s.filled]=int16_t(std::clamp(near[frame],-1.f,.999969f)*32768);s.farBlock[s.filled]=int16_t(std::clamp(far[frame],-1.f,.999969f)*32768);
            if(++s.filled!=480)continue;s.filled=0;auto &a=s.nearBlock;auto &b=s.farBlock;
            Block block;block.far=energy(b.data(),480);
            block.echoActive=s.continuous||s.echoPolicy.process(block.far,s.conversation.load(std::memory_order_relaxed));
            if(block.echoActive)s.echo.render(b);
            s.echo.capture(a,100,block.echoActive);
            spx_uint32_t in=480,out=240;
            if(speex_resampler_process_int(s.down.get(),0,s.echo.clean().data(),&in,block.mic.data(),&out)||in!=480||out!=240)throw std::runtime_error("Shared microphone resampler failed");
            // The linear residual preserves near-end speech without AEC's NLP gate.
            auto &clean=s.echo.linear();for(size_t i=0;i<160;i++)block.keyword[i]=int16_t(std::clamp(clean[i],-1.f,.999969f)*32768);
            block.raw=energy(a.data(),480);block.clean=energy(s.echo.clean().data(),480);block.keywordPower=energy(block.keyword.data(),160);
            std::lock_guard<std::mutex> lock(s.mutex);if(s.queue.size()>=100){s.failure="Shared microphone consumer fell behind";return;}s.queue.push_back(block);
            }
        }catch(const std::exception &e){s.setError(e.what());}
    }
public:
    explicit Graph(bool alwaysProcess=false):continuous(alwaysProcess){
        int error=0;down.reset(speex_resampler_init(1,48000,24000,5,&error));if(!down||error)throw std::runtime_error("Cannot initialise shared resampler");
        pw_init(nullptr,nullptr);loop=pw_thread_loop_new("thinksmart-shared",nullptr);if(!loop)throw std::runtime_error("Cannot create audio loop");
        context=pw_context_new(pw_thread_loop_get_loop(loop),nullptr,0);core=context?pw_context_connect(context,nullptr,0):nullptr;if(!core)throw std::runtime_error("Cannot connect shared audio server");
        static const pw_core_events ce=[](){pw_core_events e{};e.version=PW_VERSION_CORE_EVENTS;e.error=coreError;return e;}();pw_core_add_listener(core,&coreHook,&ce,this);
        registry=pw_core_get_registry(core,PW_VERSION_REGISTRY,0);static const pw_registry_events re=[](){pw_registry_events e{};e.version=PW_VERSION_REGISTRY_EVENTS;e.global=global;e.global_remove=removed;return e;}();pw_registry_add_listener(registry,&registryHook,&re,this);
        auto *props=pw_properties_new("factory.name","api.alsa.pcm.source","node.name","thinksmart.microphone","media.class","Audio/Source","api.alsa.path","hw:0,1","api.alsa.disable-mmap","true","api.alsa.period-size","480","api.alsa.period-num","8","audio.format","S16LE","audio.rate","48000","audio.channels","2","audio.position","[ FL FR ]","adapter.auto-port-config","{ mode = dsp position = preserve }",nullptr);
        microphone=static_cast<pw_proxy *>(pw_core_create_object(core,"adapter",PW_TYPE_INTERFACE_Node,PW_VERSION_NODE,&props->dict,0));pw_properties_free(props);if(!microphone)throw std::runtime_error("Cannot create microphone source");
        filter=pw_filter_new(core,"ThinkSmart echo cancellation",pw_properties_new("node.name","thinksmart.echo","media.class","Audio/Filter","node.always-process","true","node.autoconnect","false","node.latency","480/48000","node.force-quantum","480","node.force-rate","48000",nullptr));
        static const pw_filter_events fe=[](){pw_filter_events e{};e.version=PW_VERSION_FILTER_EVENTS;e.process=process;return e;}();pw_filter_add_listener(filter,&filterHook,&fe,this);
        auto add=[this](const char *name){return pw_filter_add_port(filter,PW_DIRECTION_INPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,pw_properties_new("format.dsp","32 bit float mono audio","port.name",name,nullptr),nullptr,0);};nearPort=add("microphone");farPort=add("reference");
        if(!nearPort||!farPort||pw_filter_connect(filter,PW_FILTER_FLAG_RT_PROCESS,nullptr,0)<0||pw_thread_loop_start(loop)<0)throw std::runtime_error("Cannot start shared audio filter");
    }
    ~Graph(){if(loop)pw_thread_loop_stop(loop);if(filter)pw_filter_destroy(filter);for(auto *p:links)pw_proxy_destroy(p);if(media)pw_proxy_destroy(reinterpret_cast<pw_proxy *>(media));if(microphone)pw_proxy_destroy(microphone);if(registry)pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));if(core)pw_core_disconnect(core);if(context)pw_context_destroy(context);if(loop)pw_thread_loop_destroy(loop);}
    std::deque<Block> take(){std::lock_guard<std::mutex> lock(mutex);if(!failure.empty())throw std::runtime_error(failure);std::deque<Block> result;result.swap(queue);return result;}
    void setConversation(bool active){conversation.store(active,std::memory_order_relaxed);}
    void duck(float gain){
        pw_thread_loop_lock(loop);
        if(media){uint8_t buffer[256];spa_pod_builder b=SPA_POD_BUILDER_INIT(buffer,sizeof(buffer));auto *param=static_cast<spa_pod *>(spa_pod_builder_add_object(&b,SPA_TYPE_OBJECT_Props,SPA_PARAM_Props,SPA_PROP_channelVolumes,SPA_POD_Array(sizeof(float),SPA_TYPE_Float,1,&gain)));pw_node_set_param(media,SPA_PARAM_Props,0,param);}
        pw_thread_loop_unlock(loop);
    }
};
class Keywords {
    Detector detector;std::mutex mutex;std::condition_variable cv;std::deque<std::array<int16_t,160>> queue;bool quit=false;std::atomic_bool fired{false},failed{false};
    std::atomic_uint decoded{0};
    std::thread thread;
public:
    Keywords(const QCommandLineParser &p):detector(QDir(p.value("model-dir")),p.value("keywords-file"),p.value("threshold").toFloat(),p.value("score").toFloat(),p.value("trailing-blanks").toInt()),thread([this](){
        try {QElapsedTimer cooldown;cooldown.start();bool first=true;std::array<int16_t,640> batch;size_t filled=0;
            while(true){std::array<int16_t,160> block;{std::unique_lock<std::mutex> l(mutex);cv.wait(l,[this](){return quit||!queue.empty();});if(quit)return;block=queue.front();queue.pop_front();}
            std::copy(block.begin(),block.end(),batch.begin()+filled);filled+=block.size();if(filled<batch.size())continue;filled=0;
            if(detector.put(batch.data(),batch.size())){if(first||cooldown.elapsed()>2000){first=false;fired=true;cooldown.restart();}detector.resetKeyword();}
            decoded.fetch_add(batch.size(),std::memory_order_relaxed);
        }}catch(...){failed=true;}
    }){}
    ~Keywords(){{std::lock_guard<std::mutex> l(mutex);quit=true;}cv.notify_one();thread.join();}
    void put(const Block &b){std::lock_guard<std::mutex> l(mutex);if(queue.size()>=100||failed)throw std::runtime_error("Keyword inference fell behind");queue.push_back(b.keyword);cv.notify_one();}
    bool wake(){return fired.exchange(false);}
    unsigned processedFrames(){return decoded.exchange(0,std::memory_order_relaxed);}
};
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QCommandLineParser p;p.addHelpOption();p.addOptions({{"model-dir","Model directory","path"},{"keywords-file","Keyword tokens","path"},{"threshold","Trigger threshold","value",".35"},{"score","Keyword score","value","1.5"},{"trailing-blanks","Trailing blank frames","value","2"},{"continuous","Disable idle echo savings for comparison/rollback"}});p.process(app);
    const bool continuous=p.isSet("continuous")||qEnvironmentVariableIntValue("THINKSMART_AUDIO_CONTINUOUS")==1;
    umask(0077);std::signal(SIGTERM,signalStop);std::signal(SIGINT,signalStop);std::signal(SIGPIPE,SIG_IGN);const auto parent=getppid();prctl(PR_SET_PDEATHSIG,SIGTERM);if(getppid()!=parent)return 1;
    fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
    try {
        Keywords keywords(p);Graph graph(continuous);QElapsedTimer alive,lastBlock;alive.start();lastBlock.start();
        QByteArray commands,packet;std::deque<QByteArray> history,pending;bool preparing=false,streaming=false,ready=false;int blocks=0,echoBlocks=0;double raw=0,clean=0,far=0,keywordPower=0;
        DuckEnvelope envelope;
        auto idle=[&](){preparing=streaming=false;pending.clear();envelope.setDucked(false);graph.setConversation(false);};
        QSocketNotifier input(STDIN_FILENO,QSocketNotifier::Read);QObject::connect(&input,&QSocketNotifier::activated,&app,[&](){char b[4096];auto n=::read(STDIN_FILENO,b,sizeof(b));if(n==0){stopping=1;return;}if(n>0)commands.append(b,n);});
        QTimer timer;timer.setInterval(10);QObject::connect(&timer,&QTimer::timeout,&app,[&](){try {
            if(stopping){idle();graph.duck(1);app.quit();return;}
            if(commands.size()>8192)throw std::runtime_error("Shared audio command exceeds limit");
            while(commands.contains('\n')){auto e=commands.indexOf('\n');auto c=QJsonDocument::fromJson(commands.left(e)).object();commands.remove(0,e+1);auto type=c.value("type").toString();
                if(type=="prepare"){if(!preparing&&!streaming){pending=history;preparing=true;}envelope.setDucked(true);graph.setConversation(true);}
                else if(type=="stream"){streaming=true;preparing=false;graph.setConversation(true);}
                else if(type=="duck")envelope.setDucked(true);
                else if(type=="unduck")envelope.setDucked(false);
                else if(type=="idle")idle();else if(type=="stop"){stopping=1;return;}else throw std::runtime_error("Unknown shared audio command");
            }
            for(const auto &b:graph.take()){
                lastBlock.restart();
                if(!ready){ready=true;publish({{"event","started"},{"local",true},{"persistent",true}});}
                keywords.put(b);packet.append(reinterpret_cast<const char *>(b.mic.data()),480);raw+=b.raw;clean+=b.clean;far+=b.far;keywordPower+=b.keywordPower;echoBlocks+=b.echoActive;blocks++;
                if(blocks%2==0){history.push_back(packet);if(history.size()>25)history.pop_front();if(streaming||preparing){pending.push_back(packet);if(pending.size()>(streaming?600u:500u))throw std::runtime_error("Voice microphone buffer limit exceeded");}packet.clear();}
                if(blocks%25==0){publish({{"event","health"},{"rawDbfs",db(raw/25)},{"cleanDbfs",db(clean/25)},{"speakerDbfs",db(far/25)},{"keywordDbfs",db(keywordPower/25)},{"echoActivePercent",echoBlocks*4},{"keywordActivePercent",keywords.processedFrames()/40.}});raw=clean=far=keywordPower=0;echoBlocks=0;}
            }
            // Drain startup audio at a bounded catch-up rate, avoiding a large
            // WebSocket burst after a slow handshake. Live audio joins the queue.
            for(int i=0;streaming&&i<2&&!pending.empty();i++){publish({{"event","mic"},{"audio",QString::fromLatin1(pending.front().toBase64())}});pending.pop_front();}
            if(keywords.wake()){if(!preparing&&!streaming){pending=history;preparing=true;}envelope.setDucked(true);graph.setConversation(true);publish({{"event","wake"},{"word","Sheila"}});}
            if(envelope.advance())graph.duck(envelope.gain());
            if(ready&&lastBlock.elapsed()>2000)throw std::runtime_error("Shared microphone stopped delivering audio");
            if(!ready&&alive.elapsed()>7000)throw std::runtime_error("Shared audio graph did not deliver microphone/reference blocks");
        }catch(const std::exception &e){idle();graph.duck(1);publish({{"event","error"},{"message",QString::fromLocal8Bit(e.what())}});app.exit(1);}});timer.start();return app.exec();
    }catch(const std::exception &e){publish({{"event","error"},{"message",QString::fromLocal8Bit(e.what())}});return 1;}
}
