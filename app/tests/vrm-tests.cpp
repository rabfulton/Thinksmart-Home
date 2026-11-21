#include "../vrm.h"
#include <QtTest>
#include <QDateTime>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QStandardPaths>
class VrmTests:public QObject {
    Q_OBJECT
    QJsonObject record(const QString &code,const QJsonValue &value,qint64 timestamp) {
        return {{"code",code},{"rawValue",value},{"timestamp",timestamp}};
    }
private slots:
    void preservesZeroMissingAndIndependentTimes() {
        qint64 now=QDateTime::currentSecsSinceEpoch();
        auto r=VrmClient::normalize({record("bs",60.7,now),record("Pdc",0,now),record("OP1",1,now-86400),record("bc","invalid",now)});
        QCOMPARE(r["soc"].toObject()["value"].toDouble(),60.7);
        QCOMPARE(r["solar"].toObject()["value"].toDouble(),0.0);
        QCOMPARE(r["acLoad"].toObject()["timestamp"].toVariant().toLongLong(),now-86400);
        QVERIFY(!r.contains("current"));QVERIFY(!r.contains("dcLoad"));
    }
    void aggregatesWithoutRefreshingOlderMeasurements() {
        qint64 now=QDateTime::currentSecsSinceEpoch();
        auto r=VrmClient::normalize({record("PVP",100,now),record("PVP",200,now-3600),record("OP1",30,now),record("OP2",40,now),record("OP3",50,now-3600)});
        QCOMPARE(r["solar"].toObject()["value"].toDouble(),300.0);
        QCOMPARE(r["solar"].toObject()["timestamp"].toVariant().toLongLong(),now-3600);
        QCOMPARE(r["acLoad"].toObject()["value"].toDouble(),120.0);
        auto preferred=VrmClient::normalize({record("Pdc",80,now),record("PVP",999,now)});
        QCOMPARE(preferred["solar"].toObject()["value"].toDouble(),80.0);
    }
    void cachedPrimaryFreshness_data(){
        QTest::addColumn<qint64>("socAge");QTest::addColumn<bool>("includeSoc");QTest::addColumn<bool>("expectedStale");
        QTest::newRow("fresh-battery-retired-ac-load")<<qint64(60)<<true<<false;
        QTest::newRow("old-battery-fresh-solar")<<qint64(7200)<<true<<true;
        QTest::newRow("unknown-battery-time")<<qint64(-1)<<true<<true;
        QTest::newRow("no-battery-fresh-solar")<<qint64(0)<<false<<false;
    }
    void cachedPrimaryFreshness(){
        QFETCH(qint64,socAge);QFETCH(bool,includeSoc);QFETCH(bool,expectedStale);
        QTemporaryDir temporary;QVERIFY(temporary.isValid());qputenv("XDG_CACHE_HOME",temporary.filePath("cache").toUtf8());qputenv("THINKSMART_VRM_CONFIG",temporary.filePath("vrm.json").toUtf8());
        QFile config(temporary.filePath("vrm.json"));QVERIFY(config.open(QIODevice::WriteOnly));config.setPermissions(QFile::ReadOwner|QFile::WriteOwner);config.write(R"({"token":"fixture-only-not-a-real-token","installationId":7})");config.close();
        const qint64 now=QDateTime::currentSecsSinceEpoch();QJsonObject readings{{"solar",QJsonObject{{"value",0},{"timestamp",now-60}}},{"acLoad",QJsonObject{{"value",1},{"timestamp",now-20000000}}}};
        if(includeSoc)readings.insert("soc",QJsonObject{{"value",65},{"timestamp",socAge<0?0:now-socAge}});
        const auto directory=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);QVERIFY(QDir().mkpath(directory));QFile cache(directory+"/vrm-7.json");QVERIFY(cache.open(QIODevice::WriteOnly));cache.write(QJsonDocument(QJsonObject{{"version",1},{"installationId",7},{"readings",readings}}).toJson());cache.close();
        // No event loop is entered: the client is destroyed before its queued refresh.
        VrmClient client;QVERIFY(client.hasData());QCOMPARE(client.stale(),expectedStale);QVERIFY(client.metrics().value("acLoad").toMap().value("stale").toBool());QVERIFY(!client.metrics().value("solar").toMap().value("stale").toBool());
    }
    void rejectsPartialInvalidAggregatesAndImpossibleSoc() {
        qint64 now=QDateTime::currentSecsSinceEpoch();
        auto r=VrmClient::normalize({record("bs",101,now),record("OP1",30,now),record("OP2",QJsonValue::Null,now)});
        QVERIFY(!r.contains("soc"));QVERIFY(!r.contains("acLoad"));
        auto future=VrmClient::normalize({record("bs",50,now+10000)});
        QCOMPARE(future["soc"].toObject()["timestamp"].toInt(),0);
    }
};
QTEST_GUILESS_MAIN(VrmTests)
#include "vrm-tests.moc"
