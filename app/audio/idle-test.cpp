#include "idle.h"
#include "aec.h"
#include "detector.h"
#include <QCoreApplication>
#include <fstream>
#include <cstdio>
#include <vector>

static void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try {
        require(argc==2,"Expected fixture/model root");const QDir root(QString::fromLocal8Bit(argv[1]));
        IdleEchoPolicy policy;
        require(!policy.process(0,false),"Standby unexpectedly runs AEC");
        require(policy.process(1e-4,false),"Speaker onset did not enable AEC");
        for(int i=0;i<299;i++)require(policy.process(0,false),"Echo tail ended early");
        require(!policy.process(0,false),"AEC remained active after echo tail");
        require(policy.process(0,true),"Silent conversation did not enable AEC");
        require(!policy.process(0,false),"Conversation hold did not release");
        EchoProcessor echo;
        for(int block=0;block<20;block++){
            EchoProcessor::Block mic;for(int i=0;i<480;i++)mic[i]=int16_t(1000*std::sin((block*480+i)*.071));
            echo.capture(mic,100,false);require(echo.clean()==mic,"Bypassed audio changed microphone gain");
            if(block>3){double power=0;for(auto x:echo.linear())power+=x*x/160;require(power>1e-4,"Idle keyword audio was suppressed");}
        }
        EchoProcessor::Block zero{};echo.render(zero);echo.capture(zero,100,true);
        echo.capture(zero,100,false); // transition back to standby stays usable
        Detector detector(QDir(root.filePath("stage/model")),root.filePath("stage/sheila.txt"),.25f,2.f,2);
        auto fixture=[&](const QString &name,double gain,int repetitions=1){
            std::ifstream file(root.filePath(name+".s16").toStdString(),std::ios::binary);require(bool(file),"Cannot read keyword fixture");
            std::vector<int16_t> data;int16_t sample;while(file.read(reinterpret_cast<char *>(&sample),2))data.push_back(int16_t(sample*gain));
            const auto original=data;
            for(int i=1;i<repetitions;i++){data.resize(data.size()+48320,0);data.insert(data.end(),original.begin(),original.end());}
            data.resize(data.size()+24000,0);detector.reset();int hits=0;
            for(size_t offset=0;offset<data.size();offset+=640){std::array<int16_t,640> batch{};std::copy_n(data.data()+offset,std::min(batch.size(),data.size()-offset),batch.data());if(detector.put(batch.data(),batch.size())){hits++;detector.resetKeyword();}}
            require(name=="sheila"?hits==repetitions:hits==0,"Batched keyword recognition failed");
            std::printf("PASS: %s gain %.2f: %d wakes\n",qPrintable(name),gain,hits);
        };
        fixture("sheila",1.,2);fixture("unrelated",1.);fixture("sheila",.1,2);
        std::puts("PASS: idle bypass, echo tail, repeated wake and quiet fixture");return 0;
    }catch(const std::exception &e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
