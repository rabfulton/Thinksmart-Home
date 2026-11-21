#include "../memory.h"
#include "../deviceconfig.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QLockFile>
#include <memory>

class MemoryTests:public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> dir;
    MemoryStore store;
    QByteArray bytes(){QFile f(MemoryStore::path());if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
    void write(const QByteArray &content){QFile f(MemoryStore::path());QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(content),content.size());f.close();QVERIFY(f.setPermissions(QFile::ReadOwner|QFile::WriteOwner));}
    QJsonObject call(const QString &action,QJsonObject args={}){args.insert("action",action);return store.request(args);}
private slots:
    void init(){
        dir=std::make_unique<QTemporaryDir>();QVERIFY(dir->isValid());qputenv("THINKSMART_PERSONAL_DIR",dir->path().toUtf8());qputenv("THINKSMART_DEVICE_CONFIG",dir->filePath("device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",dir->filePath("dashboard.json").toUtf8());DeviceConfig::reload();
    }
    void crudAndExactPatches(){
        QVERIFY(call("memory.list").value("entries").toArray().isEmpty());QVERIFY(!QFile::exists(MemoryStore::path()));
        auto result=call("memory.add",{{"text","Prefers jazz and jazz evenings."}});QVERIFY(result.value("ok").toBool());const auto id=result.value("id");QVERIFY(!id.toString().isEmpty());
        const auto original=bytes();QVERIFY(!call("memory.patch",{{"id",id},{"old_text","jazz"},{"new_text","folk"}}).value("ok").toBool());QCOMPARE(bytes(),original);
        QVERIFY(!call("memory.patch",{{"id",id},{"old_text","unknown"},{"new_text","folk"}}).value("ok").toBool());
        QVERIFY(call("memory.patch",{{"id",id},{"old_text","jazz evenings"},{"new_text","folk evenings"}}).value("ok").toBool());
        result=call("memory.list",{{"search","FOLK"}});QCOMPARE(result.value("entries").toArray().size(),1);QVERIFY(store.instructions().contains("folk evenings"));
        const auto unchanged=bytes();QVERIFY(call("memory.patch",{{"id",id},{"old_text","folk evenings"},{"new_text","folk evenings"}}).value("ok").toBool());QCOMPARE(bytes(),unchanged);
        QVERIFY(call("memory.remove",{{"id",id}}).value("ok").toBool());QVERIFY(!call("memory.remove",{{"id",id}}).value("ok").toBool());QVERIFY(call("memory.list").value("entries").toArray().isEmpty());
    }
    void limitsRejectWithoutTruncating(){
        QVERIFY(call("memory.add",{{"id","unicode"},{"text",QString(512,QChar(0xe9))}}).value("ok").toBool());QVERIFY(call("memory.list").value("ok").toBool());const auto original=bytes();
        QVERIFY(!call("memory.add",{{"text",QString(513,QChar(0xe9))}}).value("ok").toBool());QCOMPARE(bytes(),original);
        QVERIFY(call("memory.reset",{{"confirm",true}}).value("ok").toBool());
        for(int i=0;i<32;++i)QVERIFY(call("memory.add",{{"id",QString("entry-%1").arg(i)},{"text","Useful short preference."}}).value("ok").toBool());
        const auto full=bytes();QVERIFY(!call("memory.add",{{"text","One more"}}).value("ok").toBool());QCOMPARE(bytes(),full);QVERIFY(full.size()<=8192);
        QVERIFY(call("memory.reset",{{"confirm",true}}).value("ok").toBool());int added=0;
        while(call("memory.add",{{"text",QString(1000,'x')}}).value("ok").toBool())++added;
        QVERIFY(added>0&&added<32);QVERIFY(bytes().size()<=8192);QCOMPARE(call("memory.list").value("count").toInt(),added);
    }
    void humanEditsCorruptionAndResetScope(){
        write("# Sheila memory\n\n## preferences\nPrefers folk.\n");QVERIFY(store.instructions().contains("Prefers folk."));
        write("# Sheila memory\n\n## preferences\nPrefers jazz.\n");QVERIFY(store.instructions().contains("Prefers jazz."));QVERIFY(!store.instructions().contains("Prefers folk."));
        for(const auto &bad:{QByteArray("invalid header"),QByteArray("# Sheila memory\n## same\na\n## same\nb\n"),QByteArray("# Sheila memory\n## bad id\ntext\n"),QByteArray(8193,'x'),QByteArray("# Sheila memory\n## test\n\xff\n")}){
            write(bad);QVERIFY(!call("memory.add",{{"text","Must not overwrite"}}).value("ok").toBool());QCOMPARE(bytes(),bad);
            QVERIFY(!call("memory.reset").value("ok").toBool());QVERIFY(!call("memory.reset",{{"confirm","true"}}).value("ok").toBool());QCOMPARE(bytes(),bad);
            QVERIFY(call("memory.reset",{{"confirm",true}}).value("ok").toBool());QCOMPARE(bytes(),QByteArray("# Sheila memory\n"));
        }
        for(const auto &name:{"household.md","notes.json","routines.json"}){QFile f(dir->filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));f.write("keep");}
        QVERIFY(call("memory.add",{{"text","To forget"}}).value("ok").toBool());QVERIFY(call("memory.reset",{{"confirm",true}}).value("ok").toBool());
        for(const auto &name:{"household.md","notes.json","routines.json"}){QFile f(dir->filePath(name));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("keep"));}
    }
    void privateFilesLocksAndModuleGating(){
        QVERIFY(call("memory.add",{{"text","Private fact"}}).value("ok").toBool());
        const auto publicAccess=QFile::ReadGroup|QFile::WriteGroup|QFile::ReadOther|QFile::WriteOther;
        QVERIFY(!(QFile::permissions(MemoryStore::path())&publicAccess));QVERIFY(!(QFile::permissions(dir->path())&publicAccess));
        const auto original=bytes();QLockFile lock(MemoryStore::path()+".lock");QVERIFY(lock.tryLock());QVERIFY(!call("memory.remove",{{"id","anything"}}).value("ok").toBool());lock.unlock();QCOMPARE(bytes(),original);
        QVERIFY(QFile::setPermissions(MemoryStore::path(),QFile::ReadOwner|QFile::WriteOwner|QFile::ReadOther));QVERIFY(!call("memory.list").value("ok").toBool());QVERIFY(QFile::setPermissions(MemoryStore::path(),QFile::ReadOwner|QFile::WriteOwner));
        QFile target(dir->filePath("target"));QVERIFY(target.open(QIODevice::WriteOnly));target.write("keep");target.close();QVERIFY(QFile::remove(MemoryStore::path()));QVERIFY(QFile::link(target.fileName(),MemoryStore::path()));
        QVERIFY(!call("memory.reset",{{"confirm",true}}).value("ok").toBool());QVERIFY(target.open(QIODevice::ReadOnly));QCOMPARE(target.readAll(),QByteArray("keep"));target.close();QVERIFY(QFile::remove(MemoryStore::path()));
        QFile config(dir->filePath("device.json"));QVERIFY(config.open(QIODevice::WriteOnly));config.write(R"({"modules":{"assistant":false}})");config.close();DeviceConfig::reload();
        QVERIFY(!DeviceConfig::actionEnabled("memory.list"));QVERIFY(store.instructions().isEmpty());QVERIFY(!call("memory.add",{{"text","Do not save"}}).value("ok").toBool());QVERIFY(!QFile::exists(MemoryStore::path()));
    }
    void argumentsAndSecrets(){
        for(const auto &args:{QJsonObject{{"text","API key: fixture-example-value"}},QJsonObject{{"text","Bearer fixture-example-value"}},QJsonObject{{"text","ok"},{"path","/tmp/other"}},QJsonObject{{"id","../other"},{"text","ok"}},QJsonObject{{"text",123}},QJsonObject{{"text","Text\n## second\nOops"}}})QVERIFY(!call("memory.add",args).value("ok").toBool());
        QVERIFY(!QFile::exists(MemoryStore::path()));
    }
};
QTEST_GUILESS_MAIN(MemoryTests)
#include "memory-tests.moc"
