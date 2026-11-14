#include "aec.h"
#include "pcm.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QMap>
#include <QByteArray>
#include <deque>
#include <memory>
#include <fcntl.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <poll.h>
#include <cstring>
using namespace audio;
namespace {
// A single thread owns ALSA, the reference, both resamplers and AEC. The parent
// exchanges bounded JSON lines. PCM is memory-only, mono S16_LE at 24 kHz.
void send(const QJsonObject &event) {
    QByteArray line=QJsonDocument(event).toJson(QJsonDocument::Compact)+'\n';
    qsizetype pos=0;
    while(pos<line.size()&&!stopping){
        const auto n=::write(STDOUT_FILENO,line.constData()+pos,size_t(line.size()-pos));
        if(n>0){pos+=n;continue;}
        if(n<0&&errno==EINTR)continue;
        if(n<0&&errno==EAGAIN){pollfd p{STDOUT_FILENO,POLLOUT,0};if(poll(&p,1,20)>0)continue;}
        throw std::runtime_error("Voice parent stopped consuming audio");
    }
}
struct Packet { QString item; int index=0; QByteArray pcm; qsizetype offset=0; };
struct Span { QString item; int index; qint64 start; int frames; };
struct Heard { int index=0; qint64 frames=0; };
class Stream {
    bool playbackOnly;
    Pcm playback;
    std::unique_ptr<Pcm> capture;
    std::unique_ptr<EchoProcessor> echo;
    std::unique_ptr<SpeexResamplerState,decltype(&speex_resampler_destroy)> up{nullptr,speex_resampler_destroy},down{nullptr,speex_resampler_destroy};
    std::deque<Packet> queue;
    std::deque<Span> spans;
    QMap<QString,Heard> heard;
    QByteArray input,mic;
    qint64 written=0,queued=0;
    std::array<int16_t,960> stereo{},nearStereo{};
    EchoProcessor::Block render{},near{};
    QElapsedTimer wall;
    void prime() { stereo.fill(0);render.fill(0);for(int i=0;i<2&&!stopping;i++){playback.transfer(stereo.data(),480,true);if(echo)echo->render(render);written+=480;} }
    void account() {
        const qint64 cursor=written-playback.delay()-speex_resampler_get_output_latency(up.get());
        while(!spans.empty()&&spans.front().start<cursor){
            auto &span=spans.front();int n=int(std::min<qint64>(span.frames,cursor-span.start));
            auto &h=heard[span.item];h.index=span.index;h.frames+=n;span.start+=n;span.frames-=n;
            if(!span.frames)spans.pop_front();else break;
        }
        // Completed, old items are no longer needed for interruption accounting.
        while(heard.size()>16)heard.erase(heard.begin());
    }
    QJsonArray played()const { QJsonArray a;for(auto i=heard.cbegin();i!=heard.cend();++i)a.append(QJsonObject{{"item",i.key()},{"index",i->index},{"ms",int(i->frames/48)}});return a; }
    void commands() {
        char chunk[16384];
        for(int i=0;i<32;i++){
            auto n=::read(STDIN_FILENO,chunk,sizeof(chunk));
            if(n==0){stopping=1;break;}
            if(n<0){if(errno==EAGAIN)break;if(errno==EINTR)continue;throw std::runtime_error("Voice input pipe failed");}
            input.append(chunk,n);
            if(input.size()>1024*1024)throw std::runtime_error("Voice command exceeds limit");
        }
        while(input.contains('\n')){
            const auto end=input.indexOf('\n');QJsonParseError error;
            auto object=QJsonDocument::fromJson(input.left(end),&error).object();input.remove(0,end+1);
            if(error.error!=QJsonParseError::NoError)throw std::runtime_error("Invalid voice command");
            const auto type=object.value("type").toString();
            if(type=="audio"){
                auto bytes=QByteArray::fromBase64(object.value("audio").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
                auto item=object.value("item").toString();
                if(bytes.size()%2||bytes.size()>256*1024||item.isEmpty())throw std::runtime_error("Invalid speaker packet");
                queued+=bytes.size();if(queued>24000*2*30)throw std::runtime_error("Speaker buffer exceeded 30 seconds");
                if(!bytes.isEmpty())queue.push_back({item,object.value("index").toInt(),bytes,0});
            }else if(type=="flush"){
                account();queue.clear();queued=0;spans.clear();
                if(snd_pcm_drop(playback.handle)<0||snd_pcm_prepare(playback.handle)<0)throw std::runtime_error("Cannot clear speaker queue");
                // Keep the trained canceller; only the output timeline changes.
                written=0;prime();
                send({{"event","flushed"},{"request",object.value("request")},{"played",played()}});
            }else if(type=="stop")stopping=1;
            else throw std::runtime_error("Unknown voice command");
        }
    }
public:
    Stream(bool shared):playbackOnly(shared),playback(shared?"thinksmart_voice":"default",SND_PCM_STREAM_PLAYBACK){
        if(!shared){capture=std::make_unique<Pcm>("hw:0,1",SND_PCM_STREAM_CAPTURE);echo=std::make_unique<EchoProcessor>();}
        int error=0;up.reset(speex_resampler_init(1,24000,48000,5,&error));
        if(!up||error)throw std::runtime_error("Cannot initialise speaker resampler");
        down.reset(speex_resampler_init(1,48000,24000,5,&error));
        if(!down||error)throw std::runtime_error("Cannot initialise microphone resampler");
    }
    void run(){
        wall.start();prime();
        if(capture&&snd_pcm_start(capture->handle)<0)throw std::runtime_error("Cannot start microphone");
        send({{"event","started"},{"rate",24000},{"aec",true}});
        double rawPower=0,cleanPower=0,speakerPower=0,meterClean=0,meterSpeaker=0;int clipping=0;
        for(int block=0;!stopping;block++){
            if(wall.elapsed()>10*60*1000)throw std::runtime_error("Ten minute conversation limit reached");
            commands();if(stopping)break;
            std::array<int16_t,240> cloud{};int count=0;
            // Usually one item, but preserve item boundaries even within a block.
            while(count<240&&!queue.empty()){
                auto &p=queue.front();int n=int(std::min<qsizetype>(240-count,(p.pcm.size()-p.offset)/2));
                std::memcpy(cloud.data()+count,p.pcm.constData()+p.offset,size_t(n*2));
                spans.push_back({p.item,p.index,written+count*2,n*2});
                count+=n;p.offset+=n*2;queued-=n*2;
                if(p.offset==p.pcm.size())queue.pop_front();
            }
            spx_uint32_t in=240,out=480;
            if(speex_resampler_process_int(up.get(),0,cloud.data(),&in,render.data(),&out)||in!=240||out!=480)throw std::runtime_error("Speaker resampling failed");
            // This exact render block is both written and passed to AEC.
            for(int i=0;i<480;i++)stereo[i*2]=stereo[i*2+1]=render[i];
            if(echo)echo->render(render);
            if(!playback.transfer(stereo.data(),480,true)||(capture&&!capture->transfer(nearStereo.data(),480,false)))break;
            written+=480;account();
            for(int i=0;i<480;i++){near[i]=nearStereo[i*2];if(std::abs(int(near[i]))>=32700)clipping++;}
            if(echo)echo->capture(near,std::clamp((playback.delay()+capture->delay()+480)/48,0,500));
            std::array<int16_t,240> cleaned{};in=480;out=240;
            if(speex_resampler_process_int(down.get(),0,(echo?echo->clean():near).data(),&in,cleaned.data(),&out)||in!=480||out!=240)throw std::runtime_error("Microphone resampling failed");
            if(!playbackOnly)mic.append(reinterpret_cast<const char *>(cleaned.data()),480);
            if(!playbackOnly&&block%2==1){send({{"event","mic"},{"audio",QString::fromLatin1(mic.toBase64())}});mic.clear();}
            const double cleanEnergy=energy((echo?echo->clean():near).data(),480),speakerEnergy=energy(render.data(),480);
            rawPower+=energy(near.data(),480);cleanPower+=cleanEnergy;speakerPower+=speakerEnergy;
            meterClean+=cleanEnergy;meterSpeaker+=speakerEnergy;
            // Small, local-only amplitude telemetry: the audio/API path is unchanged.
            if(block%5==4){send({{"event","meter"},{"cleanDbfs",db(meterClean/5)},{"speakerDbfs",db(meterSpeaker/5)}});meterClean=meterSpeaker=0;}
            if(block%25==24){send({{"event","levels"},{"rawDbfs",db(rawPower/25)},{"cleanDbfs",db(cleanPower/25)},{"speakerDbfs",db(speakerPower/25)},
                {"clippedPercent",100.0*clipping/(25*480)},{"playing",!queue.empty()||!spans.empty()},{"played",played()}});
                rawPower=cleanPower=speakerPower=0;clipping=0;}
        }
    }
};
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);umask(0077);
    std::signal(SIGTERM,signalStop);std::signal(SIGINT,signalStop);std::signal(SIGHUP,signalStop);std::signal(SIGPIPE,SIG_IGN);
    const auto parent=getppid();prctl(PR_SET_PDEATHSIG,SIGTERM);if(getppid()!=parent)return 1;
    fcntl(STDIN_FILENO,F_SETFL,fcntl(STDIN_FILENO,F_GETFL)|O_NONBLOCK);
    fcntl(STDOUT_FILENO,F_SETFL,fcntl(STDOUT_FILENO,F_GETFL)|O_NONBLOCK);
    try { Stream stream(app.arguments().contains("--playback-only"));stream.run();return 0; }
    catch(const std::exception &error){try{send({{"event","error"},{"message",QString::fromLocal8Bit(error.what())}});}catch(...){}return 1;}
}
