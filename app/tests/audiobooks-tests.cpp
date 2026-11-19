#include "../audiobooks.h"
#include "../deviceconfig.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QScopeGuard>
#include <QImage>
#include <QBuffer>

class AudiobooksTests:public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    QTcpServer server;
    QList<QUrl> requests;
    bool offline=false,withCover=false;
    QJsonObject book()const {
        QJsonArray tracks;
        for(int i=1;i<=2;i++)tracks.append(QJsonObject{{"id",QString::number(i)},{"section_number",QString::number(i)},{"title","Chapter "+QString::number(i)},{"listen_url","https://archive.org/download/example/chapter"+QString::number(i)+".mp3"},{"playtime","180"},{"readers",QJsonArray{QJsonObject{{"display_name","Test reader"}}}}});
        QJsonObject result{{"id","753"},{"title","Moby Dick"},{"language","English"},{"num_sections","2"},{"totaltimesecs","360"},{"url_librivox","https://librivox.org/moby-dick-by-herman-melville/"},{"authors",QJsonArray{QJsonObject{{"first_name","Herman"},{"last_name","Melville"}}}},{"sections",tracks}};
        if(withCover)result.insert("coverart_thumbnail","https://archive.org/services/img/example");return result;
    }
    QByteArray top()const{return QJsonDocument(QJsonObject{{"generatedAt","2026-05-18"},{"books",QJsonArray{QJsonObject{{"r",1},{"t","Moby Dick"},{"a","Herman Melville"},{"y","Adventure|Classic Literature"},{"h",24},{"l","https://librivox.org/moby-dick-by-herman-melville/"}},QJsonObject{{"r",501},{"t","Outside the catalogue"},{"l","https://librivox.org/other/"}}}}}).toJson();}
    QByteArray picks()const{return "<rss xmlns:content=\"http://purl.org/rss/1.0/modules/content/\"><channel><item><title>Monthly adventure</title><pubDate>Sat, 02 Aug 2025 05:29:00 +0000</pubDate><content:encoded><![CDATA[<p><a href=\"https://librivox.org/moby-dick-by-herman-melville/\">Moby Dick</a><a href=\"https://librivox.org/category/news/\">Skip category</a></p>]]></content:encoded></item></channel></rss>";}
private slots:
    void initTestCase(){
        QVERIFY(dir.isValid());QCoreApplication::setOrganizationName("ThinkSmartTests");QCoreApplication::setApplicationName("Audiobooks");QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,this,[this](){auto *s=server.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,s,[this,s](){auto b=s->property("buffer").toByteArray()+s->readAll();s->setProperty("buffer",b);if(!b.contains("\r\n\r\n")||s->property("served").toBool())return;s->setProperty("served",true);const auto url=QUrl::fromEncoded(b.split(' ').value(1));requests<<url;QByteArray body;
            if(url.path()=="/api")body=QJsonDocument(QJsonObject{{"books",QJsonArray{book()}}}).toJson();else if(url.path()=="/top")body=top();else if(url.path()=="/picks")body=picks();else body="<a href=\"https://librivox.org/rss/753\">RSS</a>";
            s->write((offline?QByteArray("HTTP/1.1 503 Unavailable\r\n"):QByteArray("HTTP/1.1 200 OK\r\n"))+"Content-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);s->disconnectFromHost();});});
    }
    void init(){
        qunsetenv("THINKSMART_AUDIOBOOKS_PROXY");
        withCover=false;
        offline=false;requests.clear();qputenv("THINKSMART_DEVICE_CONFIG",dir.filePath("device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",dir.filePath("absent-dashboard.json").toUtf8());qputenv("THINKSMART_AUDIOBOOKS_FILE",dir.filePath("library.json").toUtf8());qputenv("THINKSMART_AUDIOBOOKS_CACHE",dir.filePath("cache").toUtf8());qputenv("THINKSMART_AUDIOBOOKS_TEST_BASE",QString("http://127.0.0.1:%1").arg(server.serverPort()).toUtf8());QFile::remove(AudiobooksClient::libraryPath());QDir(dir.filePath("cache")).removeRecursively();QFile f(DeviceConfig::path());QVERIFY(f.open(QIODevice::WriteOnly));f.write("{\"modules\":{\"audiobooks\":true}}");f.close();DeviceConfig::reload();
    }
    void discoveryIsBoundedAndPicksRetainDates(){
        QString date;const auto ranked=AudiobooksClient::parseTop(top(),&date);QCOMPARE(ranked.size(),1);QCOMPARE(date,QString("2026-05-18"));QVERIFY(ranked.first().toObject().value("genres").toArray().contains("Adventure"));
        const auto monthly=AudiobooksClient::parsePicks(picks());QCOMPARE(monthly.value("books").toArray().size(),1);QCOMPARE(monthly.value("periods").toArray().first().toObject().value("date").toString(),QString("2025-08-02"));
        auto invalid=book();invalid.insert("sections",QJsonArray{QJsonObject{{"id","1"},{"section_number","1"},{"listen_url","http://127.0.0.1/private.mp3"}}});QVERIFY(AudiobooksClient::normalize(invalid).value("tracks").toArray().isEmpty());
    }
    void progressSurvivesRestartAndOnlyEofAdvances(){
        AudiobooksClient client;QCOMPARE(client.current().value("id").toString(),QString("lv:753"));QSignalSpy played(&client,&AudiobooksClient::playRequested);QVERIFY(client.play("lv:753"));QTRY_COMPARE(played.size(),1);QCOMPARE(requests.size(),1);const auto key=played.last()[3].toString();
        client.report(key,"start",0,false);client.report(key,"progress",42,true);client.report(key,"stopped",43,false);QCOMPARE(client.current().value("position").toDouble(),43.);QVERIFY(!(QFileInfo(AudiobooksClient::libraryPath()).permissions()&QFile::ReadOther));
        AudiobooksClient restored;QSignalSpy resumed(&restored,&AudiobooksClient::playRequested);QVERIFY(restored.resume());QCOMPARE(resumed.size(),1);QCOMPARE(resumed.last()[4].toDouble(),43.);QCOMPARE(requests.size(),1);
        restored.report(key,"stopped",0,false);QCOMPARE(restored.current().value("position").toDouble(),43.);restored.report(key,"start",0,false);QCOMPARE(restored.current().value("position").toDouble(),43.);
        restored.completed(key);QCOMPARE(resumed.size(),2);QCOMPARE(restored.current().value("chapterIndex").toInt(),1);QCOMPARE(resumed.last()[4].toDouble(),0.);
        restored.report(key,"stopped",179,false);QCOMPARE(restored.current().value("chapterIndex").toInt(),1);QCOMPARE(restored.current().value("position").toDouble(),0.);
        const auto second=resumed.last()[3].toString();restored.report(second,"start",0,false);restored.report(second,"stopped",50,false);QVERIFY(!restored.current().value("completed").toBool());restored.completed(second);QVERIFY(restored.current().value("completed").toBool());QVERIFY(restored.resume());QCOMPARE(resumed.last()[4].toDouble(),0.);QCOMPARE(restored.current().value("chapterIndex").toInt(),0);
    }
    void cancellationAndCorruptStorageDoNotStartAudio(){
        AudiobooksClient client;QSignalSpy played(&client,&AudiobooksClient::playRequested);QVERIFY(client.play("lv:753"));client.cancelPending();QTRY_VERIFY(!client.busy());QCOMPARE(played.size(),0);QVERIFY(!QFile::exists(AudiobooksClient::libraryPath()));
        QFile f(AudiobooksClient::libraryPath());QVERIFY(f.open(QIODevice::WriteOnly));f.write("broken");f.close();AudiobooksClient bad;QVERIFY(!bad.resume());QVERIFY(!bad.problem().isEmpty());QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("broken"));
    }
    void topAndMonthlyCacheWithoutScanningCatalog(){
        AudiobooksClient client;client.setTab("top");QCOMPARE(client.items()->rowCount(),100);QVERIFY(requests.isEmpty());client.refresh();QTRY_VERIFY(!client.busy());QCOMPARE(client.items()->rowCount(),1);QCOMPARE(requests.size(),1);client.setTab("picks");QTRY_VERIFY(!client.busy());QCOMPARE(client.periods().size(),1);QCOMPARE(client.items()->rowCount(),1);QCOMPARE(requests.size(),2);
        offline=true;AudiobooksClient cached;cached.setTab("top");QCOMPARE(cached.items()->rowCount(),1);QCOMPARE(requests.size(),2);cached.setTab("picks");QCOMPARE(cached.items()->rowCount(),1);QCOMPARE(requests.size(),2);
    }
    void disabledModuleDoesNoWork(){
        QFile f(DeviceConfig::path());QVERIFY(f.open(QIODevice::WriteOnly));f.write("{\"modules\":{\"audiobooks\":false}}");f.close();DeviceConfig::reload();AudiobooksClient client;client.setTab("top");client.setActive(true);QVERIFY(!client.active());QVERIFY(!client.resume());QVERIFY(!client.request({{"action","audiobooks.list"}}).value("ok").toBool());QTest::qWait(30);QVERIFY(requests.isEmpty());
    }
    void bundledCatalogueSurvivesOfflineUpdate(){
        offline=true;AudiobooksClient client;client.setTab("top");QCOMPARE(client.items()->rowCount(),100);QVERIFY(requests.isEmpty());QVERIFY(!client.busy());client.refresh();QTRY_VERIFY(!client.busy());QCOMPARE(requests.size(),1);QCOMPARE(client.items()->rowCount(),100);QVERIFY(!client.problem().isEmpty());
    }
#ifdef THINKSMART_ENABLE_AUDIOBOOKS_PROXY
    void optionalProxyIsModuleScopedAndFailsWithoutDirectFallback(){
        withCover=true;QTcpServer proxy;QVERIFY(proxy.listen(QHostAddress::LocalHost));int hits=0;bool fail=false;QByteArray target,cover;
        QImage image(2,2,QImage::Format_RGB32);image.fill(Qt::black);QBuffer buffer(&cover);QVERIFY(buffer.open(QIODevice::WriteOnly));QVERIFY(image.save(&buffer,"PNG"));
        connect(&proxy,&QTcpServer::newConnection,this,[&](){auto *s=proxy.nextPendingConnection();connect(s,&QTcpSocket::disconnected,s,&QObject::deleteLater);connect(s,&QTcpSocket::readyRead,s,[&,s](){
            const auto bytes=s->property("bytes").toByteArray()+s->readAll();s->setProperty("bytes",bytes);if(!bytes.contains("\r\n\r\n")||s->property("served").toBool())return;s->setProperty("served",true);++hits;target=bytes.split(' ').value(1);
            s->write(QByteArray(fail?"HTTP/1.1 503 Unavailable\r\n":"HTTP/1.1 200 OK\r\n")+"Content-Length: "+QByteArray::number(cover.size())+"\r\nConnection: close\r\n\r\n"+cover);s->disconnectFromHost();
        });});
        const auto original=QNetworkProxy::applicationProxy();
        const auto endpoint=QString("http://127.0.0.1:%1").arg(proxy.serverPort());qputenv("THINKSMART_AUDIOBOOKS_PROXY",endpoint.toUtf8());
        auto cleanup=qScopeGuard([](){qunsetenv("THINKSMART_AUDIOBOOKS_PROXY");});
        AudiobooksClient client;QSignalSpy played(&client,&AudiobooksClient::playRequested);QVERIFY(client.play("lv:753"));QTRY_COMPARE(played.size(),1);QCOMPARE(hits,0);QCOMPARE(requests.size(),1);
        client.setTab("top");client.refresh();QTRY_VERIFY(!client.busy());QCOMPARE(client.items()->rowCount(),1);client.setTab("picks");QTRY_VERIFY(!client.busy());QCOMPARE(client.periods().size(),1);QCOMPARE(requests.size(),3);QCOMPARE(hits,0);
        QVERIFY(client.inspect("lv:753"));QTRY_VERIFY(client.detail().value("coverLocal").toString().startsWith("file:"));QCOMPARE(hits,1);QVERIFY(target.contains("/catalog/services/img/example"));QCOMPARE(requests.size(),3);
        QCOMPARE(QNetworkProxy::applicationProxy(),original);
        QVERIFY(AudiobooksClient::playerOptions("audiobooks").contains("--http-proxy="+endpoint));QVERIFY(AudiobooksClient::playerOptions("audiobooks").contains("--tls-verify=yes"));
        for(const auto &source:{"radio","navidrome","jellyfin","iptv","bluetooth","assistant"})QVERIFY(AudiobooksClient::playerOptions(source).isEmpty());
        QNetworkAccessManager unrelated;auto *reply=unrelated.get(QNetworkRequest(QUrl(QString("http://127.0.0.1:%1/direct").arg(server.serverPort()))));QSignalSpy finished(reply,&QNetworkReply::finished);QTRY_COMPARE(finished.size(),1);reply->deleteLater();QCOMPARE(requests.size(),4);QCOMPARE(hits,1);
        requests.clear();QDir(AudiobooksClient::cacheDirectory()+"/covers").removeRecursively();fail=true;AudiobooksClient unavailable;QVERIFY(unavailable.inspect("lv:753"));QTRY_VERIFY(!unavailable.busy());QCOMPARE(hits,2);QVERIFY(requests.isEmpty());QVERIFY(unavailable.detail().value("coverLocal").toString().isEmpty());
    }
    void invalidProxyDoesNotStartRequestsOrPlayback(){
        qputenv("THINKSMART_AUDIOBOOKS_PROXY","http://outside.invalid:40000");auto cleanup=qScopeGuard([](){qunsetenv("THINKSMART_AUDIOBOOKS_PROXY");});
        AudiobooksClient client;QVERIFY(!client.problem().isEmpty());QVERIFY(!client.resume());client.setTab("top");QVERIFY(!client.busy());QVERIFY(requests.isEmpty());
    }
#else
    void proxySettingHasNoEffectWhenCompiledOut(){
        qputenv("THINKSMART_AUDIOBOOKS_PROXY","http://127.0.0.1:1");auto cleanup=qScopeGuard([](){qunsetenv("THINKSMART_AUDIOBOOKS_PROXY");});
        AudiobooksClient client;QSignalSpy played(&client,&AudiobooksClient::playRequested);QVERIFY(client.play("lv:753"));QTRY_COMPARE(played.size(),1);QCOMPARE(requests.size(),1);QVERIFY(client.problem().isEmpty());
        const auto options=AudiobooksClient::playerOptions("audiobooks");QVERIFY(options.contains("--tls-verify=yes"));for(const auto &option:options)QVERIFY(!option.startsWith("--http-proxy="));
        for(const auto &source:{"radio","navidrome","jellyfin","iptv","bluetooth","assistant"})QVERIFY(AudiobooksClient::playerOptions(source).isEmpty());
    }
#endif
};
QTEST_MAIN(AudiobooksTests)
#include "audiobooks-tests.moc"
