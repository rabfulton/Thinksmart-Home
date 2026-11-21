#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// Bounded, human-editable learned facts. No transcript recording or background work.
class MemoryStore {
public:
    static constexpr int MaxBytes=8192,MaxEntries=32,MaxEntryBytes=1024;
    static QString path();
    static QStringList actions(){return {"memory.list","memory.add","memory.patch","memory.remove","memory.reset"};}
    QJsonObject request(const QJsonObject &request);
    QString instructions()const;
private:
    static QByteArray render(const QJsonArray &entries);
    static bool read(QJsonArray &entries,QByteArray &original,QString &error,bool reset=false);
};
