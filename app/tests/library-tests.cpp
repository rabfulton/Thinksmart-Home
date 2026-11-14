#include "../library.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QFile>
#include <QDirIterator>
#include <QJsonDocument>
#include <QCryptographicHash>
#include <algorithm>
#include <QAbstractItemModelTester>
#include <QPersistentModelIndex>
struct Request { QUrl url;QByteArray authorization;QJsonObject body; };
class LibraryTests:public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QTcpServer server;
    QList<Request> requests;
    int failure=0;
    bool direct=false,foreign=false,paged=false;
    QUrl base;
    void config(const QJsonObject &provider,const QString &name){
        QFile file(directory.filePath("media.json"));QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Truncate));file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument(QJsonObject{{name,provider}}).toJson());
    }
    QJsonObject navConfig(){return {{"url",base.toString()},{"apiKey","fixture-nav-key"},{"allowHttp",true}};}
    QJsonObject jellyConfig(){return {{"url",base.toString()},{"accessToken","fixture-jelly-token"},{"userId","user-1"},{"allowHttp",true}};}
    void accept(QTcpSocket *socket){
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QTcpSocket::readyRead,this,[this,socket](){
            QByteArray data=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",data);
            const auto split=data.indexOf("\r\n\r\n");if(split<0)return;
            auto lines=data.left(split).split('\n');int length=0;QByteArray authorization;
            for(auto line:lines){line=line.trimmed();if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();if(line.toLower().startsWith("authorization:"))authorization=line.mid(14).trimmed();}
            if(data.size()<split+4+length)return;socket->disconnect(this);
            const auto first=lines.first().trimmed().split(' ');
            Request request{QUrl::fromEncoded(first.value(1)),authorization,QJsonDocument::fromJson(data.mid(split+4,length)).object()};requests.append(request);
            QString path=request.url.path();QJsonObject response;int status=failure?failure:200;
            if(!failure){
                if(path.endsWith("getAlbumList2.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"albumList2",QJsonObject{{"album",QJsonArray{QJsonObject{{"id","album-1"},{"name","Fixture album"},{"artist","Fixture artist"}}}}}}}}};
                else if(path.endsWith("getAlbum.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"album",QJsonObject{{"song",QJsonArray{QJsonObject{{"id","song-1"},{"title","First"},{"artist","Fixture artist"},{"duration",180}},QJsonObject{{"id","song-2"},{"title","Second"},{"artist","Fixture artist"},{"duration",210}}}}}}}}};
                else if(path.endsWith("/Genres"))response={{"Items",QJsonArray{QJsonObject{{"Id","genre-1"},{"Name","Jazz & Blues"}}}}};
                else if(path.endsWith("/Views"))response={{"Items",QJsonArray{QJsonObject{{"Id","library-1"},{"Name","Films"}}}}};
                else if(path.endsWith("/Items")&&paged){QJsonArray rows;int start=QUrlQuery(request.url).queryItemValue("StartIndex").toInt();for(int i=start;i<qMin(start+60,130);i++)rows.append(QJsonObject{{"Id","film-"+QString::number(i)},{"Name","Film "+QString::number(i)},{"Type","Movie"},{"ProductionYear",1980+i%10}});response={{"Items",rows},{"TotalRecordCount",130}};}
                else if(path.endsWith("/Items"))response={{"Items",QJsonArray{QJsonObject{{"Id","movie-1"},{"Name","Fixture film"},{"Type","Movie"},{"Overview","A fixture synopsis"},{"Genres",QJsonArray{"Drama"}},{"ImageTags",QJsonObject{{"Primary","poster-tag"}}},{"RunTimeTicks",36000000000LL},{"UserData",QJsonObject{{"PlaybackPositionTicks",1200000000LL}}}}}}};
                else if(path.endsWith("/PlaybackInfo"))response={{"PlaySessionId","session-1"},{"MediaSources",QJsonArray{QJsonObject{{"Id","source-1"},{"SupportsDirectPlay",direct},{"TranscodingUrl",foreign?"https://foreign.invalid/video.m3u8":"/base/Videos/movie-1/master.m3u8?api_key=fixture-jelly-token"}}}}};
                else if(path.contains("/Sessions/")||path.endsWith("/ActiveEncodings")){status=204;}
                else if(path.endsWith("/Users/Me"))response={{"Id","user-1"}};
                else status=404;
            }
            QByteArray body=status==204?QByteArray():QJsonDocument(response).toJson(QJsonDocument::Compact);
            socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
        });
    }
private slots:
    void initTestCase(){
        QVERIFY(directory.isValid());qputenv("XDG_CACHE_HOME",directory.filePath("cache").toUtf8());
        qputenv("THINKSMART_MEDIA_CONFIG",directory.filePath("media.json").toUtf8());
        QCoreApplication::setOrganizationName("ThinkSmartTests");QCoreApplication::setApplicationName("MediaProtocol");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.filePath("settings"));
        QVERIFY(server.listen(QHostAddress::LocalHost));base=QUrl(QString("http://127.0.0.1:%1/base/").arg(server.serverPort()));
        connect(&server,&QTcpServer::newConnection,this,[this](){while(server.hasPendingConnections())accept(server.nextPendingConnection());});
    }
    void init(){requests.clear();failure=0;direct=false;foreign=false;paged=false;}
    void subsonicAuthentication(){
        auto key=MediaLibrary::authenticatedSubsonicUrl(base,navConfig(),"stream",QUrlQuery("id=song-1"));QUrlQuery query(key);
        QCOMPARE(query.queryItemValue("apiKey"),QString("fixture-nav-key"));QVERIFY(!query.hasQueryItem("u"));QVERIFY(!query.hasQueryItem("p"));
        auto url=MediaLibrary::authenticatedSubsonicUrl(base,{{"username","name+tag@example.test"},{"password","fixture-password"}},"getAlbumList2");
        QUrlQuery token(url);QCOMPARE(token.queryItemValue("u",QUrl::FullyDecoded),QString("name+tag@example.test"));QVERIFY(url.toEncoded().contains("%2B"));QVERIFY(!token.hasQueryItem("p"));
        QByteArray expected=QCryptographicHash::hash(("fixture-password"+token.queryItemValue("s")).toUtf8(),QCryptographicHash::Md5).toHex();
        QCOMPARE(token.queryItemValue("t").toLatin1(),expected);QVERIFY(token.queryItemValue("s").size()>=6);
    }
    void navidromeBrowseQueueAndOfflineCache(){
        config(navConfig(),"navidrome");MediaLibrary library("navidrome");QVERIFY(library.configured());
        library.refresh();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),1);
        QVERIFY(requests.last().url.path().startsWith("/base/rest/"));QVERIFY(!QUrlQuery(requests.last().url).hasQueryItem("u"));
        library.open(0);QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),2);
        QSignalSpy play(&library,&MediaLibrary::playRequested);library.open(0);QCOMPARE(play.count(),1);
        QCOMPARE(QUrlQuery(play.at(0).at(1).toUrl()).queryItemValue("id"),QString("song-1"));
        library.advance(1);QCOMPARE(play.count(),2);QCOMPARE(QUrlQuery(play.at(1).at(1).toUrl()).queryItemValue("id"),QString("song-2"));
        failure=503;library.refresh();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),2);QVERIFY(library.cached());
        QDirIterator files(directory.filePath("cache"),{"library-*.json"},QDir::Files,QDirIterator::Subdirectories);bool found=false;
        while(files.hasNext()){QFile file(files.next());QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("fixture-nav-key"));QVERIFY(!(file.permissions()&QFile::ReadOther));found=true;}QVERIFY(found);
    }
    void jellyfinPlayback_data(){QTest::addColumn<bool>("directPlay");QTest::newRow("transcoded")<<false;QTest::newRow("direct")<<true;}
    void jellyfinPlayback(){
        QFETCH(bool,directPlay);direct=directPlay;config(jellyConfig(),"jellyfin");MediaLibrary library("jellyfin");
        library.refresh();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),1);
        QVERIFY(requests.last().authorization.contains("MediaBrowser Token=\"fixture-jelly-token\""));QVERIFY(!QUrlQuery(requests.last().url).hasQueryItem("api_key"));
        QSignalSpy play(&library,&MediaLibrary::playRequested);library.open(0);QTRY_COMPARE(play.count(),1);
        auto request=requests.last();QCOMPARE(request.body.value("DeviceProfile").toObject().value("Name").toString(),QString("ThinkSmart View"));
        QCOMPARE(request.body.value("MaxWidth").toInt(),1920);QCOMPARE(request.body.value("MaxHeight").toInt(),1080);
        const auto profile=request.body.value("DeviceProfile").toObject();QVERIFY(profile.value("DirectPlayProfiles").toArray().first().toObject().value("VideoCodec").toString().contains("hevc"));
        QUrl stream=play.first().at(1).toUrl();QVERIFY(MediaLibrary::sameOrigin(stream,base));QVERIFY(stream.path().startsWith("/base/"));QVERIFY(!stream.toString().contains("fixture-jelly-token"));
        QVERIFY(play.first().at(4).toStringList().first().contains("fixture-jelly-token"));QCOMPARE(play.first().at(7).toDouble(),directPlay?120.0:0.0);
        QString key=play.first().at(6).toString();int count=requests.size();library.report(key,"start",directPlay?120:0,false);QTRY_COMPARE(requests.size(),count+1);
        QCOMPARE(requests.last().body.value("PositionTicks").toDouble(),1200000000.0);
        count=requests.size();library.report(key,"stopped",directPlay?125:5,false);QTRY_COMPARE(requests.size(),count+(directPlay?1:2));
        QVERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.url.path().endsWith("/Stopped")&&r.body.value("PositionTicks").toDouble()==1250000000.0;}));
        if(!directPlay)QVERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.url.path().endsWith("/ActiveEncodings")&&QUrlQuery(r.url).queryItemValue("playSessionId")=="session-1"&&!QUrlQuery(r.url).queryItemValue("deviceId").isEmpty();}));
    }
    void jellyfinCategoriesAndDetails(){
        config(jellyConfig(),"jellyfin");MediaLibrary library("jellyfin");library.refresh();QTRY_VERIFY(!library.busy());QTRY_COMPARE(library.genres().size(),1);QTRY_COMPARE(library.libraries().size(),1);
        library.setGenre("Jazz & Blues");QTRY_VERIFY(!library.busy());
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.url.path().endsWith("/Items")&&QUrlQuery(r.url).queryItemValue("Genres")=="Jazz & Blues";}));
        library.inspect(0);QCOMPARE(library.detail().value("title").toString(),QString("Fixture film"));QCOMPARE(library.detail().value("overview").toString(),QString("A fixture synopsis"));
        QVERIFY(library.detail().value("poster").toString().startsWith("image://jellyfin/movie-1/Primary"));QVERIFY(!library.detail().value("poster").toString().contains("fixture-jelly-token"));
        QSignalSpy play(&library,&MediaLibrary::playRequested);library.playSelection(true);QTRY_COMPARE(play.count(),1);QCOMPARE(play.first().at(7).toDouble(),0.0);
        library.closeDetails();QVERIFY(library.detail().isEmpty());library.setSection("recent");QTRY_VERIFY(!library.busy());
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return QUrlQuery(r.url).queryItemValue("SortBy")=="DateCreated";}));
    }
    void jellyfinLazyAppendAndDecadeFilter(){
        paged=true;config(jellyConfig(),"jellyfin");MediaLibrary library("jellyfin");
        QAbstractItemModelTester tester(library.itemModel(),QAbstractItemModelTester::FailureReportingMode::QtTest);
        library.refresh();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),60);QVERIFY(library.canNext());
        QPersistentModelIndex first=library.itemModel()->index(0,0);QSignalSpy resets(library.itemModel(),&QAbstractItemModel::modelReset);QSignalSpy inserts(library.itemModel(),&QAbstractItemModel::rowsInserted);
        library.loadMore();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),120);QCOMPARE(inserts.count(),1);QCOMPARE(resets.count(),0);QVERIFY(first.isValid());QCOMPARE(first.data(Qt::UserRole+1).toMap().value("id").toString(),QString("film-0"));
        library.loadMore();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),130);QCOMPARE(library.itemModel()->rowCount(),130);QVERIFY(!library.canNext());
        library.setDecade(1980);QTRY_VERIFY(!library.busy());QCOMPARE(library.decade(),1980);QCOMPARE(library.items().size(),60);
        QVERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){auto years=QUrlQuery(r.url).queryItemValue("Years").split(',');return years.size()==10&&years.first()=="1980"&&years.last()=="1989";}));
        library.setDecade(1985);QCOMPARE(library.decade(),1980);
        failure=503;library.refresh();QTRY_VERIFY(!library.busy());QVERIFY(library.cached());QCOMPARE(library.items().size(),60);QVERIFY(!library.canNext());
    }
    void rejectsForeignStream(){
        foreign=true;config(jellyConfig(),"jellyfin");MediaLibrary library("jellyfin");library.refresh();QTRY_VERIFY(!library.busy());
        QSignalSpy play(&library,&MediaLibrary::playRequested);library.open(0);QTRY_VERIFY(!library.busy());QCOMPARE(play.count(),0);QVERIFY(library.problem().contains("different host"));
    }
};
QTEST_GUILESS_MAIN(LibraryTests)
#include "library-tests.moc"
