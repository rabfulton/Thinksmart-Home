#include "wakeword.h"
#include "sharedaudio.h"
#include "assistantprompts.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QSettings>
#include <cmath>
#include <algorithm>
#include <sys/prctl.h>
#include <csignal>
QString WakeWord::binary(){return qEnvironmentVariable("THINKSMART_WAKEWORD_AUDIO",sharedAudioEnabled()?"/usr/local/libexec/thinksmart-shared-audio":"/usr/local/libexec/thinksmart-wakeword");}
QString WakeWord::modelDirectory(){return qEnvironmentVariable("THINKSMART_WAKEWORD_MODEL","/usr/local/share/thinksmart/wakeword/model");}
QString WakeWord::keywords(){return qEnvironmentVariable("THINKSMART_WAKEWORD_KEYWORDS","/usr/local/share/thinksmart/wakeword/sheila.txt");}
bool WakeWord::available()const{
    if(!QFileInfo(binary()).isExecutable())return false;
    const QDir model(modelDirectory());QFile micro(model.filePath("sheila.json"));
    if(micro.exists()){
        if(!micro.open(QIODevice::ReadOnly)||micro.size()>16384)return false;
        const auto config=QJsonDocument::fromJson(micro.readAll()).object();
        const auto name=config.value("model").toString();
        return !name.isEmpty()&&!name.contains('/')&&!name.contains('\\')&&QFileInfo(model.filePath(name)).isReadable()&&QFileInfo(qEnvironmentVariable("THINKSMART_TFLITE_LIB","/usr/local/lib/thinksmart-wakeword/libtensorflowlite_c.so")).isReadable();
    }
    if(!QFileInfo(keywords()).isReadable())return false;
    for(const auto &name:{"encoder.onnx","decoder.onnx","joiner.onnx","tokens.txt"})if(!QFileInfo(QDir(modelDirectory()).filePath(name)).isReadable())return false;
    return true;
}
WakeWord::WakeWord(QObject *parent):QObject(parent){
    m_clock.start();m_process.setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);});
    m_killTimer.setSingleShot(true);m_killTimer.setInterval(700);connect(&m_killTimer,&QTimer::timeout,this,&WakeWord::kill);
    m_startDeadline.setSingleShot(true);connect(&m_startDeadline,&QTimer::timeout,this,[this](){fail("Wake-word model startup timed out. Reload configuration to retry.");});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,&WakeWord::consume);
    connect(&m_process,&QProcess::readyReadStandardError,this,[this](){m_process.readAllStandardError();});
    connect(&m_process,&QProcess::errorOccurred,this,[this](){if(!m_stopping)fail("Local wake-word worker failed. Reload configuration to retry.");});
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus exit){
        consume();m_killTimer.stop();m_startDeadline.stop();const bool detected=m_detected&&m_allowed&&!m_stopping&&code==0&&exit==QProcess::NormalExit;
        if(!m_stopping&&!detected&&m_problem.isEmpty())m_problem="Wake-word listener stopped. Reload configuration to retry.";
        m_detected=false;m_listening=false;m_stopping=false;m_nextStart=m_clock.elapsed()+3000;
        emit changed();if(detected)emit this->detected();
    });
    readSettings();
}
WakeWord::~WakeWord(){m_allowed=false;halt();m_process.disconnect(this);if(!released()&&!m_process.waitForFinished(500)){m_process.kill();m_process.waitForFinished(500);}}
QString WakeWord::status()const{
    if(!m_allowed)return m_reason;
    if(!m_problem.isEmpty())return "Wake word unavailable";
    if(!available())return "Wake-word model not installed";
    if(m_listening)return "Listening locally for Sheila";
    return "Preparing local wake word…";
}
void WakeWord::setAllowed(bool allowed,const QString &reason){
    const bool changedState=m_allowed!=allowed||m_reason!=reason;m_allowed=allowed;m_reason=reason;
    if(sharedAudioEnabled()&&allowed&&released()&&!m_problem.isEmpty()&&m_nextStart>0&&m_clock.elapsed()>=m_nextStart&&m_restartCount<3){m_problem.clear();m_restartCount++;}
    if(!allowed)halt();
    else if(released()&&m_problem.isEmpty()&&available()&&m_clock.elapsed()>=m_nextStart)launch();
    if(changedState)emit changed();
}
void WakeWord::command(const QString &type){
    if(!sharedAudioEnabled()||m_process.state()!=QProcess::Running)return;
    if(m_process.bytesToWrite()>8192){fail("Local audio command queue stalled.");return;}
    m_process.write(QJsonDocument(QJsonObject{{"type",type}}).toJson(QJsonDocument::Compact)+'\n');
}
void WakeWord::retry(){halt();m_problem.clear();m_nextStart=0;m_restartCount=0;readSettings();emit changed();}
bool WakeWord::thresholdOverridden()const{return QSettings().contains("assistant/wakeThreshold");}
double WakeWord::sensitivity()const{
    const double low=std::min(m_fileThreshold,std::max(.005,m_fileThreshold-.03)),high=std::max(m_fileThreshold,std::min(.995,m_fileThreshold+.03));
    if(qFuzzyCompare(m_threshold,m_fileThreshold))return 50;
    if(m_threshold>m_fileThreshold)return high>m_fileThreshold?std::clamp(50*(high-m_threshold)/(high-m_fileThreshold),0.,50.):0;
    return m_fileThreshold>low?std::clamp(50+50*(m_fileThreshold-m_threshold)/(m_fileThreshold-low),50.,100.):100;
}
void WakeWord::setSensitivity(double value){
    if(!std::isfinite(value)||value<0||value>100)return;
    const double low=std::min(m_fileThreshold,std::max(.005,m_fileThreshold-.03)),high=std::max(m_fileThreshold,std::min(.995,m_fileThreshold+.03));
    setThreshold(value<=50?m_fileThreshold+(high-m_fileThreshold)*(1-value/50):m_fileThreshold-(m_fileThreshold-low)*((value-50)/50));
}
void WakeWord::setThreshold(double value){
    if(!std::isfinite(value)||value<.005||value>.995||qFuzzyCompare(value,m_threshold))return;
    QSettings().setValue("assistant/wakeThreshold",value);retry();
}
void WakeWord::resetThreshold(){QSettings().remove("assistant/wakeThreshold");retry();}
void WakeWord::halt(){
    m_startDeadline.stop();m_detected=false;m_listening=false;
    if(!released()&&!m_stopping){m_stopping=true;m_process.terminate();m_killTimer.start();}
}
void WakeWord::kill(){if(!released())m_process.kill();}
void WakeWord::fail(const QString &message){m_problem=message;halt();emit changed();}
bool WakeWord::readSettings(){
    double threshold=.35,score=1.5;int blanks=2;
    const QString path=AssistantPrompts::directory()+"/wakeword.json";QFile config(path);
    if(config.exists()){
        if(!config.open(QIODevice::ReadOnly)||config.size()>4096){fail("Cannot read wakeword.json.");return false;}
        QJsonParseError error;const auto settings=QJsonDocument::fromJson(config.readAll(),&error).object();
        threshold=settings.value("threshold").toDouble(-1);score=settings.value("score").toDouble(-1);blanks=settings.value("trailingBlanks").toInt(-1);
        if(error.error!=QJsonParseError::NoError||settings.value("word").toString().compare("Sheila",Qt::CaseInsensitive)!=0||!std::isfinite(threshold)||threshold<=0||threshold>=1||!std::isfinite(score)||score<=0||score>5||blanks<1||blanks>5){fail("Invalid wakeword.json. Use Sheila and valid sensitivity settings.");return false;}
    }
    bool ok=false;const auto saved=QSettings().value("assistant/wakeThreshold").toDouble(&ok);
    m_fileThreshold=threshold;m_threshold=ok&&std::isfinite(saved)&&saved>=.005&&saved<=.995?saved:threshold;m_score=score;m_blanks=blanks;
    return true;
}
void WakeWord::launch(){
    if(!readSettings())return;
    m_buffer.clear();m_stopping=false;m_detected=false;m_runningSince=m_clock.elapsed();
    m_process.start(binary(),{"--model-dir",modelDirectory(),"--keywords-file",keywords(),"--threshold",QString::number(m_threshold),"--score",QString::number(m_score),"--trailing-blanks",QString::number(m_blanks)});m_startDeadline.start(15000);emit changed();
}
void WakeWord::consume(){
    m_buffer+=m_process.readAllStandardOutput();if(m_buffer.size()>(sharedAudioEnabled()?512*1024:16384)){m_buffer.clear();fail("Invalid local wake-word response.");return;}
    while(m_buffer.contains('\n')){
        auto end=m_buffer.indexOf('\n');const auto event=QJsonDocument::fromJson(m_buffer.left(end)).object();m_buffer.remove(0,end+1);
        if(!m_allowed||m_stopping)continue;
        if(sharedAudioEnabled()&&m_clock.elapsed()-m_runningSince>60000)m_restartCount=0;
        if(event.value("event")=="started"){m_startDeadline.stop();m_listening=true;emit changed();}
        else if(event.value("event")=="mic"&&sharedAudioEnabled())emit microphone(event.value("audio").toString());
        else if(event.value("event")=="health"&&sharedAudioEnabled())emit meter(event.value("cleanDbfs").toDouble(-120));
        else if(event.value("event")=="wake"&&event.value("word").toString().compare("Sheila",Qt::CaseInsensitive)==0&&!m_detected){
            if(sharedAudioEnabled()){emit detected();continue;}
            m_detected=true;m_listening=false;m_killTimer.start(1000); // Wait for a clean, bounded microphone release.
        }else if(event.value("event")=="error")fail("Local wake-word audio failed. Reload configuration to retry.");
    }
}
