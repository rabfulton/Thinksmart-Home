#include "../personal.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QLockFile>
#include <QTimeZone>

class PersonalTests:public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> dir;
    QDateTime now=QDateTime::fromString("2026-11-14T15:00:00Z",Qt::ISODate);
    QJsonArray stopSteps()const{return {QJsonObject{{"action","media.stop"}}};}
private slots:
    void init(){dir=std::make_unique<QTemporaryDir>();QVERIFY(dir->isValid());qputenv("THINKSMART_PERSONAL_DIR",dir->path().toUtf8());}
    void notesPersistUpdateDeleteAndRefuseCorruption(){
        PersonalStore store;
        auto result=store.request("notes.add",{{"text","Remember <b>milk</b>\nand coffee"}},now);QVERIFY(result.value("ok").toBool());
        const auto id=result.value("id");QCOMPARE(store.notes().size(),1);
        const auto listed=store.request("notes.list");QVERIFY(listed.value("ok").toBool());QCOMPARE(listed.value("total").toInt(),1);
        QVERIFY(!listed.value("items").toArray().first().toObject().contains("text"));
        QCOMPARE(store.request("notes.get",{{"id",id}}).value("item").toObject().value("text").toString(),QString("Remember <b>milk</b>\nand coffee"));
        PersonalStore other;QCOMPARE(other.notes().first().toMap().value("text").toString(),QString("Remember <b>milk</b>\nand coffee"));
        QVERIFY(other.request("notes.update",{{"id",id},{"title","Shopping"},{"text","Coffee"}},now).value("ok").toBool());
        QCOMPARE(store.notes().first().toMap().value("title").toString(),QString("Shopping"));
        QLockFile lock(dir->filePath("store.lock"));QVERIFY(lock.tryLock());
        QVERIFY(!store.request("notes.delete",{{"id",id}}).value("ok").toBool());lock.unlock();
        QVERIFY(store.request("notes.delete",{{"id",id}}).value("ok").toBool());QVERIFY(other.notes().isEmpty());
        QFile f(dir->filePath("notes.json"));QVERIFY(f.open(QIODevice::WriteOnly));f.write("{ broken");f.close();
        QVERIFY(!store.request("notes.add",{{"text","Do not overwrite"}}).value("ok").toBool());
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("{ broken"));
    }
    void themesAreValidatedUndoableAndReloadable(){
        PersonalStore store;QCOMPARE(store.themeNames().size(),5);
        // Existing installs may still have a seeded Dawn file. Keep the file,
        // but do not reintroduce the retired theme as a personal palette.
        QFile legacy(dir->filePath("themes/dawn.json"));QVERIFY(legacy.open(QIODevice::WriteOnly));
        legacy.write(QJsonDocument(QJsonObject{{"version",1},{"id","dawn"},{"colors",store.palette("autumn")}}).toJson());legacy.close();
        store.reload();QVERIFY(!store.themeNames().contains("dawn"));QVERIFY(legacy.exists());
        QVERIFY(!store.request("theme.apply",{{"id","dawn"}}).value("ok").toBool());
        QVERIFY(!store.request("theme.save",{{"id","dawn"},{"base","ember"},{"colors",QJsonObject{{"accent","#e8b568"}}}}).value("ok").toBool());
        const auto original=store.palette("autumn");QVERIFY(!original.isEmpty());
        QVERIFY(store.request("theme.save",{{"id","plum"},{"base","dusk"},{"colors",QJsonObject{{"accent","#e8b568"}}}}).value("ok").toBool());
        QVERIFY(store.themeNames().contains("plum"));QCOMPARE(store.palette("plum").value("accent").toString(),QString("#e8b568"));
        QVERIFY(store.request("theme.save",{{"id","autumn"},{"base","autumn"},{"colors",QJsonObject{{"accent","#c8aff0"}}}}).value("ok").toBool());
        QVERIFY(store.request("theme.undo",{{"id","autumn"}}).value("ok").toBool());QCOMPARE(store.palette("autumn"),original);
        QVERIFY(!store.request("theme.save",{{"id","../escape"},{"base","ember"},{"colors",QJsonObject{{"accent","#ffffff"}}}}).value("ok").toBool());
        QVERIFY(!store.request("theme.save",{{"id","unreadable"},{"base","ember"},{"colors",QJsonObject{{"ink","#10161c"}}}}).value("ok").toBool());
        QVERIFY(!store.request("theme.save",{{"id","code"},{"base","ember"},{"colors",QJsonObject{{"source","file:///etc/passwd"}}}}).value("ok").toBool());
        QVERIFY(store.request("theme.delete",{{"id","plum"}}).value("ok").toBool());QVERIFY(!store.themeNames().contains("plum"));
    }
    void schedulesClaimOnceSurviveRestartAndMarkMissed(){
        PersonalStore store;
        auto result=store.request("schedule.save",{{"title","Stop later"},{"afterSeconds",30},{"steps",stopSteps()}},now);QVERIFY(result.value("ok").toBool());
        const auto id=result.value("id");QVERIFY(store.claimDue(now.addSecs(29)).isEmpty());
        auto due=store.claimDue(now.addSecs(30));QCOMPARE(due.size(),1);QCOMPARE(due.first().toObject().value("id"),id);
        PersonalStore restarted;QVERIFY(restarted.claimDue(now.addSecs(31)).isEmpty());
        QVERIFY(restarted.request("schedule.result",{{"id",id},{"lastRun",due.first().toObject().value("lastRun")},{"ok",true},{"message","Requested"}}).value("ok").toBool());
        QCOMPARE(restarted.schedules().first().toMap().value("status").toString(),QString("completed"));
        QVERIFY(!restarted.request("schedule.enable",{{"id",id},{"enabled",true}},now.addSecs(90)).value("ok").toBool());
        QVERIFY(store.request("schedule.save",{{"title","Missed"},{"afterSeconds",10},{"steps",stopSteps()}},now).value("ok").toBool());
        QVERIFY(store.claimDue(now.addSecs(10),false).isEmpty());QVERIFY(store.schedules().last().toMap().value("enabled").toBool());
        QVERIFY(store.claimDue(now.addSecs(100),false).isEmpty());QCOMPARE(store.schedules().last().toMap().value("status").toString(),QString("missed"));
    }
    void lockFilesDoNotCauseContinuousModelResets(){
        PersonalStore store;QSignalSpy changes(&store,&PersonalStore::changed);
        store.claimDue(now);store.request("notes.list");QTest::qWait(100);
        QCOMPARE(changes.count(),0);
    }
    void calendarSchedulesRespectWeekdaysAndBritishSummerTime(){
        QJsonObject schedule{{"time","09:00"},{"timezone","Europe/London"},{"weekdays",QJsonArray{1,2,3,4,5}}};
        // Saturday -> Monday. Summer schedules remain at 09:00 local, 08:00 UTC.
        QCOMPARE(PersonalStore::nextOccurrence(schedule,now),QDateTime::fromString("2026-11-16T09:00:00Z",Qt::ISODate));
        QCOMPARE(PersonalStore::nextOccurrence(schedule,QDateTime::fromString("2026-06-12T08:00:01Z",Qt::ISODate)),QDateTime::fromString("2026-06-15T08:00:00Z",Qt::ISODate));
        schedule.insert("time","01:30");schedule.insert("weekdays",QJsonArray{1,2,3,4,5,6,7});
        const auto before=QDateTime::fromString("2026-10-25T00:00:00Z",Qt::ISODate);
        const auto first=PersonalStore::nextOccurrence(schedule,before);QVERIFY(first.isValid());
        const auto next=PersonalStore::nextOccurrence(schedule,first.addSecs(1));
        QCOMPARE(next.toTimeZone(QTimeZone("Europe/London")).date(),QDate(2026,10,26));
        PersonalStore store;auto result=store.request("schedule.save",{{"title","Weekdays"},{"time","09:00"},{"timezone","Europe/London"},{"weekdays",QJsonArray{1,2,3,4,5}},{"steps",stopSteps()}},now);QVERIFY(result.value("ok").toBool());
        const auto due=store.claimDue(QDateTime::fromString("2026-11-16T09:00:00Z",Qt::ISODate));QCOMPARE(due.size(),1);
        QVERIFY(store.schedules().first().toMap().value("enabled").toBool());
        QCOMPARE(store.schedules().first().toMap().value("nextAt").toString(),QString("2026-11-17T09:00:00.000Z"));
    }
    void routinesResolveAtRunTimeAndOnlyAllowKnownActions(){
        PersonalStore store;
        QVERIFY(!store.request("routine.save",{{"title","Bad"},{"steps",QJsonArray{QJsonObject{{"action","shell"},{"command","anything"}}}}},now).value("ok").toBool());
        auto routine=store.request("routine.save",{{"id","evening"},{"title","Evening"},{"steps",stopSteps()}},now);QVERIFY(routine.value("ok").toBool());
        QVERIFY(store.request("schedule.save",{{"title","Evening"},{"afterSeconds",5},{"routineId","evening"}},now).value("ok").toBool());
        QVERIFY(!store.request("routine.delete",{{"id","evening"}},now).value("ok").toBool());
        QJsonArray updated{QJsonObject{{"action","media.volume"},{"percent",20}}};
        QVERIFY(store.request("routine.save",{{"id","evening"},{"title","Evening"},{"steps",updated}},now).value("ok").toBool());
        QCOMPARE(store.claimDue(now.addSecs(5)).first().toObject().value("steps").toArray(),updated);
        QVERIFY(!store.request("schedule.save",{{"title","Ambiguous"},{"afterSeconds",5},{"at","2027-01-01T00:00:00Z"},{"steps",stopSteps()}},now).value("ok").toBool());
        QVERIFY(!store.request("schedule.save",{{"title","No offset"},{"at","2027-01-01T00:00:00"},{"steps",stopSteps()}},now).value("ok").toBool());
    }
};
QTEST_GUILESS_MAIN(PersonalTests)
#include "personal-tests.moc"
