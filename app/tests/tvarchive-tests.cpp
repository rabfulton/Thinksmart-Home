#include "../tvarchive.h"
#include "../deviceconfig.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>

class TvArchiveTests:public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QTcpServer server;
    int requests=0;
    bool offline=false;
    QJsonObject metadata()const {
        QJsonArray files;
        for(int i:{10,2,1}){
            const auto path=QString("Example Show/Season 1/Example.Show.S01E%1.Title.avi").arg(i,2,10,QChar('0'));
            files.append(QJsonObject{{"name",path},{"size","900000000"},{"original","collection.torrent"}});
            files.append(QJsonObject{{"name",path.chopped(4)+".mp4"},{"original",path},{"size","90000000"},{"height","480"},{"length","1200"}});
            files.append(QJsonObject{{"name",path.chopped(4)+"_512kb.mp4"},{"original",path},{"size","60000000"},{"height","360"},{"length","1200"}});
        }
        files.append(QJsonObject{{"name","Another Show/Another.Show.2x01.Pilot.mp4"},{"size","70000000"}});
        files.append(QJsonObject{{"name","../unsafe.mp4"},{"size","100"}});
        return {{"result",files}};
    }
    void save(const QString &path,const QByteArray &bytes){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());}
    QJsonArray catalogue()const{return TvArchiveClient::parseCatalogue(metadata());}
    void seed(){save(TvArchiveClient::cachePath(),QJsonDocument(QJsonObject{{"version",1},{"collection",TvArchiveClient::collection()},{"updated",QDateTime::currentSecsSinceEpoch()},{"episodes",catalogue()}}).toJson());}
    QString id(int index)const{return catalogue()[index].toObject().value("id").toString();}
    QString sid()const{return catalogue()[1].toObject().value("seriesId").toString();}
private slots:
    void initTestCase(){
        QVERIFY(dir.isValid());QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,this,[this](){auto s=server.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,s,[this,s](){auto b=s->property("buffer").toByteArray()+s->readAll();s->setProperty("buffer",b);if(!b.contains("\r\n\r\n")||s->property("served").toBool())return;s->setProperty("served",true);++requests;const auto body=QJsonDocument(metadata()).toJson();s->write(QByteArray(offline?"HTTP/1.1 503 Unavailable\r\n":"HTTP/1.1 200 OK\r\n")+"Content-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();});});
    }
    void init(){
        requests=0;offline=false;qunsetenv("THINKSMART_TVARCHIVE_PROXY");qputenv("THINKSMART_DEVICE_CONFIG",dir.filePath("device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",dir.filePath("missing.json").toUtf8());qputenv("THINKSMART_TVARCHIVE_FILE",dir.filePath("state.json").toUtf8());qputenv("THINKSMART_TVARCHIVE_CACHE",dir.filePath("cache.json").toUtf8());qputenv("THINKSMART_TVARCHIVE_TEST_URL",QString("http://127.0.0.1:%1/files").arg(server.serverPort()).toUtf8());
        QFile::remove(TvArchiveClient::storagePath());QFile::remove(TvArchiveClient::cachePath());save(DeviceConfig::path(),"{\"modules\":{\"tvarchive\":true}}");DeviceConfig::reload();
    }
    void choosesSmallMp4AndKeepsStableIdentities(){
        const auto rows=catalogue();QCOMPARE(rows.size(),4);QCOMPARE(rows[1].toObject().value("number").toInt(),1);QCOMPARE(rows[2].toObject().value("number").toInt(),2);QCOMPARE(rows[3].toObject().value("number").toInt(),10);QCOMPARE(rows[0].toObject().value("season").toInt(),2);
        for(int n=1;n<4;n++){QCOMPARE(rows[n].toObject().value("format").toString(),QString("MP4"));QCOMPARE(rows[n].toObject().value("size").toDouble(),60000000.);}
        auto original=metadata();auto files=original.value("result").toArray();for(int i=files.size()-1;i>=0;--i)if(files[i].toObject().value("name").toString().endsWith(".mp4"))files.removeAt(i);original.insert("result",files);const auto avi=TvArchiveClient::parseCatalogue(original);QCOMPARE(avi.size(),3);QCOMPARE(avi[0].toObject().value("id"),rows[1].toObject().value("id"));
        QVERIFY(rows[1].toObject().value("url").toString().contains("Example%20Show"));
    }
    void mixedNamesAndExtrasKeepEpisodeOrder(){
        QJsonArray files;for(const auto &name:{"Example/Season 2/202 - Second.mp4","Example/Season 2/201 - First.mp4","Example/Season 1/example.e01.pilot.mp4","Example/Season 1/Extras/interview.mp4"})files.append(QJsonObject{{"name",name},{"size","1000"}});
        const auto rows=TvArchiveClient::parseCatalogue({{"files",files}});QCOMPARE(rows.size(),4);QCOMPARE(rows[0].toObject().value("number").toInt(),1);QCOMPARE(rows[1].toObject().value("number").toInt(),1);QCOMPARE(rows[2].toObject().value("number").toInt(),2);QVERIFY(rows[3].toObject().value("extra").toBool());
        save(TvArchiveClient::cachePath(),QJsonDocument(QJsonObject{{"version",1},{"collection",TvArchiveClient::collection()},{"episodes",rows}}).toJson());TvArchiveClient c;c.setAutoplay(true);QSignalSpy played(&c,&TvArchiveClient::playRequested);QVERIFY(c.play(rows[2].toObject().value("id").toString()));const auto key=played.last()[2].toString();c.report(key,"start",0,1200,false);c.completed(key);QCOMPARE(played.size(),1);
    }
    void compactAndUnseparatedEpisodeNames(){
        QJsonArray files;for(const auto &name:{"Example/Season 1/prefixs01e02.avi","Example/Season 2/01 - First.mp4","Example/Season 3/Example 3.mp4","Example/Movies/Example 1997.mp4"})files.append(QJsonObject{{"name",name},{"size","1000"}});
        const auto rows=TvArchiveClient::parseCatalogue({{"files",files}});QCOMPARE(rows[0].toObject().value("number").toInt(),2);QCOMPARE(rows[1].toObject().value("number").toInt(),1);QCOMPARE(rows[2].toObject().value("number").toInt(),3);QVERIFY(rows[3].toObject().value("extra").toBool());
    }
    void resumeSurvivesRestartAndFailedStartDoesNotErase(){
        seed();TvArchiveClient c;QVERIFY(!c.autoplay());QSignalSpy played(&c,&TvArchiveClient::playRequested);QVERIFY(c.play(id(1)));QCOMPARE(c.selected().value("id").toString(),sid());QCOMPARE(played.size(),1);const auto key=played.last()[2].toString();c.report(key,"start",0,1200,false);c.report(key,"progress",45,1200,true);c.report(key,"stopped",46,1200,false);
        TvArchiveClient restored;QSignalSpy resumed(&restored,&TvArchiveClient::playRequested);QVERIFY(restored.resume());QCOMPARE(resumed.last()[3].toDouble(),46.);restored.report(key,"stopped",0,0,false);restored.completed(key);QCOMPARE(resumed.size(),1);QCOMPARE(restored.current().value("position").toDouble(),46.);restored.report(key,"start",0,1200,false);QCOMPARE(restored.current().value("position").toDouble(),46.);
        QVERIFY(!(QFileInfo(TvArchiveClient::storagePath()).permissions()&QFile::ReadOther));restored.report(key,"progress",50,1200,false);QCOMPARE(restored.current().value("position").toDouble(),50.);
    }
    void eofAdvancesButStopDoesNotAutoplay(){
        seed();TvArchiveClient c;QSignalSpy played(&c,&TvArchiveClient::playRequested);QVERIFY(c.play(id(1)));auto key=played.last()[2].toString();c.report(key,"start",0,1200,false);c.report(key,"stopped",32,1200,false);QCOMPARE(c.current().value("id").toString(),id(1));c.completed(key);QCOMPARE(c.current().value("id").toString(),id(2));QCOMPARE(played.size(),1);c.completed(key);QCOMPARE(played.size(),1);
        c.setAutoplay(true);QVERIFY(c.resume(sid()));key=played.last()[2].toString();c.report(key,"start",0,1200,false);c.completed(key);QCOMPARE(played.size(),3);QCOMPARE(c.current().value("id").toString(),id(3));key=played.last()[2].toString();c.report(key,"start",0,1200,false);c.completed(key);QCOMPARE(played.size(),3);TvArchiveClient restored;QVERIFY(restored.autoplay());
    }
    void randomStaysInSeriesAndAvoidsImmediateRepeats(){
        seed();TvArchiveClient c;c.setAutoplay(true);QSignalSpy played(&c,&TvArchiveClient::playRequested);QVERIFY(c.random(sid()));const auto previous=played.last()[2].toString();c.report(previous,"start",0,1200,false);c.completed(previous);QCOMPARE(played.size(),2);QVERIFY(previous!=played.last()[2].toString());QCOMPARE(c.current().value("seriesId").toString(),sid());
        QVERIFY(!c.request({{"action","tvarchive.play"},{"id","https://evil.invalid/test.mp4"}}).value("ok").toBool());QVERIFY(!c.request({{"action","tvarchive.autoplay"},{"enabled","false"}}).value("ok").toBool());
        QCOMPARE(c.request({{"action","tvarchive.list"}}).value("items").toArray().size(),2);QCOMPARE(c.request({{"action","tvarchive.list"},{"id",sid()}}).value("items").toArray().size(),3);
    }
    void refreshCachesAndFailureKeepsLastCatalogue(){
        TvArchiveClient c;QVERIFY(!c.ready());QVERIFY(c.request({{"action","tvarchive.refresh"}}).value("ok").toBool());QTRY_VERIFY(!c.busy());QCOMPARE(requests,1);QCOMPARE(c.series()->rowCount(),2);QVERIFY(c.knows(id(1)));offline=true;TvArchiveClient restored;QCOMPARE(restored.series()->rowCount(),2);restored.refresh();QTRY_VERIFY(!restored.busy());QCOMPARE(requests,2);QVERIFY(restored.ready());QVERIFY(!restored.problem().isEmpty());
    }
    void disabledAndCorruptStateRemainInert(){
        seed();save(DeviceConfig::path(),"{\"modules\":{\"tvarchive\":false}}");DeviceConfig::reload();TvArchiveClient disabled;disabled.setActive(true);disabled.refresh();QVERIFY(!disabled.resume());QVERIFY(!disabled.random());QVERIFY(!disabled.active());QCOMPARE(disabled.series()->rowCount(),0);QTest::qWait(20);QCOMPARE(requests,0);
        save(DeviceConfig::path(),"{\"modules\":{\"tvarchive\":true}}");DeviceConfig::reload();save(TvArchiveClient::storagePath(),"broken");TvArchiveClient bad;QVERIFY(!bad.play(id(1)));QVERIFY(!bad.problem().isEmpty());QFile f(TvArchiveClient::storagePath());QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("broken"));
    }
    void unsafeCacheIsRejected(){
        auto rows=catalogue();auto row=rows[0].toObject();row.insert("url","https://archive.org/download/classic-british-tv-pack/../../other.mp4");rows[0]=row;save(TvArchiveClient::cachePath(),QJsonDocument(QJsonObject{{"version",1},{"collection",TvArchiveClient::collection()},{"episodes",rows}}).toJson());TvArchiveClient c;QVERIFY(!c.ready());
    }
#ifdef THINKSMART_ENABLE_TVARCHIVE_PROXY
    void optionalProxyValidationAndNoDirectFallback(){
        qputenv("THINKSMART_TVARCHIVE_PROXY","http://outside.invalid:40000");TvArchiveClient bad;QVERIFY(!bad.problem().isEmpty());bad.refresh();QVERIFY(!bad.busy());QCOMPARE(requests,0);
        QTcpServer closed;QVERIFY(closed.listen(QHostAddress::LocalHost));const auto port=closed.serverPort();closed.close();qputenv("THINKSMART_TVARCHIVE_PROXY",QString("http://127.0.0.1:%1").arg(port).toUtf8());TvArchiveClient failed;failed.refresh();QTRY_VERIFY(!failed.busy());QVERIFY(!failed.problem().isEmpty());QCOMPARE(requests,0);
    }
#else
    void proxySettingIgnoredWhenCompiledOut(){qputenv("THINKSMART_TVARCHIVE_PROXY","http://127.0.0.1:1");TvArchiveClient c;c.refresh();QTRY_VERIFY(!c.busy());QCOMPARE(requests,1);QVERIFY(c.ready());QVERIFY(TvArchiveClient::proxyUrl().isEmpty());}
#endif
};
QTEST_GUILESS_MAIN(TvArchiveTests)
#include "tvarchive-tests.moc"
