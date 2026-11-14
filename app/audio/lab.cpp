#include "aec.h"
#include "pcm.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QElapsedTimer>
#include <QDir>
#include <alsa/asoundlib.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <csignal>
#include <stdexcept>
#include <algorithm>
#include <random>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
namespace {
using namespace audio;
class Recording {
public:
    QFile raw,clean,linear,reference;
    explicit Recording(const QString &directory){
        if(directory.isEmpty())return;
        if(!QDir().mkpath(directory))throw std::runtime_error("Cannot create private recording directory");
        QFile::setPermissions(directory,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        for(auto pair:{std::pair<QFile *,QString>{&raw,"mic-raw-48k.s16"},{&clean,"mic-clean-48k.s16"},{&linear,"mic-linear-16k.s16"},{&reference,"speaker-48k.s16"}}){
            pair.first->setFileName(directory+"/"+pair.second);
            if(QFile::exists(pair.first->fileName()))throw std::runtime_error("Recording exists; choose a new directory");
            if(!pair.first->open(QIODevice::WriteOnly|QIODevice::NewOnly))throw std::runtime_error("Cannot create recording");
            pair.first->setPermissions(QFile::ReadOwner|QFile::WriteOwner);
        }
    }
    void put(QFile &file,const int16_t *data,size_t size){if(file.isOpen()&&file.write(reinterpret_cast<const char *>(data),qint64(size*2))!=qint64(size*2))throw std::runtime_error("Recording write failed");}
};
int selfTest(){
    // Exercise the real AEC3 with a delayed, filtered echo; then add unrelated
    // near-end speech-like audio to check that a blanket noise gate cannot pass.
    EchoProcessor echo;std::mt19937 random(8123);std::normal_distribution<float> noise(0,.12f);
    std::vector<int16_t> far(48000*24);float state=0;
    for(size_t i=0;i<far.size();i++){state=.85f*state+.15f*noise(random);far[i]=int16_t(state*32768);}
    double rawEcho=0,cleanEcho=0,nearPower=0,cleanNear=0;bool linear=false;
    for(int block=0;block<2400;block++){
        EchoProcessor::Block render{},mic{};
        for(int i=0;i<480;i++){
            int t=block*480+i;render[i]=far[size_t(t)];
            double reflected=t>=2400?.6*far[size_t(t-2400)]:0;
            double near=block>=1600?1600*std::sin(t*.071)*(0.65+0.35*std::sin(t*.00071)):0;
            mic[i]=int16_t(reflected+near);
            if(block>=1800)nearPower+=(near/32768)*(near/32768);
        }
        echo.render(render);echo.capture(mic,50);linear|=echo.hasLinear();
        if(block>=1000&&block<1500){rawEcho+=energy(mic.data(),480);cleanEcho+=energy(echo.clean().data(),480);}
        if(block>=1800)cleanNear+=energy(echo.clean().data(),480)*480;
    }
    double reduction=db(rawEcho)-db(cleanEcho),retained=db(cleanNear)-db(nearPower);
    publish({{"event","self-test"},{"echoReductionDb",reduction},{"nearEndLevelChangeDb",retained},{"linearOutput",linear}});
    // Ten dB means at least 90% echo-energy reduction in this controlled signal,
    // while a near-end level check rejects a blanket mute/noise gate.
    return reduction>=10&&retained>-9&&retained<9&&linear?0:1;
}
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QCommandLineParser parser;parser.addHelpOption();
    parser.addOptions({{"self-test","Run an offline synthetic AEC check without audio devices"},
        {"reference","Raw mono 48 kHz S16_LE playback file","path"},
        {"seconds","Maximum duration (1..120 seconds)","seconds","20"},
        {"level","Playback amplitude multiplier (0..0.75)","level","0.25"},
        {"delay-ms","Fixed reference delay (0..500); omit for ALSA queue estimate","ms"},
        {"record-dir","Opt in to private raw/clean/reference PCM recordings","directory"}});
    parser.process(app);
    try {
        if(parser.isSet("self-test"))return selfTest();
        bool secondsOk=false,levelOk=false;int seconds=parser.value("seconds").toInt(&secondsOk);double level=parser.value("level").toDouble(&levelOk);
        if(!secondsOk||seconds<1||seconds>120||!levelOk||!std::isfinite(level)||level<0||level>.75)throw std::runtime_error("Invalid duration or playback level");
        int fixedDelay=-1;if(parser.isSet("delay-ms")){bool ok=false;fixedDelay=parser.value("delay-ms").toInt(&ok);if(!ok||fixedDelay<0||fixedDelay>500)throw std::runtime_error("Invalid echo delay");}
        QFile reference(parser.value("reference"));
        if(!reference.open(QIODevice::ReadOnly)||reference.size()<960||reference.size()>48000*2*120||reference.size()%2)throw std::runtime_error("A raw mono 48 kHz S16_LE reference file is required");
        const auto bytes=reference.readAll();std::vector<int16_t> speech(size_t(bytes.size()/2));std::memcpy(speech.data(),bytes.data(),size_t(bytes.size()));
        umask(0077);std::signal(SIGTERM,signalStop);std::signal(SIGINT,signalStop);std::signal(SIGHUP,signalStop);
        const pid_t parent=getppid();prctl(PR_SET_PDEATHSIG,SIGTERM);if(getppid()!=parent)return 1;
        Recording recording(parser.value("record-dir"));EchoProcessor echo;
        Pcm playback("default",SND_PCM_STREAM_PLAYBACK),capture("hw:0,1",SND_PCM_STREAM_CAPTURE);
        std::array<int16_t,960> stereo{},input{};EchoProcessor::Block mono{},near{};
        // Prime one short queue of silence before capture. The rendering side
        // owns the only playback PCM and all reference samples.
        for(int i=0;i<2;i++){if(!playback.transfer(stereo.data(),480,true))return 0;echo.render(mono);}
        if(snd_pcm_start(capture.handle)<0)throw std::runtime_error("Cannot start microphone capture");
        publish({{"event","started"},{"sampleRate",48000},{"channels",2},{"format","S16_LE"},{"recording",!parser.value("record-dir").isEmpty()}});
        QElapsedTimer wall;wall.start();size_t position=0;double rawPower=0,cleanPower=0,linearPower=0,referencePower=0;int clipped=0;
        const double cpuStart=double(std::clock())/CLOCKS_PER_SEC;
        for(int block=0;block<seconds*100&&!stopping;block++){
            if(wall.elapsed()>(seconds+5)*1000)throw std::runtime_error("Audio processing fell behind real time");
            for(int i=0;i<480;i++){
                // A quiet startup lets the amplifier settle, followed by looping
                // offline test speech. No microphone audio is sent to the speaker.
                mono[i]=block<100?0:int16_t(std::clamp(std::lround(speech[position++%speech.size()]*level),-32768L,32767L));
                stereo[i*2]=stereo[i*2+1]=mono[i];
            }
            echo.render(mono);
            if(!playback.transfer(stereo.data(),480,true)||!capture.transfer(input.data(),480,false))break;
            const int delay=fixedDelay>=0?fixedDelay:std::clamp((playback.delay()+capture.delay()+480)/48,0,500);
            for(int i=0;i<480;i++){near[i]=input[i*2];if(std::abs(int(near[i]))>=32700)clipped++;}
            echo.capture(near,delay);
            rawPower+=energy(near.data(),480);cleanPower+=energy(echo.clean().data(),480);referencePower+=energy(mono.data(),480);
            std::array<int16_t,160> linear{};
            if(echo.hasLinear())for(int i=0;i<160;i++){double x=echo.linear()[i];linearPower+=x*x/160;linear[i]=int16_t(std::clamp(std::lround(x*32768),-32768L,32767L));}
            recording.put(recording.raw,near.data(),480);recording.put(recording.clean,echo.clean().data(),480);recording.put(recording.reference,mono.data(),480);recording.put(recording.linear,linear.data(),160);
            if(block%25==24){
                auto stats=echo.statistics();QJsonObject event{{"event","levels"},{"elapsed",(block+1)/100.0},{"rawDbfs",db(rawPower/25)},{"cleanDbfs",db(cleanPower/25)},
                    {"linearDbfs",echo.hasLinear()?QJsonValue(db(linearPower/25)):QJsonValue()}, {"speakerDbfs",db(referencePower/25)},
                    {"levelDifferenceDb",db(rawPower)-db(cleanPower)},{"clippedPercent",100.0*clipped/(25*480)},{"queueDelayMs",delay},
                    {"cpuPercent",100*(double(std::clock())/CLOCKS_PER_SEC-cpuStart)/std::max(.001,wall.elapsed()/1000.0)}};
                if(stats.echo_return_loss_enhancement)event.insert("estimatedErleDb",*stats.echo_return_loss_enhancement);
                if(stats.delay_ms)event.insert("estimatedDelayMs",*stats.delay_ms);
                publish(event);rawPower=cleanPower=linearPower=referencePower=0;clipped=0;
            }
        }
        publish({{"event","stopped"},{"seconds",wall.elapsed()/1000.0}});return 0;
    }catch(const std::exception &error){publish({{"event","error"},{"message",QString::fromLocal8Bit(error.what())}});return 1;}
}
