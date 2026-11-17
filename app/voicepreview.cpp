#include "voicepreview.h"
#include "sharedaudio.h"
#include <sys/prctl.h>
#include <csignal>
#include <QDataStream>
#include <cmath>
#include <algorithm>

WakeChime::WakeChime(){
    m_process.setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);});
    m_deadline.setSingleShot(true);m_deadline.setInterval(2000);
    connect(&m_deadline,&QTimer::timeout,this,&WakeChime::stop);
    connect(&m_process,&QProcess::readyReadStandardError,this,[this](){m_process.readAllStandardError();});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this](){m_process.readAllStandardOutput();});
    connect(&m_process,&QProcess::started,this,[this](){
        QByteArray pcm;QDataStream stream(&pcm,QIODevice::WriteOnly);stream.setByteOrder(QDataStream::LittleEndian);
        constexpr int rate=48000,note=4320;
        for(int i=0;i<note*2;i++){
            const int n=i%note;const double envelope=std::min({1.,n/720.,(note-1-n)/720.});
            stream<<qint16(std::lround(32767*.12*envelope*std::sin(2*3.141592653589793*(i<note?660:880)*n/rate)));
        }
        m_process.write(pcm);m_process.closeWriteChannel();
    });
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](){m_deadline.stop();});
    connect(&m_process,&QProcess::errorOccurred,this,[this](){m_deadline.stop();});
}
WakeChime::~WakeChime(){m_process.disconnect(this);stop();if(playing())m_process.waitForFinished(500);}
void WakeChime::play(){
    if(playing())return;
    QString player="aplay";
#ifdef THINKSMART_VOICE_TESTING
    player=qEnvironmentVariable("THINKSMART_WAKE_CHIME_PLAYER","aplay");
#endif
    m_process.start(player,{"-q","-D",sharedAudioEnabled()?"thinksmart_voice":"default","-t","raw","-f","S16_LE","-c","1","-r","48000"});m_deadline.start();
}
void WakeChime::stop(){m_deadline.stop();if(playing())m_process.kill();}

VoicePreview::VoicePreview(){
    m_process.setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this](){m_process.readAllStandardOutput();});
    connect(&m_process,&QProcess::readyReadStandardError,this,[this](){m_process.readAllStandardError();});
    m_kill.setSingleShot(true);m_kill.setInterval(500);
    connect(&m_kill,&QTimer::timeout,this,[this](){m_process.kill();});
    m_deadline.setSingleShot(true);m_deadline.setInterval(15000);
    connect(&m_deadline,&QTimer::timeout,this,[this](){stop();m_problem="Voice preview timed out.";emit changed();});
    connect(&m_process,&QProcess::started,this,[this](){if(m_stopping)m_process.terminate();});
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart){m_deadline.stop();m_kill.stop();m_next.clear();m_problem="Cannot start the local voice preview player.";emit changed();}
    });
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus){
        m_kill.stop();m_deadline.stop();
        if(!m_stopping&&code!=0)m_problem="Could not play this voice preview.";
        if(!m_next.isEmpty())launch();
        emit changed();
    });
}
VoicePreview::~VoicePreview(){
    m_process.disconnect(this);
    if(m_process.state()!=QProcess::NotRunning){m_process.kill();m_process.waitForFinished(500);}
}
void VoicePreview::play(const QString &path){
    m_problem.clear();m_next=path;
    if(m_process.state()==QProcess::NotRunning)launch();
    else{m_stopping=true;m_deadline.stop();m_process.terminate();m_kill.start();}
    emit changed();
}
void VoicePreview::stop(){
    m_next.clear();m_problem.clear();m_deadline.stop();m_stopping=true;
    if(m_process.state()!=QProcess::NotRunning){m_process.terminate();m_kill.start();}
    emit changed();
}
void VoicePreview::launch(){
    const auto path=m_next;m_next.clear();m_stopping=false;
    QString player="mpv";
#ifdef THINKSMART_VOICE_TESTING
    player=qEnvironmentVariable("THINKSMART_VOICE_PREVIEW_PLAYER","mpv");
#endif
    // The voice bus bypasses media ducking and remains in the AEC reference mix.
    m_process.start(player,{"--no-config","--no-video","--ao=alsa",
        sharedAudioEnabled()?"--audio-device=alsa/thinksmart_voice":"--audio-device=alsa/default",
        "--audio-format=s16","--audio-channels=mono","--volume=100",
        "--terminal=no","--input-terminal=no","--msg-level=all=error","--",path});
    m_deadline.start();
}
