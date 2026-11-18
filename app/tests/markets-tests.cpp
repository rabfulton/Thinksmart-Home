#include "../markets.h"
#include "../deviceconfig.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>

class MarketsTests:public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    void write(const QString &path,const QJsonObject &data){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(data).toJson());}
    static QJsonObject body(const QString &symbol,qint64 now,double price=110,bool crypto=false){
        return {{"chart",QJsonObject{{"result",QJsonArray{QJsonObject{
            {"meta",QJsonObject{{"symbol",symbol},{"regularMarketPrice",price},{"regularMarketTime",now},{"previousClose",100},{"currency","USD"},{"instrumentType",crypto?"CRYPTOCURRENCY":"EQUITY"},{"currentTradingPeriod",QJsonObject{{"regular",QJsonObject{{"start",now-3600},{"end",now+3600}}}}}}},
            {"timestamp",QJsonArray{now-86400,now-300,now}},
            {"indicators",QJsonObject{{"quote",QJsonArray{QJsonObject{{"close",QJsonArray{80,105,price}}}}}}}
        }}}}}};
    }
private slots:
    void init(){
        QVERIFY(dir.isValid());qputenv("THINKSMART_DEVICE_CONFIG",dir.filePath("device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",dir.filePath("absent-dashboard.json").toUtf8());
        qputenv("THINKSMART_MARKETS_FILE",dir.filePath("markets.json").toUtf8());qputenv("THINKSMART_MARKETS_CACHE",dir.filePath("cache.json").toUtf8());qputenv("THINKSMART_DISABLE_MARKETS","1");
        QFile::remove(MarketsClient::settingsPath());QFile::remove(MarketsClient::cachePath());qunsetenv("THINKSMART_MARKETS_TEST_URL");
        write(DeviceConfig::path(),{{"modules",QJsonObject{{"markets",true}}}});DeviceConfig::reload();
    }
    void quotesSessionsAndRollingChange(){
        const auto now=QDateTime::currentSecsSinceEpoch();const auto r=body("NVDA",now);
        const auto q=MarketsClient::normalize(r,"NVDA",now);QCOMPARE(q.value("percent").toDouble(),10.);QCOMPARE(q.value("marketStatus").toString(),QString("Open"));
        QCOMPARE(MarketsClient::normalize(r,"NVDA",now+7200).value("marketStatus").toString(),QString("Closed"));
        const auto crypto=MarketsClient::normalize(body("BTC-USD",now,100,true),"BTC-USD",now);QCOMPARE(crypto.value("percent").toDouble(),25.);QCOMPARE(crypto.value("changePeriod").toString(),QString("24h"));
        QVERIFY(MarketsClient::normalize(r,"MSFT",now).isEmpty());QVERIFY(MarketsClient::normalize(body("NVDA",now+1000),"NVDA",now).isEmpty());QVERIFY(MarketsClient::normalize(body("NVDA",now,0),"NVDA",now).isEmpty());
        QCOMPARE(MarketsClient::canonical("SPX500USD"),QString("^GSPC"));QCOMPARE(MarketsClient::canonical("dxy"),QString("DX-Y.NYB"));QCOMPARE(MarketsClient::canonical("btc"),QString("BTC-USD"));QVERIFY(MarketsClient::canonical("https://example.test").isEmpty());
    }
    void watchlistPersistsAndCorruptionIsNotOverwritten(){
        MarketsClient client;QVERIFY(client.setSymbols("NVDA, MSFT, DXY, BTC, NVDA"));QCOMPARE(client.symbols(),(QStringList{"NVDA","MSFT","DX-Y.NYB","BTC-USD"}));QVERIFY(client.select("MSFT"));client.setMode("focus");
        QVERIFY(!(QFileInfo(MarketsClient::settingsPath()).permissions()&QFile::ReadOther));
        MarketsClient restored;QCOMPARE(restored.focus(),QString("MSFT"));QCOMPARE(restored.mode(),QString("focus"));QVERIFY(!restored.setSymbols(""));QVERIFY(!restored.setSymbols("NVDA, url:bad"));
        QFile f(MarketsClient::settingsPath());QVERIFY(f.open(QIODevice::WriteOnly));f.write("broken");f.close();MarketsClient bad;QVERIFY(!bad.setSymbols("NVDA"));QVERIFY(!bad.addAlert("NVDA","above",200));QVERIFY(!bad.problem().isEmpty());QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("broken"));
    }
    void alertsCrossOnceAndIgnoreStaleReplayAndRestart(){
        MarketsClient client;const auto now=QDateTime::currentSecsSinceEpoch();QSignalSpy fired(&client,&MarketsClient::alertTriggered);
        QVERIFY(client.addAlert("NVDA","above",120));
        auto feed=[&](qint64 t,double price,qint64 receipt){client.accept("NVDA",MarketsClient::normalize(body("NVDA",t,price),"NVDA",receipt),receipt);};
        feed(now,130,now);QCOMPARE(fired.size(),0); // Already above on first quote is not a crossing.
        feed(now+1,110,now+1);feed(now+2,125,now+2);QCOMPARE(fired.size(),1);QVERIFY(!client.alerts()[0].toMap().value("armed").toBool());
        feed(now+3,110,now+3);feed(now+4,130,now+4);QCOMPARE(fired.size(),1);
        const auto id=client.alerts()[0].toMap().value("id").toString();QVERIFY(client.rearmAlert(id));
        feed(now+5,110,now+5);feed(now+5,130,now+6);QCOMPARE(fired.size(),1); // Duplicate timestamp.
        feed(now+6,130,now+600);QCOMPARE(fired.size(),1); // Delayed quote clears baseline.
        feed(now+601,130,now+601);QCOMPARE(fired.size(),1);
        client.saveCache();MarketsClient restored;QSignalSpy afterRestart(&restored,&MarketsClient::alertTriggered);
        restored.accept("NVDA",MarketsClient::normalize(body("NVDA",now+602,130),"NVDA",now+602),now+602);QCOMPARE(afterRestart.size(),0);
        QVERIFY(!client.setSymbols("BTC"));QVERIFY(client.removeAlert(id));QVERIFY(client.setSymbols("BTC"));
    }
    void disabledModuleIsInert(){
        write(DeviceConfig::path(),{{"modules",QJsonObject{{"markets",false}}}});DeviceConfig::reload();MarketsClient client;
        client.refresh();client.setActive(true);QVERIFY(!client.active());QVERIFY(!client.busy());QVERIFY(!client.setSymbols("NVDA"));QVERIFY(!client.addAlert("NVDA","above",123));QVERIFY(!client.request({{"action","markets.get"}}).value("ok").toBool());QVERIFY(!QFile::exists(MarketsClient::settingsPath()));QVERIFY(!QFile::exists(MarketsClient::cachePath()));
    }
    void networkUpdatesAreBoundedAndFailuresKeepQuotes(){
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));int requests=0;bool fail=false;
        connect(&server,&QTcpServer::newConnection,this,[&](){auto *socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[&,socket](){
            auto bytes=socket->property("request").toByteArray()+socket->readAll();socket->setProperty("request",bytes);if(!bytes.contains("\r\n\r\n"))return;
            ++requests;const auto url=QUrl::fromEncoded(bytes.split(' ').value(1));const auto symbol=url.path().mid(1);const auto b=fail?QByteArray("unavailable"):QJsonDocument(body(symbol,QDateTime::currentSecsSinceEpoch())).toJson();
            socket->write((fail?QByteArray("HTTP/1.1 429 Too Many Requests\r\n"):QByteArray("HTTP/1.1 200 OK\r\n"))+"Content-Length: "+QByteArray::number(b.size())+"\r\nConnection: close\r\n\r\n"+b);socket->disconnectFromHost();
        });connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);});
        qputenv("THINKSMART_MARKETS_TEST_URL",QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());MarketsClient client;QVERIFY(client.setSymbols("NVDA"));client.refresh();QTRY_VERIFY(!client.busy());QCOMPARE(requests,1);QVERIFY(client.selected().value("available").toBool());
        client.refresh();QCOMPARE(requests,1);fail=true;client.m_lastRefresh=0;client.m_charts={};client.refresh();QTRY_VERIFY(!client.busy());QCOMPARE(requests,2);QVERIFY(client.selected().value("available").toBool());QVERIFY(client.problem().contains("rate limit"));client.m_lastRefresh=0;client.refresh();QTest::qWait(20);QCOMPARE(requests,2);
    }
};
QTEST_GUILESS_MAIN(MarketsTests)
#include "markets-tests.moc"
