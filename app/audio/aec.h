#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <speex/speex_echo.h>
#include <speex/speex_resampler.h>
#include <modules/audio_processing/include/audio_processing.h>
#include <api/scoped_refptr.h>

// One owner/thread calls both directions, in 10 ms blocks. No audio device or
// Qt dependency here: the same processor can be reused by a cloud voice worker.
class EchoProcessor {
public:
    static constexpr int rate=48000, frames=480;
    using Block=std::array<int16_t,frames>;
    EchoProcessor();
    void render(const Block &speaker);
    void capture(const Block &mic,int delayMs,bool cancel=true);
    const Block &clean() const { return m_clean; }
    const std::array<float,160> &linear() const { return m_linear[0]; }
    bool hasLinear() const { return m_hasLinear; }
    webrtc::AudioProcessingStats statistics() { return m_apm->GetStatistics(); }
private:
    rtc::scoped_refptr<webrtc::AudioProcessing> m_apm;
    webrtc::StreamConfig m_format{16000,1};
    Block m_clean{},m_reverse{};
    std::unique_ptr<SpeexEchoState,decltype(&speex_echo_state_destroy)> m_echo{nullptr,speex_echo_state_destroy};
    std::unique_ptr<SpeexResamplerState,decltype(&speex_resampler_destroy)> m_farResampler{nullptr,speex_resampler_destroy},m_nearResampler{nullptr,speex_resampler_destroy},m_cleanResampler{nullptr,speex_resampler_destroy};
    std::array<int16_t,160> m_far16{};
    std::array<std::array<float,160>,1> m_linear{};
    bool m_hasLinear=false;
};
