#pragma once
#include "pcm.h"
#include "microdetector.h"
#include <sherpa-onnx/c-api/c-api.h>
#include <QDir>
#include <QFile>
#include <memory>
#include <vector>
class Detector {
    using Spotter=std::unique_ptr<const SherpaOnnxKeywordSpotter,decltype(&SherpaOnnxDestroyKeywordSpotter)>;
    using Stream=std::unique_ptr<const SherpaOnnxOnlineStream,decltype(&SherpaOnnxDestroyOnlineStream)>;
    Spotter spotter{nullptr,SherpaOnnxDestroyKeywordSpotter};
    Stream stream{nullptr,SherpaOnnxDestroyOnlineStream};
    std::unique_ptr<MicroDetector> micro;
public:
    void resetKeyword(){if(!micro)SherpaOnnxResetKeywordStream(spotter.get(),stream.get());}
    void reset(){if(micro){micro->reset();return;}stream.reset(SherpaOnnxCreateKeywordStream(spotter.get()));if(!stream)throw std::runtime_error("Cannot reset keyword stream");}
    Detector(const QDir &model,const QString &keywords,float threshold,float score,int blanks){
        if(QFile::exists(model.filePath("sheila.json"))){micro=std::make_unique<MicroDetector>(model,threshold);return;}
        const auto encoder=QFile::encodeName(model.filePath("encoder.onnx")),decoder=QFile::encodeName(model.filePath("decoder.onnx")),joiner=QFile::encodeName(model.filePath("joiner.onnx")),tokens=QFile::encodeName(model.filePath("tokens.txt")),keywordPath=QFile::encodeName(keywords);
        SherpaOnnxKeywordSpotterConfig config{};
        config.feat_config.sample_rate=16000;config.feat_config.feature_dim=80;
        config.model_config.transducer.encoder=encoder.constData();config.model_config.transducer.decoder=decoder.constData();config.model_config.transducer.joiner=joiner.constData();
        config.model_config.tokens=tokens.constData();config.model_config.provider="cpu";config.model_config.num_threads=1;
        config.max_active_paths=4;config.keywords_score=score;config.keywords_threshold=threshold;config.num_trailing_blanks=blanks;config.keywords_file=keywordPath.constData();
        spotter.reset(SherpaOnnxCreateKeywordSpotter(&config));if(!spotter)throw std::runtime_error("Cannot load local keyword model");
        stream.reset(SherpaOnnxCreateKeywordStream(spotter.get()));if(!stream)throw std::runtime_error("Cannot create local keyword stream");
    }
    bool put(const int16_t *pcm,size_t count){
        if(micro)return micro->put(pcm,count);
        std::vector<float> samples(count);for(size_t i=0;i<count;i++)samples[i]=pcm[i]/32768.f;
        SherpaOnnxOnlineStreamAcceptWaveform(stream.get(),16000,samples.data(),int(samples.size()));
        while(!audio::stopping&&SherpaOnnxIsKeywordStreamReady(spotter.get(),stream.get())){
            SherpaOnnxDecodeKeywordStream(spotter.get(),stream.get());
            std::unique_ptr<const SherpaOnnxKeywordResult,decltype(&SherpaOnnxDestroyKeywordResult)> result(SherpaOnnxGetKeywordResult(spotter.get(),stream.get()),SherpaOnnxDestroyKeywordResult);
            if(result&&result->keyword&&QString::fromUtf8(result->keyword).compare("Sheila",Qt::CaseInsensitive)==0)return true;
        }
        return false;
    }
};
