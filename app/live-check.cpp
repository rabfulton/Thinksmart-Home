// Short, explicit API configuration probe. No microphone or speaker is opened.
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QWebSocket>
#include <QNetworkRequest>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QTimer>
#include <QRegularExpression>
#include <cstdio>
static void report(const QJsonObject &o){const auto b=QJsonDocument(o).toJson(QJsonDocument::Compact);fwrite(b.data(),1,size_t(b.size()),stdout);fputc('\n',stdout);fflush(stdout);}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QCommandLineParser p;p.addHelpOption();
    p.addOptions({{"config","Private voice configuration","file"},{"session","Public session-start fixture","file"},{"exercise-tools","Run a read-only backend protocol probe with a synthetic status result"}});p.process(app);
    QFile config(p.value("config")),fixture(p.value("session"));
    if(!config.open(QIODevice::ReadOnly)||config.size()>16384||(config.permissions()&(QFile::ReadGroup|QFile::ReadOther|QFile::WriteGroup|QFile::WriteOther))||!fixture.open(QIODevice::ReadOnly)||fixture.size()>262144)return 2;
    const auto key=QJsonDocument::fromJson(config.readAll()).object().value("apiKey").toString();
    auto start=QJsonDocument::fromJson(fixture.readAll()).object();auto session=start.value("session").toObject();
    if(key.isEmpty()||session.value("model")!="gpt-live-1")return 2;
    session.insert("instructions","Remain silent until the user speaks. This is a configuration probe; no speech is requested.");session.insert("store",false);start.insert("session",session);
    QWebSocket socket;bool started=false,failed=false,pendingTools=false;QSet<QString> calls;
    auto send=[&](QJsonObject event){socket.sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));};
    auto close=[&](){if(started)socket.sendTextMessage("{\"type\":\"session.close\",\"event_id\":\"probe_close\"}");};
    QObject::connect(&socket,&QWebSocket::connected,&app,[&](){socket.sendTextMessage(QString::fromUtf8(QJsonDocument(start).toJson(QJsonDocument::Compact)));});
    QObject::connect(&socket,&QWebSocket::textMessageReceived,&app,[&](const QString &message){
        const auto event=QJsonDocument::fromJson(message.toUtf8()).object();const auto type=event.value("type").toString();
        if(type=="session.started"){
            started=true;report({{"event","started"},{"backend",session.value("delegation").toObject().value("responses").toObject().value("model")}});
            if(p.isSet("exercise-tools")){
                send({{"type","response.item.create"},{"event_id","probe_request"},{"item",QJsonObject{{"type","message"},{"role","user"},{"content",QJsonArray{QJsonObject{{"type","input_text"},{"text","Read media status once using device_action with action media.status, then give a short result. Do not play, stop, pause or change anything. This is a read-only protocol check."}}}}}}});
                send({{"type","response.create"},{"event_id","probe_run"}});
            }else QTimer::singleShot(2500,&app,close);
        }else if(type=="response.event"&&p.isSet("exercise-tools")){
            const auto nested=event.value("event").toObject();const auto kind=nested.value("type").toString();
            if(kind=="response.output_item.done"){
                const auto item=nested.value("item").toObject();const auto id=item.value("call_id").toString();
                if(item.value("type")=="function_call"&&!id.isEmpty()&&!calls.contains(id)){
                    calls.insert(id);if(calls.size()>3){failed=true;close();return;}
                    const auto args=QJsonDocument::fromJson(item.value("arguments").toString().toUtf8()).object();
                    const bool allowed=item.value("name")=="device_action"&&args.value("action")=="media.status";
                    QJsonObject result{{"ok",allowed},{"probe",true},{"media",QJsonObject{{"state","stopped"}}}};
                    if(!allowed){result.insert("error","This probe permits only media.status; no hardware actions were performed");failed=true;}
                    send({{"type","response.item.create"},{"item",QJsonObject{{"type","function_call_output"},{"call_id",id},{"output",QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact))}}}});
                    pendingTools=true;report({{"event","tool_result"},{"allowed",allowed}});
                }
            }else if(kind=="response.completed"){
                if(pendingTools){pendingTools=false;send({{"type","response.create"}});}
                else if(!calls.isEmpty()){report({{"event","backend_completed"},{"toolCalls",calls.size()}});QTimer::singleShot(500,&app,close);}
                else {failed=true;report({{"event","missing_tool_call"}});close();}
            }else if(kind=="response.failed"||kind=="response.incomplete"){failed=true;report({{"event","backend_failed"}});close();}
        }else if(type=="error"){
            failed=true;auto safe=[](QString value){value.remove(QRegularExpression("[^A-Za-z0-9_.\\[\\]-]"));return value.left(120);};
            const auto error=event.value("error").toObject();report({{"event","rejected"},{"code",safe(error.value("code").toString())},{"param",safe(error.value("param").toString())}});
            if(started)close();else QCoreApplication::exit(1);
        }else if(type=="session.closed"){
            report({{"event","finalized"},{"seconds",event.value("usage").toObject().value("seconds")}});socket.close();QCoreApplication::exit(failed?1:0);
        }
    });
    QObject::connect(&socket,&QWebSocket::errorOccurred,&app,[&](QAbstractSocket::SocketError error){report({{"event","connection_failed"},{"socketError",int(error)}});QCoreApplication::exit(1);});
    QTimer::singleShot(20000,&app,[&](){report({{"event","timeout"}});socket.abort();QCoreApplication::exit(1);});
    QNetworkRequest request(QUrl("wss://api.openai.com/v1/live/sessions"));request.setRawHeader("Authorization","Bearer "+key.toUtf8());socket.open(request);return app.exec();
}
