#include "aec.h"
#include <stdexcept>
#include <algorithm>
EchoProcessor::EchoProcessor():m_apm(webrtc::AudioProcessingBuilder().Create()) {
    if(!m_apm)throw std::runtime_error("Cannot create WebRTC echo processor");
    webrtc::AudioProcessing::Config config;
    config.echo_canceller.enabled=true;
    config.echo_canceller.mobile_mode=false;
    // Debian's APM 1.3 export flag does not initialize AEC3's linear framer.
    // Use its normal AEC3 output and a separate Speex linear path, as in the
    // reference device project, instead of that crashing optional API.
    config.echo_canceller.export_linear_aec_output=false;
    // Keep AGC and independent noise suppression off for this calibration.
    // Attenuation should not be confused with unrelated gain/noise processing.
    config.noise_suppression.enabled=false;
    config.high_pass_filter.enabled=true;
    config.gain_controller1.enabled=false;
    config.gain_controller2.enabled=false;
    m_apm->ApplyConfig(config);
    m_echo.reset(speex_echo_state_init(160,8000));
    int sampleRate=16000;int error=0;
    if(!m_echo)throw std::runtime_error("Cannot create linear echo processor");
    speex_echo_ctl(m_echo.get(),SPEEX_ECHO_SET_SAMPLING_RATE,&sampleRate);
    m_farResampler.reset(speex_resampler_init(1,48000,16000,3,&error));
    m_nearResampler.reset(speex_resampler_init(1,48000,16000,3,&error));
    m_cleanResampler.reset(speex_resampler_init(1,16000,48000,3,&error));
    if(!m_farResampler||!m_nearResampler||!m_cleanResampler||error)throw std::runtime_error("Cannot create audio resamplers");
}
void EchoProcessor::render(const Block &speaker) {
    spx_uint32_t in=480,out=160;
    if(speex_resampler_process_int(m_farResampler.get(),0,speaker.data(),&in,m_far16.data(),&out)!=0||in!=480||out!=160)
        throw std::runtime_error("Reference resampling failed");
    if(m_apm->ProcessReverseStream(m_far16.data(),m_format,m_format,m_reverse.data())!=0)
        throw std::runtime_error("WebRTC render processing failed");
}
void EchoProcessor::capture(const Block &mic,int delayMs,bool cancel) {
    std::array<int16_t,160> near{},linear{},clean{};spx_uint32_t in=480,out=160;
    if(speex_resampler_process_int(m_nearResampler.get(),0,mic.data(),&in,near.data(),&out)!=0||in!=480||out!=160)
        throw std::runtime_error("Microphone resampling failed");
    if(!cancel){
        // Keep microphone resampling continuous for the keyword gate, without
        // running either canceller or the cleaned-audio upsampler in standby.
        m_clean=mic;
        for(size_t i=0;i<near.size();i++)m_linear[0][i]=near[i]/32768.f;
        m_hasLinear=true;return;
    }
    if(m_apm->set_stream_delay_ms(std::clamp(delayMs,0,500))!=0 ||
       m_apm->ProcessStream(near.data(),m_format,m_format,clean.data())!=0)
        throw std::runtime_error("WebRTC capture processing failed");
    in=160;out=480;
    if(speex_resampler_process_int(m_cleanResampler.get(),0,clean.data(),&in,m_clean.data(),&out)!=0||in!=160||out!=480)
        throw std::runtime_error("Clean audio resampling failed");
    speex_echo_cancellation(m_echo.get(),near.data(),m_far16.data(),linear.data());
    for(int i=0;i<160;i++)m_linear[0][i]=linear[i]/32768.f;
    m_hasLinear=true;
}
