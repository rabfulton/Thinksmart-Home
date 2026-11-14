#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <alsa/asoundlib.h>
#include <csignal>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <string>
namespace audio {
inline volatile sig_atomic_t stopping=0;
inline void signalStop(int){stopping=1;}
inline void publish(const QJsonObject &event){const auto bytes=QJsonDocument(event).toJson(QJsonDocument::Compact);std::fwrite(bytes.data(),1,bytes.size(),stdout);std::fputc('\n',stdout);std::fflush(stdout);}
inline double energy(const int16_t *data,size_t size){double total=0;for(size_t i=0;i<size;i++){double x=data[i]/32768.0;total+=x*x;}return total/std::max(size,size_t(1));}
inline double db(double power){return 10*std::log10(std::max(1e-12,power));}
class Pcm {
public:
    snd_pcm_t *handle=nullptr;
    explicit Pcm(const char *name,snd_pcm_stream_t direction){
        int rc=snd_pcm_open(&handle,name,direction,SND_PCM_NONBLOCK);
        if(rc<0)throw std::runtime_error(std::string("Cannot open ")+name+": "+snd_strerror(rc));
        try {
            snd_pcm_hw_params_t *hw;snd_pcm_hw_params_alloca(&hw);
            auto check=[](int n){if(n<0)throw std::runtime_error(snd_strerror(n));};
            check(snd_pcm_hw_params_any(handle,hw));
            check(snd_pcm_hw_params_set_access(handle,hw,SND_PCM_ACCESS_RW_INTERLEAVED));
            check(snd_pcm_hw_params_set_format(handle,hw,SND_PCM_FORMAT_S16_LE));
            check(snd_pcm_hw_params_set_channels(handle,hw,2));
            check(snd_pcm_hw_params_set_rate(handle,hw,48000,0));
            snd_pcm_uframes_t period=480,buffer=3840;int dir=0;
            check(snd_pcm_hw_params_set_period_size_near(handle,hw,&period,&dir));
            check(snd_pcm_hw_params_set_buffer_size_near(handle,hw,&buffer));
            check(snd_pcm_hw_params(handle,hw));
            snd_pcm_sw_params_t *sw;snd_pcm_sw_params_alloca(&sw);
            check(snd_pcm_sw_params_current(handle,sw));
            check(snd_pcm_sw_params_set_avail_min(handle,sw,480));
            check(snd_pcm_sw_params_set_start_threshold(handle,sw,direction==SND_PCM_STREAM_PLAYBACK?960:1));
            check(snd_pcm_sw_params(handle,sw));check(snd_pcm_prepare(handle));
        }catch(...){snd_pcm_close(handle);handle=nullptr;throw;}
    }
    ~Pcm(){if(handle){snd_pcm_drop(handle);snd_pcm_close(handle);}}
    // Bounded waits let SIGTERM stop capture promptly. Fail on xrun rather than
    // reporting cancellation metrics for a reference timeline that was lost.
    bool transfer(int16_t *data,int frames,bool writing){
        int done=0,waits=0;
        while(done<frames&&!stopping){
            auto n=writing?snd_pcm_writei(handle,data+done*2,frames-done):snd_pcm_readi(handle,data+done*2,frames-done);
            if(n==-EAGAIN){if(++waits>20)throw std::runtime_error("Audio device stalled");int rc=snd_pcm_wait(handle,100);if(rc<0&&rc!=-EINTR)throw std::runtime_error(snd_strerror(rc));continue;}
            if(n==-EINTR)continue;
            if(n<0)throw std::runtime_error(std::string("Audio timeline lost: ")+snd_strerror(n));
            if(n==0)throw std::runtime_error("Audio device returned no frames");
            done+=int(n);
        }
        return done==frames;
    }
    int delay(){snd_pcm_sframes_t samples=0;return snd_pcm_delay(handle,&samples)>=0?std::max(0,int(samples)):0;}
};
}
