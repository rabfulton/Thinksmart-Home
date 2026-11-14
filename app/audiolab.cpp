#include "audiolab.h"
#include "controller.h"
#include "display.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <QSettings>
#include <cmath>
#include <QUuid>
#include <sys/prctl.h>
#include <csignal>
QString AudioLab::binary(){return qEnvironmentVariable("THINKSMART_AUDIO_LAB","/usr/local/libexec/thinksmart-audio-lab");}
QString AudioLab::reference(){return qEnvironmentVariable("THINKSMART_AUDIO_REFERENCE","/usr/local/share/thinksmart/audio-lab/reference.s16");}
bool AudioLab::available()const{return QFileInfo(binary()).isExecutable()&&QFileInfo(reference()).isReadable();}
AudioLab::AudioLab(Controller *controller,ScreenController *display):QObject(controller),m_controller(controller),m_display(display){
    QSettings settings;m_testVoiceLevel=qBound(.05,settings.value("audioLab/testVoiceLevel",.25).toDouble(),.75);
    m_reportPath=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/audio-lab/last-result.json";
    QFile previous(m_reportPath);
    if(previous.open(QIODevice::ReadOnly)&&previous.size()<256*1024){
        const auto report=QJsonDocument::fromJson(previous.readAll()).object();
        m_summary=report.value("summary").toObject();
        const auto history=report.value("levels").toArray();
        if(!history.isEmpty())m_levels=history.last().toObject().toVariantMap();
    }
    m_process.setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,&AudioLab::consume);
    connect(&m_process,&QProcess::readyReadStandardError,this,[this](){m_process.readAllStandardError();});
    connect(&m_process,&QProcess::errorOccurred,this,[this](){if(!m_stopping)fail("Audio test process failed. Try again.");});
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        consume();m_deadline.stop();m_killTimer.stop();m_starting=false;
        if(!m_stopping&&code!=0&&m_problem.isEmpty())m_problem="Audio test stopped unexpectedly.";
        if(m_problem.isEmpty())m_status=m_stopping?"Stopped · microphone closed":"Test complete · microphone closed";
        saveReport(code);emit changed();
    });
    m_wait.setInterval(25);connect(&m_wait,&QTimer::timeout,this,[this](){
        if(!m_controller->audioReleased()){
            if(m_waitClock.elapsed()>1500)m_controller->releaseAudio(true);
            if(m_waitClock.elapsed()>2500)fail("The previous audio source could not release the speaker.");
            return;
        }
        m_wait.stop();m_starting=false;
        QStringList args{"--reference",reference(),"--seconds","20","--level",QString::number(m_testVoiceLevel)};
        if(m_saveClips){
            const QString parent=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/audio-lab";
            QDir().mkpath(parent);QFile::setPermissions(parent,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
            m_recordingPath=parent+"/"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-")+QUuid::createUuid().toString(QUuid::Id128).left(8);
            args<<"--record-dir"<<m_recordingPath;
        }
        m_process.start(binary(),args);m_deadline.start(24000);emit changed();
    });
    m_deadline.setSingleShot(true);connect(&m_deadline,&QTimer::timeout,this,[this](){fail("Audio test timed out; capture has been stopped.");});
    m_killTimer.setSingleShot(true);m_killTimer.setInterval(700);connect(&m_killTimer,&QTimer::timeout,this,&AudioLab::kill);
    connect(display,&ScreenController::changed,this,[this](){if(m_display->sleeping()&&running())stop();});
}
AudioLab::~AudioLab(){m_process.disconnect(this);if(m_process.state()!=QProcess::NotRunning){m_process.terminate();if(!m_process.waitForFinished(500)){m_process.kill();m_process.waitForFinished(500);}}}
void AudioLab::start(bool saveClips){
    if(running())return;
    if(!available()){fail("Install the local audio test worker first.");return;}
    m_controller->releaseAudio();
    m_buffer.clear();m_problem.clear();m_recordingPath.clear();m_levels.clear();m_history=QJsonArray();m_summary=QJsonObject();m_startedAt=QDateTime::currentDateTimeUtc().toString(Qt::ISODate);m_saveClips=saveClips;m_stopping=false;
    m_starting=true;m_status="Preparing speaker and microphones…";m_waitClock.restart();m_wait.start();emit changed();
}
void AudioLab::stop(){
    const bool wasRunning=running();m_starting=false;m_stopping=true;m_wait.stop();m_deadline.stop();
    if(m_process.state()!=QProcess::NotRunning){m_process.terminate();m_killTimer.start();}
    if(wasRunning)m_status="Stopping audio test…";emit changed();
}
void AudioLab::kill(){if(m_process.state()!=QProcess::NotRunning)m_process.kill();}
void AudioLab::fail(const QString &message){stop();m_problem=message;m_status="Audio test unavailable";emit changed();}
void AudioLab::consume(){
    m_buffer+=m_process.readAllStandardOutput();
    if(m_buffer.size()>128*1024){fail("Invalid response from audio worker.");m_buffer.clear();return;}
    while(m_buffer.contains('\n')){
        const int end=m_buffer.indexOf('\n');const auto data=QJsonDocument::fromJson(m_buffer.left(end)).object();m_buffer.remove(0,end+1);
        const auto event=data.value("event").toString();
        if(event=="error"){m_problem=data.value("message").toString();m_status="Audio test failed";}
        else if(event=="started")m_status="Playing test speech · microphones active";
        else if(event=="levels"){
            m_levels=data.toVariantMap();if(m_history.size()<100)m_history.append(data);m_display->activity();
            m_status=data.value("clippedPercent").toDouble()>0.5?"Microphone clipping · gain needs tuning":"Playing test speech · microphones active";
        }
        emit changed();
    }
}
QJsonObject AudioLab::snapshot()const{return {{"available",available()},{"running",running()},{"status",m_status},{"error",m_problem},{"levels",QJsonObject::fromVariantMap(m_levels)},{"recordingPath",m_recordingPath},{"testVoiceLevel",m_testVoiceLevel},{"summary",m_summary},{"reportPath",m_reportPath}};}

void AudioLab::setTestVoiceLevel(double value){
    if(running()||!std::isfinite(value))return;
    m_testVoiceLevel=qBound(.05,value,.75);QSettings().setValue("audioLab/testVoiceLevel",m_testVoiceLevel);emit changed();
}
void AudioLab::saveReport(int exitCode){
    if(m_history.isEmpty())return;
    double rawPower=0,cleanPower=0,clipped=0,maxClipped=0;
    for(const auto &value:m_history){
        const auto row=value.toObject();
        rawPower+=std::pow(10,row.value("rawDbfs").toDouble()/10);
        cleanPower+=std::pow(10,row.value("cleanDbfs").toDouble()/10);
        const double clip=row.value("clippedPercent").toDouble();clipped+=clip;maxClipped=qMax(maxClipped,clip);
    }
    const double count=m_history.size();
    m_summary={{"durationSeconds",m_levels.value("elapsed").toDouble()},{"windows",m_history.size()},
        {"completed",exitCode==0&&!m_stopping&&m_levels.value("elapsed").toDouble()>=19.9},
        {"rawRmsDbfs",10*std::log10(rawPower/count)},{"cleanRmsDbfs",10*std::log10(cleanPower/count)},
        {"meanClippedPercent",clipped/count},{"maxWindowClippedPercent",maxClipped},
        {"cpuPercentOneCore",m_levels.value("cpuPercent").toDouble()}};
    const QString parent=QFileInfo(m_reportPath).absolutePath();
    if(!QDir().mkpath(parent)){m_problem="Could not save numeric test results.";return;}
    QFile::setPermissions(parent,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
    QSaveFile file(m_reportPath);
    if(!file.open(QIODevice::WriteOnly)){m_problem="Could not save numeric test results.";return;}
    file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);
    const auto bytes=QJsonDocument(QJsonObject{{"version",1},{"startedAt",m_startedAt},{"status",m_status},{"error",m_problem},
        {"testVoiceLevel",m_testVoiceLevel},{"speakerVolume",m_controller->volume()},{"summary",m_summary},{"levels",m_history}}).toJson(QJsonDocument::Compact);
    if(file.write(bytes)!=bytes.size()||!file.commit())m_problem="Could not save numeric test results.";
}
