#include "../weather.h"
#include "../radio.h"
#include "radio-server.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QtTest>
#include <QDateTime>
#include <QTimeZone>
#include <QJsonArray>
#include <QUrlQuery>
class WeatherRadioTests:public QObject {
    Q_OBJECT
    QTemporaryDir root;
private slots:
    void init(){
        QVERIFY(root.isValid());QCoreApplication::setApplicationName(QUuid::createUuid().toString(QUuid::WithoutBraces));qputenv("XDG_CACHE_HOME",root.path().toUtf8());qputenv("THINKSMART_RADIO_FAVOURITES_FILE",root.filePath(QCoreApplication::applicationName()+"-favourites.json").toUtf8());
        const auto path=root.filePath("curated.json");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("{\"stations\":[{\"id\":\"radio4\",\"name\":\"BBC Radio 4\",\"genres\":[\"news\"],\"url\":\"https://bbc.example.test/radio4\"}]}");file.close();qputenv("THINKSMART_RADIO_CATALOG",path.toUtf8());
    }
    void cleanup(){qunsetenv("THINKSMART_WEATHER_CONFIG");qunsetenv("THINKSMART_WEATHER_GEOCODING_URL");qunsetenv("THINKSMART_WEATHER_FORECAST_URL");qunsetenv("THINKSMART_DISABLE_WEATHER");DeviceConfig::reload();qunsetenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS");qunsetenv("THINKSMART_RADIO_CATALOG");}
    void directoryFiltersAndCandidateValidation(){
        QString error;auto filters=RadioDirectory::filters({{"country","Columbia"},{"language","es"},{"tag"," Cumbia "}},error);
        QVERIFY(error.isEmpty());QCOMPARE(filters.value("country").toString(),QString("CO"));QCOMPARE(filters.value("language").toString(),QString("spanish"));
        QCOMPARE(RadioDirectory::filters({{"country","Japan"}},error).value("country").toString(),QString("JP"));
        QCOMPARE(RadioDirectory::filters({{"country","Germany"}},error).value("country").toString(),QString("DE"));
        QCOMPARE(RadioDirectory::filters({{"country","United Kingdom"}},error).value("country").toString(),QString("GB"));
        RadioDirectory::filters({{"tag",17}},error);QVERIFY(!error.isEmpty());error.clear();RadioDirectory::filters({},error);QVERIFY(!error.isEmpty());
        auto a=RadioServer::row();QJsonArray rows{a,a,RadioServer::row("Duplicate stream",a.value("url").toString()),RadioServer::row("Local","http://127.0.0.1/private"),RadioServer::row("Private","http://192.168.1.2/private"),RadioServer::row("File","file:///etc/passwd")};
        auto broken=RadioServer::row("Broken");broken.insert("lastcheckok",0);rows.append(broken);
        for(int i=0;i<7;i++)rows.append(RadioServer::row("Station "+QString::number(i),"https://stream.example.test/"+QString::number(i)));
        const auto result=RadioDirectory::normalize(rows);QCOMPARE(result.size(),5);QCOMPARE(result[0].toObject().value("url"),a.value("url"));QVERIFY(result[0].toObject().value("id").toString().startsWith("rb:"));
    }
    void directorySearchCoalescesCachesAndCanPlayReturnedIds(){
        RadioServer server;server.delay=60;server.rows={RadioServer::row()};qputenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS",server.url().toUtf8());
        RadioCatalog radio;QSignalSpy interrupt(&radio,&RadioCatalog::interruptPlayback),played(&radio,&RadioCatalog::playRequested);
        const QJsonObject query{{"country","CO"},{"language","spanish"},{"tag","cumbia"}};int completed=0;QJsonObject result;
        radio.search(query,[&](QJsonObject r){result=r;completed++;});radio.search(query,[&](QJsonObject){completed++;});
        QCOMPARE(completed,0);QTRY_COMPARE(completed,2);QCOMPARE(server.searches(),1);QVERIFY(result.value("ok").toBool());QCOMPARE(interrupt.size(),0);
        const auto params=QUrlQuery(server.requests.first());QCOMPARE(params.queryItemValue("countrycode"),QString("CO"));QCOMPARE(params.queryItemValue("language"),QString("spanish"));QCOMPARE(params.queryItemValue("tag"),QString("cumbia"));QCOMPARE(params.queryItemValue("hidebroken"),QString("true"));QVERIFY(server.agents.first().startsWith("ThinkSmart-Home/"));
        const auto id=result.value("stations").toArray().first().toObject().value("id").toString();
        QVERIFY(radio.knows(id));QVERIFY(radio.knows("radio4"));QVERIFY(!radio.knows("rb:unknown"));
        radio.search(query,[&](QJsonObject r){QCOMPARE(r.value("cache").toString(),QString("fresh"));completed++;});QCOMPARE(completed,3);QCOMPARE(server.searches(),1);
        QVERIFY(!radio.play("https://invented.example/radio"));QCOMPARE(interrupt.size(),0);
        QVERIFY(radio.play(id));QCOMPARE(played.size(),1);QCOMPARE(played.first()[1].toUrl(),QUrl("https://stream.example.test/radio"));
        QVERIFY(radio.play("radio4"));QCOMPARE(played.size(),2);QCOMPARE(played.last()[1].toUrl(),QUrl("https://bbc.example.test/radio4"));QCOMPARE(server.searches(),1);
        RadioDirectory restored;QVERIFY(!restored.station(id).isEmpty());
        auto path=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/radio-browser/searches.json";QFile file(path);QVERIFY(file.permissions()&QFile::ReadOwner);QVERIFY(!(file.permissions()&QFile::ReadOther));
    }
    void uiSearchAndFavouritesSurviveCacheExpiry(){
        RadioServer server;for(int i=0;i<55;i++)server.rows.append(RadioServer::row("Jazz "+QString::number(i),"https://radio.example.test/"+QString::number(i)));
        qputenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS",server.url().toUtf8());RadioCatalog radio;
        radio.searchStations("Jazz");QTRY_VERIFY(!radio.searchBusy());QCOMPARE(radio.resultModel()->rowCount(),50);QCOMPARE(server.searches(),1);
        const auto entry=radio.resultModel()->data(radio.resultModel()->index(0,0),Qt::UserRole+1).toMap();const auto id=entry.value("id").toString();
        QSignalSpy reset(radio.resultModel(),&QAbstractItemModel::modelReset),changed(radio.resultModel(),&QAbstractItemModel::dataChanged);
        QVERIFY(radio.favourite(id,true));QCOMPARE(reset.size(),0);QCOMPARE(changed.size(),1);QCOMPARE(radio.favouriteCount(),1);QVERIFY(!entry.contains("url"));
        QVERIFY(!(QFileInfo(RadioCatalog::favouritesPath()).permissions()&QFile::ReadOther));
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/radio-browser").removeRecursively();RadioCatalog restored;QVERIFY(restored.knows(id));QCOMPARE(restored.favouriteCount(),1);QSignalSpy playback(&restored,&RadioCatalog::playRequested);QVERIFY(restored.play(id));QCOMPARE(playback.size(),1);
        QVERIFY(restored.favourite(id,false));QCOMPARE(restored.favouriteCount(),0);
        QFile f(RadioCatalog::favouritesPath());QVERIFY(f.open(QIODevice::WriteOnly));f.write("broken");f.close();RadioCatalog corrupt;QVERIFY(!corrupt.favourite("radio4",true));QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),QByteArray("broken"));
    }
    void directoryFailsOverAndRetainsStaleCache(){
        RadioServer bad,good;bad.status=503;good.rows={RadioServer::row()};qputenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS",(bad.url()+";"+good.url()).toUtf8());
        RadioDirectory directory;QJsonObject result;bool done=false;const QJsonObject query{{"tag","cumbia"}};
        directory.search(query,[&](QJsonObject r){result=r;done=true;});QTRY_VERIFY(done);QVERIFY(result.value("ok").toBool());QCOMPARE(bad.searches(),1);QCOMPARE(good.searches(),1);
        const auto path=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/radio-browser/searches.json";QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto entries=QJsonDocument::fromJson(file.readAll()).object();file.close();
        for(const auto &key:entries.keys()){auto entry=entries.value(key).toObject();entry.insert("fetchedAt",QDateTime::currentSecsSinceEpoch()-86400);entries.insert(key,entry);}QVERIFY(file.open(QIODevice::WriteOnly));file.write(QJsonDocument(entries).toJson());file.close();
        good.malformed=true;RadioDirectory offline;done=false;offline.search(query,[&](QJsonObject r){result=r;done=true;});QTRY_VERIFY(done);QVERIFY(result.value("ok").toBool());QCOMPARE(result.value("cache").toString(),QString("stale"));QCOMPARE(result.value("stations").toArray().size(),1);
        done=false;offline.search({{"tag","unknown"}},[&](QJsonObject r){result=r;done=true;});QTRY_VERIFY(done);QVERIFY(!result.value("ok").toBool());
    }
    void weatherKeepsMissingAndZeroSeparate(){
        const qint64 now=QDateTime::currentSecsSinceEpoch();
        QJsonObject response{{"current",QJsonObject{{"time",now},{"temperature_2m",0},{"precipitation",0},{"weather_code",0}}},
            {"hourly",QJsonObject{{"time",QJsonArray{now-7200,now,now+3600}},{"temperature_2m",QJsonArray{2,0,4}}}},
            {"daily",QJsonObject{{"time",QJsonArray{now}},{"temperature_2m_max",QJsonArray{9}}}}};
        const auto data=WeatherClient::normalize(response,now);QVERIFY(!data.isEmpty());
        auto current=data.value("current").toObject();QVERIFY(current.value("temperature_2m").isDouble());QCOMPARE(current.value("temperature_2m").toDouble(),0.0);
        QVERIFY(current.value("wind_speed_10m").isNull());QCOMPARE(current.value("condition").toString(),QString("Clear sky"));
        const auto hour=data.value("hourly").toArray();QCOMPARE(hour.size(),1);QCOMPARE(hour.first().toObject().value("time").toDouble(),double(now+3600));QVERIFY(hour.first().toObject().value("precipitation_probability").isNull());
        QVERIFY(WeatherClient::normalize({},now).isEmpty());
        response.insert("current",QJsonObject{{"time",now+86400},{"temperature_2m",4}});QVERIFY(WeatherClient::normalize(response,now).isEmpty());
        auto query=QUrlQuery(WeatherClient::forecastUrl());QCOMPARE(query.queryItemValue("timezone"),QString("Europe/London"));QCOMPARE(query.queryItemValue("wind_speed_unit"),QString("mph"));
    }
    void weatherLocationSearchPersistenceAndCacheIsolation(){
        qputenv("THINKSMART_DISABLE_WEATHER","1");qputenv("THINKSMART_WEATHER_CONFIG",root.filePath("weather-location.json").toUtf8());QFile::remove(DeviceConfig::weatherPath());
        const auto original=DeviceConfig::weather();const auto oldKey=DeviceConfig::weatherKey();
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));
        const QJsonObject berlin{{"name","Berlin"},{"latitude",52.52},{"longitude",13.405},{"timezone","Europe/Berlin"},{"country","Germany"}};
        const auto body=QJsonDocument(QJsonObject{{"results",QJsonArray{berlin,QJsonObject{{"name","Invalid"}}}}}).toJson();
        connect(&server,&QTcpServer::newConnection,this,[&](){auto socket=server.nextPendingConnection();connect(socket,&QTcpSocket::readyRead,socket,[socket,body](){if(!socket->readAll().contains("\r\n\r\n"))return;socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();});connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);});
        qputenv("THINKSMART_WEATHER_GEOCODING_URL",("http://127.0.0.1:"+QString::number(server.serverPort())).toUtf8());
        auto cached=original;cached.insert("location",cached.take("name"));cached.insert("current",QJsonObject{{"temperature_2m",12}});
        const auto cacheDir=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);QDir().mkpath(cacheDir);QFile cache(cacheDir+"/weather-"+oldKey+".json");QVERIFY(cache.open(QIODevice::WriteOnly));cache.write(QJsonDocument(cached).toJson());cache.close();
        WeatherClient weather;QCOMPARE(weather.current().value("temperature_2m").toInt(),12);QSignalSpy changed(&weather,&WeatherClient::changed);weather.searchLocations("Berlin");QTRY_VERIFY(!weather.searching());QCOMPARE(weather.locations().size(),1);
        QVERIFY(weather.selectLocation(0));QCOMPARE(weather.location(),QString("Berlin"));QVERIFY(DeviceConfig::weatherKey()!=oldKey);QVERIFY(weather.current().isEmpty());QVERIFY(!changed.isEmpty());
        QVERIFY(!(QFileInfo(DeviceConfig::weatherPath()).permissions()&QFile::ReadOther));
        auto query=QUrlQuery(WeatherClient::forecastUrl());QCOMPARE(query.queryItemValue("timezone"),QString("Europe/Berlin"));QVERIFY(query.queryItemValue("hourly").contains("is_day"));
        WeatherClient restored;QCOMPARE(restored.location(),QString("Berlin"));QVERIFY(DeviceConfig::context().contains("Berlin"));
        QCOMPARE(restored.snapshot().value("location").toString(),QString("Berlin"));
        QVERIFY(!weather.selectLocation(-1));QCOMPARE(weather.location(),QString("Berlin"));
        QVERIFY(weather.resetLocation());QCOMPARE(DeviceConfig::weather(),original);QCOMPARE(DeviceConfig::weatherKey(),oldKey);
    }
    void locationChangeCancelsAnOldForecast(){
        qputenv("THINKSMART_DISABLE_WEATHER","1");qputenv("THINKSMART_WEATHER_CONFIG",root.filePath("race-location.json").toUtf8());QFile::remove(DeviceConfig::weatherPath());
        QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));int forecasts=0;
        const auto url=("http://127.0.0.1:"+QString::number(server.serverPort())).toUtf8();qputenv("THINKSMART_WEATHER_FORECAST_URL",url);qputenv("THINKSMART_WEATHER_GEOCODING_URL",url);
        connect(&server,&QTcpServer::newConnection,this,[&](){auto socket=server.nextPendingConnection();connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);connect(socket,&QTcpSocket::readyRead,socket,[&,socket](){
            auto bytes=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",bytes);if(socket->property("handled").toBool()||!bytes.contains("\r\n\r\n"))return;socket->setProperty("handled",true);
            const auto query=QUrlQuery(QUrl::fromEncoded(bytes.split(' ').value(1)));QJsonObject response;int delay=0;
            if(query.hasQueryItem("name"))response={{"results",QJsonArray{QJsonObject{{"name","Berlin"},{"latitude",52.52},{"longitude",13.405},{"timezone","Europe/Berlin"}}}}};
            else{forecasts++;delay=forecasts==1?250:0;response={{"current",QJsonObject{{"time",QDateTime::currentSecsSinceEpoch()},{"temperature_2m",forecasts==1?5:20}}}};}
            const auto body=QJsonDocument(response).toJson();QTimer::singleShot(delay,socket,[socket,body](){socket->write("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();});
        });});
        WeatherClient weather;weather.searchLocations("Berlin");QTRY_COMPARE(weather.locations().size(),1);qunsetenv("THINKSMART_DISABLE_WEATHER");weather.refresh();QTRY_COMPARE(forecasts,1);QVERIFY(weather.busy());QVERIFY(weather.selectLocation(0));
        QTRY_COMPARE(weather.current().value("temperature_2m").toInt(),20);QCOMPARE(forecasts,2);QTest::qWait(300);QCOMPARE(weather.location(),QString("Berlin"));QCOMPARE(weather.current().value("temperature_2m").toInt(),20);
        qputenv("THINKSMART_DISABLE_WEATHER","1");WeatherClient restored;QCOMPARE(restored.current().value("temperature_2m").toInt(),20);QFile::remove(DeviceConfig::weatherPath());
    }
    void weatherSolarTimesAndHourlyConditionsUseSelectedTimezone(){
        qputenv("THINKSMART_WEATHER_CONFIG",root.filePath("solar-location.json").toUtf8());QFile f(DeviceConfig::weatherPath());QVERIFY(f.open(QIODevice::WriteOnly));f.write(R"({"name":"Berlin","latitude":52.52,"longitude":13.405,"timezone":"Europe/Berlin"})");f.close();
        const qint64 now=QDateTime(QDate(2026,1,10),QTime(12,0),QTimeZone::UTC).toSecsSinceEpoch();
        auto solar=now-5*3600;
        const auto data=WeatherClient::normalize({{"current",QJsonObject{{"time",now},{"temperature_2m",0},{"weather_code",71},{"is_day",0}}},{"hourly",QJsonObject{{"time",QJsonArray{now+3600}},{"weather_code",QJsonArray{95}},{"is_day",QJsonArray{0}}}},{"daily",QJsonObject{{"time",QJsonArray{now}},{"sunrise",QJsonArray{solar}},{"sunset",QJsonArray{now+3*3600}}}}},now);
        QCOMPARE(data.value("daily").toArray()[0].toObject().value("sunriseLabel").toString(),QString("08:00"));
        QCOMPARE(data.value("daily").toArray()[0].toObject().value("sunsetLabel").toString(),QString("16:00"));
        QCOMPARE(data.value("hourly").toArray()[0].toObject().value("is_day").toInt(),0);
        QCOMPARE(data.value("hourly").toArray()[0].toObject().value("condition").toString(),QString("Thunderstorms"));
        QVERIFY(!DeviceConfig::validWeather({{"name","Bad"},{"latitude",91},{"longitude",0},{"timezone","UTC"}}));
        QFile::remove(DeviceConfig::weatherPath());
    }
    void parliamentSelectsChamberAndExcludesFutureAndDuplicates(){
        const QString today=QDateTime::currentDateTime(QTimeZone("Europe/London")).toString("d MMMM yyyy");
        const QString tomorrow=QDateTime::currentDateTime(QTimeZone("Europe/London")).addDays(1).toString("d MMMM yyyy");
        auto anchor=[](const QString &id,const QString &title,const QString &date){return QString("<a href=\"https://parliamentlive.tv/Event/Index/%1\"><h5>%3</h5><p> %2 </p></a>").arg(id,title,date);};
        QString html=anchor("11111111-1111-1111-1111-111111111111","House of Commons",tomorrow)
            +anchor("22222222-2222-2222-2222-222222222222","BSL - House of Commons",today)
            +anchor("33333333-3333-3333-3333-333333333333","House of Commons",today)
            +anchor("33333333-3333-3333-3333-333333333333","House of Commons",today);
        auto rows=RadioCatalog::parliamentEvents(html,"Commons");QCOMPARE(rows.size(),1);QCOMPARE(rows.first().toObject().value("id").toString(),QString("33333333-3333-3333-3333-333333333333"));
    }
};
QTEST_GUILESS_MAIN(WeatherRadioTests)
#include "weather-radio-tests.moc"
