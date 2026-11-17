#include "../iptv.h"
#include "../deviceconfig.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QSignalSpy>
#include <QDateTime>

class IptvTests:public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QJsonObject channel(QString id="News.test",bool adult=false){return {{"id",id},{"name","World News"},{"country","GB"},{"categories",QJsonArray{"news"}},{"is_nsfw",adult},{"closed",QJsonValue::Null},{"website","https://news.example"}};}
    QJsonObject stream(QString id="News.test",QString url="https://cdn.news.example/live.m3u8",QString quality="720p",QString feed="English"){return {{"channel",id},{"feed",feed},{"title","World News English"},{"url",url},{"quality",quality},{"labels",QJsonArray{}}};}
    void write(QString name,QJsonObject data){QFile f(dir.filePath(name));QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(data).toJson());}
private slots:
    void initTestCase(){QVERIFY(dir.isValid());qputenv("THINKSMART_DEVICE_CONFIG",dir.filePath("device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",dir.filePath("dashboard.json").toUtf8());qputenv("THINKSMART_IPTV_FILE",dir.filePath("favourites.json").toUtf8());qputenv("THINKSMART_IPTV_CACHE",dir.filePath("cache.json").toUtf8());}
    void init(){QFile::remove(dir.filePath("favourites.json"));QFile::remove(dir.filePath("cache.json"));write("device.json",{{"modules",QJsonObject{{"iptv",true}}}});DeviceConfig::reload();}
    void selectionAndSafety(){
        auto adult=channel("Adult.test",true),closed=channel("Closed.test");closed.insert("closed","2025-01-01");
        auto foreign=stream("News.test","https://other.example/1080.m3u8","1080p");
        auto referrer=stream("News.test","https://other.example/header.m3u8");referrer.insert("referrer","https://other.example");
        auto rows=IptvClient::normalize({channel(),adult,closed},{foreign,stream(),stream("Adult.test"),stream("Closed.test"),stream("News.test","file:///etc/passwd"),stream("News.test","https://evil.example/4k.m3u8","2160p"),referrer});
        QCOMPARE(rows.size(),1);QCOMPARE(rows[0].toObject().value("url").toString(),QString("https://cdn.news.example/live.m3u8"));
        const auto id=rows[0].toObject().value("id");auto otherFeed=stream("News.test","https://cdn.news.example/french.m3u8","720p","French");
        QCOMPARE(IptvClient::normalize({channel()},{stream(),otherFeed}).size(),2);
        QCOMPARE(IptvClient::normalize({channel()},{stream()})[0].toObject().value("id"),id);
        QVERIFY(!IptvClient::validUrl("https://example.test/a\n"));QVERIFY(!IptvClient::validUrl("rtsp://example.test/live"));
    }
    void selectSingleHdVariant(){
        const QByteArray master=R"M3U(#EXTM3U
#EXT-X-STREAM-INF:BANDWIDTH=1000000,CODECS="avc1.64001f,mp4a.40.2",RESOLUTION=960x540
low.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=5192000,CODECS="avc1.640020,mp4a.40.2",RESOLUTION=1280x720
hd.m3u8?videoId=4
#EXT-X-STREAM-INF:BANDWIDTH=20000000,CODECS="hvc1.1.6.L153,mp4a.40.2",RESOLUTION=3840x2160
ultra.m3u8
)M3U";
        QCOMPARE(IptvClient::selectHlsVariant(master,QUrl("https://example.test/live/master.m3u8")),QUrl("https://example.test/live/hd.m3u8?videoId=4"));
        QVERIFY(IptvClient::selectHlsVariant("bad playlist",QUrl("https://example.test/live")).isEmpty());
        QVERIFY(IptvClient::selectHlsVariant("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=5,CODECS=\"avc1.640020\",RESOLUTION=1280x720,AUDIO=\"separate\"\nhd.m3u8\n",QUrl("https://example.test/live")).isEmpty());
    }
    void bbcMasterIsResolvedAndCanBeCancelled(){
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));qputenv("THINKSMART_IPTV_TEST_PLAYLIST",QString("http://127.0.0.1:%1/live/master.m3u8").arg(server.serverPort()).toUtf8());
        auto reset=qScopeGuard([](){qunsetenv("THINKSMART_IPTV_TEST_PLAYLIST");});
        connect(&server,&QTcpServer::newConnection,this,[&](){while(server.hasPendingConnections()){auto s=server.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,this,[s](){auto bytes=s->property("input").toByteArray()+s->readAll();s->setProperty("input",bytes);if(!bytes.contains("\r\n\r\n"))return;const QByteArray body="#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=5192000,CODECS=\"avc1.640020,mp4a.40.2\",RESOLUTION=1280x720\nhd.m3u8?videoId=4\n";s->write("HTTP/1.1 200 OK\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();});}});
        IptvClient c;QVERIFY(c.addCustom("BBC test","https://n-5-1.eu.cdn-redge.media/livehls/oeu/mt/live/bbc_news/live.livx/playlist.m3u8"));
        const auto id=c.favourites().first().toMap().value("id").toString();QSignalSpy playback(&c,&IptvClient::playRequested);QVERIFY(c.play(id));QVERIFY(c.preparing());QTRY_COMPARE(playback.size(),1);
        QVERIFY(!c.preparing());QCOMPARE(playback.first().first().toUrl(),QUrl(QString("http://127.0.0.1:%1/live/hd.m3u8?videoId=4").arg(server.serverPort())));
        QVERIFY(c.play(id));c.cancelPlayback();QTest::qWait(30);QVERIFY(!c.preparing());QCOMPARE(playback.size(),1);
        IptvClient restarted;QCOMPARE(restarted.lastChannel().value("id").toString(),id);QCOMPARE(restarted.favourites().size(),1);
    }
    void favouriteToggleKeepsScrolledModelStable(){
        IptvClient c;const auto id=c.suggestions().first().toMap().value("id").toString();
        auto model=c.suggestedModel();QSignalSpy reset(model,&QAbstractItemModel::modelReset),removed(model,&QAbstractItemModel::rowsRemoved),changed(model,&QAbstractItemModel::dataChanged);
        QVERIFY(c.favourite(id,true));QCOMPARE(reset.size(),0);QCOMPARE(removed.size(),0);QCOMPARE(changed.size(),1);
        QVERIFY(c.favourite(id,false));QCOMPARE(reset.size(),0);QCOMPARE(changed.size(),2);
        QVERIFY(IptvClient::softwareDecode(QUrl("https://n-5-1.eu.cdn-redge.media/livehls/oeu/mt/live/bbc_news/live.livx/playlist.m3u8")));
        QVERIFY(!IptvClient::softwareDecode(QUrl("https://live.france24.com/live.m3u8")));
    }
    void savedChannelsPrivateAndPersistent(){
        IptvClient c;QVERIFY(!c.suggestions().isEmpty());const auto id=c.suggestions()[0].toMap().value("id").toString();
        QVERIFY(c.favourite(id,true));QSignalSpy playback(&c,&IptvClient::playRequested);QVERIFY(c.play(id));QCOMPARE(playback.size(),1);
        QVERIFY(c.addCustom("My TV","https://user:private@example.test/live.m3u8?token=private"));QCOMPARE(c.favourites().size(),2);
        auto result=c.request({{"action","iptv.list"}});QVERIFY(!QJsonDocument(result).toJson().contains("private"));QVERIFY(!QJsonDocument(result).toJson().contains("m3u8"));
        IptvClient restored;QCOMPARE(restored.favourites().size(),2);QCOMPARE(restored.lastChannel().value("id").toString(),id);QVERIFY(restored.favourite(id,false));QCOMPARE(restored.favourites().size(),1);
        QVERIFY(!QFileInfo(IptvClient::storagePath()).permissions().testFlag(QFile::ReadOther));
        QFile f(IptvClient::storagePath());QVERIFY(f.open(QIODevice::WriteOnly));f.write("damaged");f.close();IptvClient corrupt;QVERIFY(!corrupt.addCustom("Test","https://example.test/live"));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("damaged"));
    }
    void failedRefreshKeepsCachedAndSavedChannels(){
        auto rows=IptvClient::normalize({channel()},{stream()});write("cache.json",{{"version",1},{"updated",1},{"channels",rows}});
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));qputenv("THINKSMART_IPTV_TEST_API",QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        connect(&server,&QTcpServer::newConnection,this,[&](){auto s=server.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,this,[s](){s->readAll();s->write("HTTP/1.1 503 Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");s->disconnectFromHost();});});
        IptvClient c;const auto id=rows[0].toObject().value("id").toString();QVERIFY(c.favourite(id,true));bool done=false;QJsonObject result;
        c.search({{"action","iptv.search"},{"search","World News"}},[&](auto r){result=r;done=true;});QTRY_VERIFY(done);QVERIFY(result.value("ok").toBool());QVERIFY(result.value("stale").toBool());QVERIFY(!result.value("warning").toString().isEmpty());QCOMPARE(result.value("channels").toArray().size(),1);QVERIFY(c.play(id));
    }
    void refreshCacheAndDisabledIsolation(){
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));int requests=0;
        qputenv("THINKSMART_IPTV_TEST_API",QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        connect(&server,&QTcpServer::newConnection,this,[&](){while(server.hasPendingConnections()){auto s=server.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,this,[&,s](){auto input=s->property("input").toByteArray()+s->readAll();s->setProperty("input",input);if(!input.contains("\r\n\r\n"))return;++requests;const auto bytes=QJsonDocument(input.startsWith("GET /channels")?QJsonArray{channel()}:QJsonArray{stream()}).toJson(QJsonDocument::Compact);s->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(bytes.size())+"\r\nConnection: close\r\n\r\n"+bytes);s->disconnectFromHost();});}});
        IptvClient c;bool done=false;QJsonObject result;c.search({{"action","iptv.search"},{"country","GB"},{"category","news"}},[&](auto r){done=true;result=r;});QTRY_VERIFY(done);QCOMPARE(requests,2);QCOMPARE(result.value("channels").toArray().size(),1);QVERIFY(!result.value("stale").toBool());QVERIFY(!QJsonDocument(result).toJson().contains("m3u8"));
        IptvClient cached;QCOMPARE(cached.request({{"action","iptv.search"},{"search","World News"}}).value("channels").toArray().size(),1);QVERIFY(!cached.busy());
        write("device.json",{{"modules",QJsonObject{{"iptv",false}}}});DeviceConfig::reload();IptvClient disabled;disabled.setActive(true);disabled.refresh();QVERIFY(!disabled.active());QVERIFY(!disabled.busy());QVERIFY(disabled.suggestions().isEmpty());QVERIFY(!disabled.request({{"action","iptv.play"}}).value("ok").toBool());QCOMPARE(requests,2);
    }
};
QTEST_GUILESS_MAIN(IptvTests)
#include "iptv-tests.moc"
