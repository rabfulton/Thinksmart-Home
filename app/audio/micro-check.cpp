#include "microdetector.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <array>
#include <cstdio>
#include <ctime>
#include <cstring>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QCommandLineParser p;p.addHelpOption();
    p.addOptions({{"model-dir","Directory with sheila.json","directory"},{"file","Raw PCM16 mono 16 kHz input","file"},{"threshold","Detection threshold","value",".85"},{"repeat","Repeat input to benchmark streaming","count","1"},{"scores","Include inference probability sequence"}});p.process(app);
    try {
        MicroDetector detector(QDir(p.value("model-dir")),p.value("threshold").toFloat());
        QFile input(p.value("file"));if(!input.open(QIODevice::ReadOnly)||input.size()%2||input.size()>32000*3600)throw std::runtime_error("Invalid benchmark input");
        const auto bytes=input.readAll();const auto repeats=p.value("repeat").toInt();if(repeats<1||repeats>10000||bytes.isEmpty())throw std::runtime_error("Invalid repeat count");
        QElapsedTimer wall;wall.start();const double cpu=std::clock()/double(CLOCKS_PER_SEC);int wakes=0;double maximum=0;QJsonArray scores;
        for(int pass=0;pass<repeats;pass++)for(qint64 offset=0;offset<bytes.size();offset+=320){
            std::array<int16_t,160> pcm{};const auto length=std::min<qint64>(320,bytes.size()-offset);std::memcpy(pcm.data(),bytes.constData()+offset,size_t(length));
            const auto before=detector.inferences();if(detector.put(pcm.data(),length/2))wakes++;
            maximum=std::max(maximum,detector.probability());if(p.isSet("scores")&&detector.inferences()!=before)scores.append(detector.probability());
        }
        const double seconds=bytes.size()/32000.*repeats,used=std::clock()/double(CLOCKS_PER_SEC)-cpu;
        QJsonObject result{{"audioSeconds",seconds},{"cpuSeconds",used},{"cpuPercentOneCore",used/seconds*100},{"wallSeconds",wall.elapsed()/1000.},{"inferences",double(detector.inferences())},{"wakes",wakes},{"maxProbability",maximum}};
        if(p.isSet("scores"))result.insert("scores",scores);
        const auto output=QJsonDocument(result).toJson(QJsonDocument::Compact);fwrite(output.data(),1,output.size(),stdout);fputc('\n',stdout);return 0;
    }catch(const std::exception &e){fprintf(stderr,"%s\n",e.what());return 1;}
}
