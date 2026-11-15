#include "assistant.h"
#include "sharedaudio.h"
#include "controller.h"
#include "display.h"
#include <QSettings>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>
#include <cmath>
namespace {
QString eventId(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
}
QStringList VoiceAssistant::voiceChoices(bool live){
    QStringList choices{"marin","cedar","alloy","ash","ballad","coral","echo","sage","shimmer","verse"};
    if(live)choices.append({"quartz","ripple","vesper","willow","stone","gleam","meridian","beacon","delta","cinder","bossa","tempo"});
    return choices;
}
QStringList VoiceAssistant::voices()const{return voiceChoices(isLive());}
QVariantList VoiceAssistant::backendModels()const{
    QVariantList choices{QVariantMap{{"id","gpt-6-luna"},{"name","GPT-6 Luna"}},QVariantMap{{"id","gpt-6-sol"},{"name","GPT-6 Sol"}}};
    if(std::none_of(choices.cbegin(),choices.cend(),[this](const QVariant &v){return v.toMap().value("id").toString()==m_configBackend;}))
        choices.append(QVariantMap{{"id",m_configBackend},{"name",m_configBackend}});
    return choices;
}
void VoiceAssistant::setBackendModel(const QString &value){
    if(m_active||value==m_backendModel)return;
    const auto choices=backendModels();
    if(std::none_of(choices.cbegin(),choices.cend(),[&value](const QVariant &v){return v.toMap().value("id").toString()==value;}))return;
    m_backendModel=value;QSettings().setValue("assistant/liveBackendModel",value);emit changed();
}
QJsonObject VoiceAssistant::liveConfiguration()const{
    // Local tools remain allowlisted by dispatchTool. Responses function schemas
    // explicitly permit the optional arguments already used by our dispatcher.
    QJsonArray functions=liveTools();
    for(int i=0;i<functions.size();i++){auto f=functions[i].toObject();f.insert("strict",false);functions[i]=f;}
    return {{"model","gpt-live-1"},{"store",false},{"instructions",m_prompts.liveInstructions()+personalInstructions()+conversationInstructions(true)},
        {"audio",QJsonObject{{"format",QJsonObject{{"type","audio/pcm"},{"rate",24000}}},{"output",QJsonObject{{"voice",m_voice}}}}},
        {"delegation",QJsonObject{{"type","responses"},{"responses",QJsonObject{{"model",m_backendModel},{"instructions",m_prompts.backendInstructions()+personalInstructions()+conversationInstructions()},{"tools",functions},
            {"tool_choice","auto"},{"parallel_tool_calls",false},{"reasoning",QJsonObject{{"effort","low"}}},{"max_output_tokens",2048}}}}}};
}
void VoiceAssistant::sendMicrophone(const QString &audio){
    if(m_active&&m_sessionReady&&(isLive()||m_vad!="manual"||m_manualTurn))
        send({{"type",isLive()?"session.input_audio.append":"input_audio_buffer.append"},{"audio",audio}});
}
void VoiceAssistant::liveMeter(bool input,double dbfs){
    if(!m_active||!m_liveClock.isValid())return;
    // Live streams continuously, including silence. Queue occupancy is not a
    // speaking indicator, and silent packets must not keep a paid session open.
    const auto now=m_liveClock.elapsed();
    if(std::isfinite(dbfs)&&dbfs>(input?-50.:-56.)){
        if(input)m_liveInputUntil=now+650;else m_liveOutputUntil=now+300;
        // Microphone energy drives the display, but only received user text,
        // actual speaker activity and backend work extend the billed idle timer.
        if(!input)m_activity.restart();
    }
    updateLiveActivity();
}
void VoiceAssistant::updateLiveActivity(){
    if(!m_active)return;
    const auto now=m_liveClock.elapsed();
    const bool input=now<m_liveInputUntil,output=now<m_liveOutputUntil;
    if(input!=m_userSpeaking||output!=m_playing){m_userSpeaking=input;m_playing=output;updateMode();}
}
void VoiceAssistant::interruptLive(){
    m_livePendingAudio.clear();
    worker({{"type","flush"},{"request",++m_flushId}});m_playing=false;m_liveOutputUntil=0;
    // Ordinary spoken interruptions are handled by Live's full-duplex model.
    // This path is for the explicit Interrupt button or saying Sheila again.
    m_liveDiscardUntil=m_liveClock.elapsed()+750;
    send({{"type","session.instructions.append"},{"event_id",eventId()},{"delegation_id",QJsonValue::Null},
        {"content","Stop speaking now and listen for the user's next request. Do not continue your previous spoken reply."}});
    m_activity.restart();updateMode();
}
void VoiceAssistant::closeLiveSocket(QWebSocket *socket){
    if(socket->state()!=QAbstractSocket::ConnectedState){socket->abort();socket->deleteLater();return;}
    socket->setParent(this);
    // Capture/upload is already stopped. Only consume finalization events on
    // this retiring connection, never more tools or audio from a closed session.
    const auto generation=m_liveGeneration;
    connect(socket,&QWebSocket::textMessageReceived,this,[this,socket,generation](const QString &message){
        const auto event=QJsonDocument::fromJson(message.toUtf8()).object();
        if(event.value("type")=="session.closed"){
            if(generation==m_liveGeneration){
                const auto seconds=event.value("usage").toObject().value("seconds").toDouble(-1);
                if(std::isfinite(seconds)&&seconds>=0)m_liveSeconds=seconds;
                m_liveUsageFinalized=true;emit changed();
            }
            socket->close();
        }
    });
    connect(socket,&QWebSocket::disconnected,socket,&QObject::deleteLater);
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"type","session.close"},{"event_id",eventId()}}).toJson(QJsonDocument::Compact)));socket->flush();
    QTimer::singleShot(15000,socket,[socket](){socket->abort();socket->deleteLater();});
}
void VoiceAssistant::receiveLive(const QJsonObject &event){
    const auto type=event.value("type").toString(),id=event.value("event_id").toString();
    if(!id.isEmpty()){
        if(m_liveEvents.contains(id))return;
        if(m_liveEvents.size()>=4096)m_liveEvents.clear();m_liveEvents.insert(id);
    }
    if(type=="session.started"){
        if(m_sessionReady)return;m_sessionReady=true;
        m_audio.start(binary(),sharedAudioEnabled()?QStringList{"--playback-only"}:QStringList{});
        if(sharedAudioEnabled())m_wake.command("stream");
        m_limit.start(10*60*1000);m_activity.restart();m_idleTimer.start();
    }else if(type=="session.output_audio.delta"){
        if(m_liveClock.elapsed()<m_liveDiscardUntil)return;
        if(m_audio.state()!=QProcess::Running){
            const auto pcm=QByteArray::fromBase64(event.value("delta").toString().toLatin1(),QByteArray::AbortOnBase64DecodingErrors);
            if(pcm.size()%2||m_livePendingAudio.size()+pcm.size()>96000){fail("Live speaker did not become ready in time.");return;}
            m_livePendingAudio+=pcm;return;
        }
        worker({{"type","audio"},{"item","live-audio"},{"index",0},{"audio",event.value("delta")}});
    }else if(type=="session.input_transcript.delta"||type=="session.output_transcript.delta"){
        const int speaker=type=="session.input_transcript.delta"?0:1;
        const auto text=event.value("delta").toString();if(text.isEmpty())return;
        const auto start=qint64(event.value("start_ms").toDouble()),end=qint64(event.value("end_ms").toDouble());
        // Each speaker has an independent timeline during overlapping speech.
        if(m_liveTranscriptId[speaker].isEmpty()||start-m_liveTranscriptEnd[speaker]>1800)
            m_liveTranscriptId[speaker]=QString("live-%1-%2").arg(speaker).arg(++m_liveTranscriptSerial);
        m_liveTranscriptEnd[speaker]=std::max(start,end);
        appendTranscript(m_liveTranscriptId[speaker],speaker==0?"user":"assistant",text,true);
        if(!text.trimmed().isEmpty()){
            m_activity.restart();if(speaker==0)m_liveInputUntil=m_liveClock.elapsed()+650;
        }
        updateLiveActivity();
    }else if(type=="session.delegation.created"){
        const auto d=event.value("delegation").toObject();const auto response=d.value("response_id").toString();
        if(d.value("target")!="responses"){fail("Live requested an unsupported backend. Reload configuration.");return;}
        if(!response.isEmpty()){m_liveResponses.insert(response);m_liveDelegationResponses.insert(d.value("id").toString(),response);}
        m_responseActive=true;m_responseDeadline.start(45000);m_activity.restart();updateMode();
    }else if(type=="response.event"){
        const auto nested=event.value("event").toObject();const auto kind=nested.value("type").toString();
        const auto delegation=event.value("delegation_id").toString();
        const auto response=nested.value("response").toObject();
        QString responseId=response.value("id").toString(nested.value("response_id").toString(m_liveDelegationResponses.value(delegation)));
        if(kind=="response.created"){
            if(responseId.isEmpty()){fail("Live backend response has no identifier.");return;}
            m_liveDelegationResponses.insert(delegation,responseId);m_liveResponses.insert(responseId);
            m_responseActive=true;m_responseDeadline.start(45000);m_activity.restart();updateMode();
        }else if(kind=="response.output_item.done"){
            const auto item=nested.value("item").toObject();
            if(item.value("type")=="function_call"&&!m_calls.contains(item.value("call_id").toString())){
                if(responseId.isEmpty()||m_liveCompleted.contains(responseId)||item.value("call_id").toString().isEmpty())return;
                if(handleEndConversation(item))return;
                auto &calls=m_liveToolCalls[responseId];
                if(calls.size()>=32){fail("Live requested too many local tool calls.");return;}
                calls.append(item);
            }
        }else if(kind=="response.completed"){
            if(responseId.isEmpty()||m_liveCompleted.contains(responseId))return;
            m_liveCompleted.insert(responseId);m_liveResponses.remove(responseId);
            const auto calls=m_liveToolCalls.take(responseId);
            const bool continued=dispatchTools(calls,[this](){send({{"type","response.create"},{"event_id",eventId()}});m_responseDeadline.start(45000);updateMode();});
            if(!m_active)return;
            if(!continued){m_responseActive=!m_liveResponses.isEmpty();if(!m_responseActive)m_responseDeadline.stop();}
            m_activity.restart();updateMode();
        }else if(kind=="response.failed"||kind=="response.incomplete"){
            fail("Live's reasoning backend could not finish. Try again or choose another backend.");
        }else if(kind=="response.cancelled"){
            m_liveResponses.remove(responseId);m_liveToolCalls.remove(responseId);m_liveCompleted.insert(responseId);
            m_responseActive=!m_liveResponses.isEmpty();if(!m_responseActive)m_responseDeadline.stop();updateMode();
        }
    }else if(type=="error"){
        const auto code=event.value("error").toObject().value("code").toString();
        fail(code=="responses_validation_failed"?"Live could not configure the selected backend. Check backend model access.":"OpenAI rejected a Live request. Check model access and settings.");
    }else if(type=="session.usage.updated"||type=="session.closed"){
        const auto seconds=event.value("usage").toObject().value("seconds").toDouble(-1);
        if(std::isfinite(seconds)&&seconds>=0)m_liveSeconds=seconds;
        if(type!="session.closed")return;
        m_liveUsageFinalized=true;
        m_sessionReady=false;stop();m_status="Live conversation ended";emit changed();
    }
}
