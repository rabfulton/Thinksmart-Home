#include "deviceconfig.h"
#include "assistantprompts.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>
namespace {
bool readText(const QString &path,QString &text,QString &problem){
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){problem="Cannot read "+QFileInfo(path).fileName()+" in the assistant prompt folder.";return false;}
    const auto bytes=file.read(32769);
    if(bytes.size()>32768){problem=QFileInfo(path).fileName()+" exceeds 32 KiB. Shorten this prompt file.";return false;}
    QStringDecoder decoder(QStringDecoder::Utf8);text=decoder.decode(bytes);
    if(decoder.hasError()||text.contains(QChar(0))){problem=QFileInfo(path).fileName()+" must contain UTF-8 text.";return false;}
    text=text.trimmed();return true;
}
QString moduleText(const QString &role,QString &problem){
    QString result;
    for(const auto &id:ModuleCatalog::names()){
        if(!DeviceConfig::enabled(id))continue;
        const auto relative="modules/"+id+"/"+role+".md";
        const auto local=AssistantPrompts::directory()+"/"+relative;
        const auto path=QFile::exists(local)?local:":/"+relative;
        if(!QFile::exists(path))continue;
        QString body;if(!readText(path,body,problem))return {};
        result+="\n\n"+body;
    }
    return result;
}

}
QString AssistantPrompts::directory(){return qEnvironmentVariable("THINKSMART_ASSISTANT_DIR","/etc/thinksmart/assistant");}
void AssistantPrompts::reload(const QString &selected,bool live){
    m_selected=selected;m_personalities.clear();m_instructions.clear();m_liveInstructions.clear();m_backendInstructions.clear();m_problem.clear();
    const QDir root(directory()),profiles(root.filePath("personalities"));
    QMap<QString,QString> bodies;QMap<QString,QString> errors;
    for(const auto &file:profiles.entryInfoList({"*.md"},QDir::Files,QDir::Name|QDir::IgnoreCase)){
        const QString id=file.completeBaseName();
        if(!QRegularExpression("^[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}$").match(id).hasMatch())continue;
        QString body,error;
        if(!readText(file.absoluteFilePath(),body,error)){errors.insert(id,error);continue;}
        bodies.insert(id,body);m_personalities.append(QVariantMap{{"id",id},{"name",file.fileName()}});
    }
    if(m_selected.isEmpty())m_selected=bodies.contains("warm")?QString("warm"):(bodies.isEmpty()?QString():bodies.firstKey());
    if(m_personalities.isEmpty()){m_problem="No readable personalities found. Check "+profiles.path()+".";return;}
    if(!bodies.contains(m_selected)){
        m_problem=errors.contains(m_selected)?errors.value(m_selected):"The selected personality is missing. Choose another personality in Settings.";return;
    }
    QString household;
    if(!readText(root.filePath("household.md"),household,m_problem))return;
    const auto personality="\n\n# Selected personality\n"+bodies.value(m_selected);
    const auto knowledge="\n\n# Shared household knowledge\n"+household;
    if(live){
        QString conversation,backend;
        if(!readText(root.filePath("live.md"),conversation,m_problem)||!readText(root.filePath("backend.md"),backend,m_problem))return;
        if(conversation.isEmpty()||backend.isEmpty()){m_problem="live.md and backend.md must contain instructions.";return;}
        const auto frontendModules=moduleText("live",m_problem);if(!m_problem.isEmpty())return;
        const auto backendModules=moduleText("backend",m_problem);if(!m_problem.isEmpty())return;
        m_liveInstructions=conversation+personality+knowledge+frontendModules+DeviceConfig::context();
        // Personality and Realtime speaking rules belong to the voice model only.
        m_backendInstructions=backend+knowledge+backendModules+DeviceConfig::context();
        if(m_liveInstructions.toUtf8().size()>65536||m_backendInstructions.toUtf8().size()>65536){
            m_liveInstructions.clear();m_backendInstructions.clear();m_problem="Combined Live or backend prompt exceeds 64 KiB. Shorten the policy, shared notes or personality.";
        }
    }else{
        QString system;if(!readText(root.filePath("system.md"),system,m_problem))return;
        const auto modules=moduleText("realtime",m_problem);if(!m_problem.isEmpty())return;
        m_instructions=system+personality+knowledge+modules+DeviceConfig::context();
        if(m_instructions.toUtf8().size()>65536){m_instructions.clear();m_problem="Combined assistant prompt exceeds 64 KiB. Shorten the shared notes or personality.";}
    }
}
