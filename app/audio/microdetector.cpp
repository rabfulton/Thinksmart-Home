#include "microdetector.h"
#include <QLibrary>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <array>
#include <vector>
#include <deque>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <stdexcept>

extern "C" {
#include "tensorflow/lite/experimental/microfrontend/lib/frontend.h"
#include "tensorflow/lite/experimental/microfrontend/lib/frontend_util.h"
}

struct MicroDetector::Impl {
    // Public TensorFlow Lite C ABI (2.17+); opaque handles match c_api.h.
    struct Quant {float scale;int32_t zero_point;};
    QLibrary library;
    void *model=nullptr,*options=nullptr,*interpreter=nullptr,*input=nullptr,*output=nullptr;
    void *(*modelCreate)(const char *)=nullptr;
    void (*modelDelete)(void *)=nullptr;
    void *(*optionsCreate)()=nullptr;
    void (*optionsDelete)(void *)=nullptr;
    void (*setThreads)(void *,int)=nullptr;
    void *(*interpreterCreate)(const void *,const void *)=nullptr;
    void (*interpreterDelete)(void *)=nullptr;
    int (*allocate)(void *)=nullptr;
    int (*invoke)(void *)=nullptr;
    void *(*getInput)(void *,int)=nullptr;
    const void *(*getOutput)(const void *,int)=nullptr;
    int (*inputCount)(const void *)=nullptr;
    int (*dimCount)(const void *)=nullptr;
    int (*dim)(const void *,int)=nullptr;
    int (*type)(const void *)=nullptr;
    size_t (*bytes)(const void *)=nullptr;
    Quant (*quant)(const void *)=nullptr;
    int (*copyIn)(void *,const void *,size_t)=nullptr;
    int (*copyOut)(const void *,void *,size_t)=nullptr;
    FrontendConfig config{};FrontendState frontend{};bool frontendReady=false;
    Quant inputQuant{},outputQuant{};
    int inputType=0,outputType=0,stride=0,windows=0;
    float threshold=0;
    std::vector<uint8_t> features;
    std::deque<double> probabilities;
    std::array<int16_t,160> pcm{};size_t filled=0;
    double last=0;uint64_t calls=0;bool armed=true;

    template<class T>void bind(T &function,const char *name){
        function=reinterpret_cast<T>(library.resolve(name));if(!function)throw std::runtime_error("TensorFlow Lite runtime is incomplete");
    }
    void clearInterpreter(){if(interpreter)interpreterDelete(interpreter);interpreter=nullptr;}
    void createInterpreter(){
        clearInterpreter();interpreter=interpreterCreate(model,options);
        if(!interpreter||allocate(interpreter)!=0)throw std::runtime_error("Cannot allocate Sheila model");
        if(inputCount(interpreter)!=1)throw std::runtime_error("Expected a streaming Sheila model with internal state");
        input=getInput(interpreter,0);output=const_cast<void *>(getOutput(interpreter,0));
        if(!input||!output||dimCount(input)!=3||dim(input,0)!=1||dim(input,2)!=40||bytes(output)!=1)
            throw std::runtime_error("Unsupported Sheila model tensor shape");
        stride=dim(input,1);inputType=type(input);outputType=type(output);
        inputQuant=quant(input);outputQuant=quant(output);
        if(stride<1||stride>16||bytes(input)!=size_t(stride*40)||(inputType!=3&&inputType!=9)||(outputType!=3&&outputType!=9)||inputQuant.scale<=0||outputQuant.scale<=0)
            throw std::runtime_error("Expected a valid INT8 or UINT8 Sheila model");
        features.clear();features.reserve(stride*40);probabilities.clear();filled=0;last=0;calls=0;armed=true;
    }
    ~Impl(){
        clearInterpreter();if(options)optionsDelete(options);if(model)modelDelete(model);
        if(frontendReady)FrontendFreeStateContents(&frontend);
    }
};

MicroDetector::MicroDetector(const QDir &directory,float threshold):state(std::make_unique<Impl>()){
    auto &s=*state;
    QFile file(directory.filePath("sheila.json"));
    if(!file.open(QIODevice::ReadOnly)||file.size()>16384)throw std::runtime_error("Cannot read Sheila model configuration");
    const auto settings=QJsonDocument::fromJson(file.readAll()).object();const auto micro=settings.value("micro").toObject();
    const auto modelName=settings.value("model").toString();
    if(settings.value("wake_word").toString().compare("Sheila",Qt::CaseInsensitive)!=0||!QRegularExpression("^[A-Za-z0-9_.-]+\\.tflite$").match(modelName).hasMatch()||micro.value("feature_step_size").toInt()!=10)
        throw std::runtime_error("Invalid Sheila streaming model configuration");
    s.windows=micro.value("sliding_window_size").toInt(5);s.threshold=threshold;
    if(s.windows<1||s.windows>30||!std::isfinite(threshold)||threshold<=0||threshold>=1)throw std::runtime_error("Invalid Sheila detection threshold");
    s.library.setFileName(qEnvironmentVariable("THINKSMART_TFLITE_LIB","/usr/local/lib/thinksmart-wakeword/libtensorflowlite_c.so"));
    if(!s.library.load())throw std::runtime_error("TensorFlow Lite runtime is unavailable");
    s.bind(s.modelCreate,"TfLiteModelCreateFromFile");s.bind(s.modelDelete,"TfLiteModelDelete");
    s.bind(s.optionsCreate,"TfLiteInterpreterOptionsCreate");s.bind(s.optionsDelete,"TfLiteInterpreterOptionsDelete");s.bind(s.setThreads,"TfLiteInterpreterOptionsSetNumThreads");
    s.bind(s.interpreterCreate,"TfLiteInterpreterCreate");s.bind(s.interpreterDelete,"TfLiteInterpreterDelete");s.bind(s.allocate,"TfLiteInterpreterAllocateTensors");s.bind(s.invoke,"TfLiteInterpreterInvoke");
    s.bind(s.getInput,"TfLiteInterpreterGetInputTensor");s.bind(s.getOutput,"TfLiteInterpreterGetOutputTensor");s.bind(s.inputCount,"TfLiteInterpreterGetInputTensorCount");
    s.bind(s.dimCount,"TfLiteTensorNumDims");s.bind(s.dim,"TfLiteTensorDim");s.bind(s.type,"TfLiteTensorType");s.bind(s.bytes,"TfLiteTensorByteSize");s.bind(s.quant,"TfLiteTensorQuantizationParams");
    s.bind(s.copyIn,"TfLiteTensorCopyFromBuffer");s.bind(s.copyOut,"TfLiteTensorCopyToBuffer");
    s.model=s.modelCreate(QFile::encodeName(directory.filePath(modelName)).constData());s.options=s.optionsCreate();
    if(!s.model||!s.options)throw std::runtime_error("Cannot load Sheila streaming model");s.setThreads(s.options,1);s.createInterpreter();
    FrontendFillConfigWithDefaults(&s.config);
    s.config.window.size_ms=30;s.config.window.step_size_ms=10;
    s.config.filterbank.num_channels=40;s.config.filterbank.lower_band_limit=125;s.config.filterbank.upper_band_limit=7500;
    s.config.noise_reduction.smoothing_bits=10;s.config.noise_reduction.even_smoothing=.025;s.config.noise_reduction.odd_smoothing=.06;s.config.noise_reduction.min_signal_remaining=.05;
    s.config.pcan_gain_control.enable_pcan=1;s.config.pcan_gain_control.strength=.95;s.config.pcan_gain_control.offset=80;s.config.pcan_gain_control.gain_bits=21;
    s.config.log_scale.enable_log=1;s.config.log_scale.scale_shift=6;
    if(!FrontendPopulateState(&s.config,&s.frontend,16000))throw std::runtime_error("Cannot create Sheila audio frontend");s.frontendReady=true;
}
MicroDetector::~MicroDetector()=default;
bool MicroDetector::put(const int16_t *samples,size_t count){
    auto &s=*state;bool detected=false;
    for(size_t i=0;i<count;i++){
        s.pcm[s.filled++]=samples[i];if(s.filled<s.pcm.size())continue;s.filled=0;size_t offset=0;
        while(offset<s.pcm.size()){
            size_t used=0;const auto feature=FrontendProcessSamples(&s.frontend,s.pcm.data()+offset,s.pcm.size()-offset,&used);
            if((used==0&&feature.size==0)||used>s.pcm.size()-offset)throw std::runtime_error("Sheila audio frontend stalled");offset+=used;
            if(feature.size==0)continue;if(feature.size!=40)throw std::runtime_error("Invalid Sheila feature size");
            for(size_t bin=0;bin<40;bin++){
                const auto value=int(std::nearbyint((feature.values[bin]*.0390625f)/s.inputQuant.scale+s.inputQuant.zero_point));
                s.features.push_back(uint8_t(s.inputType==9?std::clamp(value,-128,127):std::clamp(value,0,255)));
            }
            if(s.features.size()<size_t(s.stride*40))continue;
            if(s.copyIn(s.input,s.features.data(),s.features.size())!=0||s.invoke(s.interpreter)!=0)throw std::runtime_error("Sheila model inference failed");
            s.features.clear();uint8_t result=0;if(s.copyOut(s.output,&result,1)!=0)throw std::runtime_error("Cannot read Sheila prediction");
            const auto integer=s.outputType==9?int(int8_t(result)):int(result);
            const double probability=std::clamp((integer-s.outputQuant.zero_point)*double(s.outputQuant.scale),0.,1.);
            s.probabilities.push_back(probability);if(s.probabilities.size()>size_t(s.windows))s.probabilities.pop_front();s.calls++;
            s.last=s.probabilities.size()==size_t(s.windows)?std::accumulate(s.probabilities.begin(),s.probabilities.end(),0.)/s.windows:0.;
            if(s.last<s.threshold*.5)s.armed=true;
            if(s.armed&&s.last>=s.threshold){s.armed=false;detected=true;}
        }
    }
    return detected;
}
void MicroDetector::reset(){auto &s=*state;s.createInterpreter();FrontendReset(&s.frontend);}
double MicroDetector::probability()const{return state->last;}
uint64_t MicroDetector::inferences()const{return state->calls;}
