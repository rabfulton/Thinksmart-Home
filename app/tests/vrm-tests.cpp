#include "../vrm.h"
#include <QtTest>
#include <QDateTime>
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
