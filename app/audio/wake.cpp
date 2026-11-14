#include "pcm.h"
#include "detector.h"
#include <sherpa-onnx/c-api/c-api.h>
#include <speex/speex_resampler.h>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <memory>
#include <array>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <deque>
#include <chrono>
#include <cstring>
#include <ctime>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>
using namespace audio;
namespace {
// ALSA's capture driver caps its queue at 80 ms. Keep capture draining while
// ONNX runs bursty inference; never grow an unbounded audio backlog.
class Microphone {
public:
    using Block=std::array<int16_t,160>;
private:
    std::atomic_bool cancelled{false};
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Block> queue;
    bool ready=false,finished=false;
    QString error;
    std::thread reader;
    void run(){
        try {
            int status=0;std::unique_ptr<SpeexResamplerState,decltype(&speex_resampler_destroy)> resampler(speex_resampler_init(1,48000,16000,5,&status),speex_resampler_destroy);
            if(!resampler||status)throw std::runtime_error("Cannot create wake resampler");
            Pcm capture("hw:0,1",SND_PCM_STREAM_CAPTURE);
            if(snd_pcm_start(capture.handle)<0)throw std::runtime_error("Cannot start wake microphone");
            {std::lock_guard<std::mutex> lock(mutex);ready=true;}changed.notify_one();
            std::array<int16_t,960> stereo{};std::array<int16_t,480> mono{};
            while(!stopping&&!cancelled.load()){
                if(!capture.transfer(stereo.data(),480,false))break;
                for(int i=0;i<480;i++)mono[i]=stereo[i*2];
                Block samples{};spx_uint32_t in=480,out=160;
                if(speex_resampler_process_int(resampler.get(),0,mono.data(),&in,samples.data(),&out)||in!=480||out!=160)throw std::runtime_error("Wake resampling failed");
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    if(queue.size()>=50)throw std::runtime_error("Wake inference fell behind real time");
                    queue.push_back(samples);
                }
                changed.notify_one();
            }
        }catch(const std::exception &failure){std::lock_guard<std::mutex> lock(mutex);error=QString::fromLocal8Bit(failure.what());}
        {std::lock_guard<std::mutex> lock(mutex);finished=true;}changed.notify_one();
    }
public:
    Microphone():reader([this](){run();}){}
    ~Microphone(){close();}
    void close(){cancelled.store(true);if(reader.joinable())reader.join();}
    bool waitReady(){
        std::unique_lock<std::mutex> lock(mutex);
        while(!ready&&!finished&&!stopping)changed.wait_for(lock,std::chrono::milliseconds(20));
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        return ready&&!stopping;
    }
    bool next(Block &block){
        std::unique_lock<std::mutex> lock(mutex);
        while(queue.empty()&&!finished&&!stopping)changed.wait_for(lock,std::chrono::milliseconds(20));
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        if(stopping||queue.empty())return false;
        block=queue.front();queue.pop_front();return true;
    }
};

}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QCommandLineParser parser;parser.addHelpOption();
    parser.addOptions({{"model-dir","Local model directory","directory"},{"keywords-file","Tokenised Sheila keyword","file"},
        {"threshold","Trigger threshold (0..1)","threshold","0.35"},{"score","Keyword score (0..5)","score","1.5"},{"trailing-blanks","Trailing blank frames (1..5)","frames","2"},
        {"file","Offline test: raw mono S16_LE 16 kHz instead of microphone","file"}});parser.process(app);
    umask(0077);std::signal(SIGTERM,signalStop);std::signal(SIGINT,signalStop);std::signal(SIGHUP,signalStop);std::signal(SIGPIPE,SIG_IGN);
    const auto parent=getppid();prctl(PR_SET_PDEATHSIG,SIGTERM);if(getppid()!=parent)return 1;
    try {
        bool a=false,b=false,c=false;const float threshold=parser.value("threshold").toFloat(&a),score=parser.value("score").toFloat(&b);const int blanks=parser.value("trailing-blanks").toInt(&c);
        if(!a||!b||!c||!std::isfinite(threshold)||threshold<=0||threshold>1||!std::isfinite(score)||score<=0||score>5||blanks<1||blanks>5)throw std::runtime_error("Invalid local wake sensitivity");
        Detector detector(QDir(parser.value("model-dir")),parser.value("keywords-file"),threshold,score,blanks);
        if(stopping)return 0;
        if(parser.isSet("file")){
            QFile input(parser.value("file"));if(!input.open(QIODevice::ReadOnly)||input.size()%2||input.size()>16000*2*120)throw std::runtime_error("Invalid offline 16 kHz PCM fixture");
            while(!input.atEnd()&&!stopping){auto bytes=input.read(320);std::array<int16_t,160> samples{};std::memcpy(samples.data(),bytes.data(),size_t(bytes.size()));if(detector.put(samples.data(),160)){publish({{"event","wake"},{"word","Sheila"}});return 0;}}
            std::array<int16_t,160> tail{};for(int i=0;i<100&&!stopping;i++)if(detector.put(tail.data(),160)){publish({{"event","wake"},{"word","Sheila"}});return 0;}
            publish({{"event","no-wake"}});return 0;
        }
        Microphone capture;
        if(!capture.waitReady())return 0;
        publish({{"event","started"},{"word","Sheila"},{"local",true}});
        QElapsedTimer wall;wall.start();const double cpu=double(std::clock())/CLOCKS_PER_SEC;
        Microphone::Block samples{};
        for(int block=0;!stopping&&capture.next(samples);block++){
            if(detector.put(samples.data(),samples.size())){
                capture.close(); // Release the PCM before announcing activation.
                publish({{"event","wake"},{"word","Sheila"}});return 0;
            }
            if(block%500==499)publish({{"event","health"},{"cpuPercentOneCore",100*(double(std::clock())/CLOCKS_PER_SEC-cpu)/std::max(.001,wall.elapsed()/1000.)}});
        }
        return 0;
    }catch(const std::exception &error){publish({{"event","error"},{"message",QString::fromLocal8Bit(error.what())}});return 1;}
}
