#include "../library.h"
#include "../deviceconfig.h"
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
#include <QScopeGuard>
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
                if((path.endsWith("getAlbumList2.view")||path.endsWith("search3.view"))&&paged){
                    const bool search=path.endsWith("search3.view");const auto query=QUrlQuery(request.url);const int offset=query.queryItemValue(search?"songOffset":"offset").toInt();QJsonArray rows;
                    for(int i=offset;i<qMin(offset+60,130);i++)rows.append(QJsonObject{{"id",(search?"song-":"album-")+QString::number(i)},{"name","Fixture album "+QString::number(i)},{"title","Fixture track "+QString::number(i)},{"artist","Fixture artist"},{"year",1995}});
                    QJsonObject data{{"status","ok"}};data.insert(search?"searchResult3":"albumList2",QJsonObject{{search?"song":"album",rows}});response={{"subsonic-response",data}};
                }
                else if(path.endsWith("search3.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"searchResult3",QJsonObject{{"artist",QJsonArray{QJsonObject{{"id","artist-1"},{"name","Fixture artist"}}}},{"album",QJsonArray{QJsonObject{{"id","album-1"},{"name","Fixture album"},{"artist","Fixture artist"}}}},{"song",QJsonArray{QJsonObject{{"id","song-1"},{"title","First"},{"artist","Fixture artist"}}}}}}}}};
                else if(path.endsWith("getArtist.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"artist",QJsonObject{{"id","artist-1"},{"album",QJsonArray{QJsonObject{{"id","album-1"},{"name","Fixture album"},{"artist","Fixture artist"}}}}}}}}};
                else if(path.endsWith("getGenres.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"genres",QJsonObject{{"genre",QJsonArray{QJsonObject{{"value","Rock"},{"albumCount",12}},QJsonObject{{"value","Jazz & Blues"},{"albumCount",3}}}}}}}}};
                else if(path.endsWith("getAlbumList2.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"albumList2",QJsonObject{{"album",QJsonArray{QJsonObject{{"id","album-1"},{"name","Fixture album"},{"artist","Fixture artist"}}}}}}}}};
                else if(path.endsWith("getAlbum.view"))response={{"subsonic-response",QJsonObject{{"status","ok"},{"album",QJsonObject{{"song",QJsonArray{QJsonObject{{"id","song-1"},{"title","First"},{"artist","Fixture artist"},{"duration",180}},QJsonObject{{"id","song-2"},{"title","Second"},{"artist","Fixture artist"},{"duration",210}}}}}}}}};
                else if(path.endsWith("/Genres"))response={{"Items",QJsonArray{QJsonObject{{"Id","genre-1"},{"Name","Jazz & Blues"}}}}};
                else if(path.endsWith("/Views"))response={{"Items",QJsonArray{QJsonObject{{"Id","library-1"},{"Name","Films"}}}}};
                else if(path.endsWith("/Items")&&paged){QJsonArray rows;int start=QUrlQuery(request.url).queryItemValue("StartIndex").toInt();for(int i=start;i<qMin(start+60,130);i++)rows.append(QJsonObject{{"Id","film-"+QString::number(i)},{"Name","Film "+QString::number(i)},{"Type","Movie"},{"ProductionYear",1980+i%10}});response={{"Items",rows},{"TotalRecordCount",130}};}
                else if(path.endsWith("/Items"))response={{"Items",QJsonArray{QJsonObject{{"Id","movie-1"},{"Name","Fixture film"},{"Type","Movie"},{"Overview","A fixture synopsis"},{"Genres",QJsonArray{"Drama"}},{"ImageTags",QJsonObject{{"Primary","poster-tag"}}},{"RunTimeTicks",36000000000LL},{"UserData",QJsonObject{{"PlaybackPositionTicks",1200000000LL}}}}}}};
                else if(path.endsWith("/PlaybackInfo"))response={{"PlaySessionId","session-1"},{"MediaSources",QJsonArray{QJsonObject{{"Id","source-1"},{"SupportsDirectPlay",direct},{"TranscodingUrl",foreign?"https://foreign.invalid/video.m3u8":"/base/Videos/movie-1/master.m3u8?api_key=fixture-jelly-token"}}}}};
                else if(path.contains("/Sessions/")||path.endsWith("/ActiveEncodings")){status=204;}
                else if(path.endsWith("/Users/Me"))response={{"Id","user-1"}};
                else if(path.endsWith("/Users/AuthenticateByName"))response={{"AccessToken","fixture-login-token"},{"User",QJsonObject{{"Id","user-1"}}}};
                else status=404;
            }
            QByteArray body=status==204?QByteArray():QJsonDocument(response).toJson(QJsonDocument::Compact);
            socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
        });
    }
private slots:
    void initTestCase(){
        QVERIFY(directory.isValid());qputenv("XDG_CACHE_HOME",directory.filePath("cache").toUtf8());qputenv("XDG_DATA_HOME",directory.filePath("data").toUtf8());
        qputenv("THINKSMART_MEDIA_CONFIG",directory.filePath("media.json").toUtf8());
        QCoreApplication::setOrganizationName("ThinkSmartTests");QCoreApplication::setApplicationName("MediaProtocol");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.filePath("settings"));
        QVERIFY(server.listen(QHostAddress::LocalHost));base=QUrl(QString("http://127.0.0.1:%1/base/").arg(server.serverPort()));
        connect(&server,&QTcpServer::newConnection,this,[this](){while(server.hasPendingConnections())accept(server.nextPendingConnection());});
    }
    void init(){QDir(directory.filePath("data")).removeRecursively();requests.clear();failure=0;direct=false;foreign=false;paged=false;}
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
        QSignalSpy play(&library,&MediaLibrary::playRequested);library.open(0);library.open(1);QCOMPARE(play.count(),0);QCOMPARE(library.queueCount(),2);library.playQueue();QCOMPARE(play.count(),1);
        QCOMPARE(QUrlQuery(play.at(0).at(1).toUrl()).queryItemValue("id"),QString("song-1"));
        library.advance(1);QCOMPARE(play.count(),2);QCOMPARE(QUrlQuery(play.at(1).at(1).toUrl()).queryItemValue("id"),QString("song-2"));
        failure=503;library.refresh();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),2);QVERIFY(library.cached());
        QDirIterator files(directory.filePath("cache"),{"library-*.json"},QDir::Files,QDirIterator::Subdirectories);bool found=false;
        while(files.hasNext()){QFile file(files.next());QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("fixture-nav-key"));QVERIFY(!(file.permissions()&QFile::ReadOther));found=true;}QVERIFY(found);
    }
    void musicQueuePersistsReplaysAndClears(){
        config(navConfig(),"navidrome");MediaLibrary library("navidrome");library.refresh();QTRY_VERIFY(!library.busy());library.open(0);QTRY_VERIFY(!library.busy());
        QSignalSpy play(&library,&MediaLibrary::playRequested),cleared(&library,&MediaLibrary::queueCleared);QAbstractItemModelTester tester(library.queueModel(),QAbstractItemModelTester::FailureReportingMode::QtTest);
        library.open(0);library.open(1);library.open(0);QCOMPARE(library.queueCount(),3);QCOMPARE(play.size(),0);QCOMPARE(library.queueIndex(),-1);
        QPersistentModelIndex first=library.queueModel()->index(0,0);library.playQueue();QCOMPARE(play.size(),1);QCOMPARE(library.queueIndex(),0);QCOMPARE(QUrlQuery(play.last().at(1).toUrl()).queryItemValue("id"),QString("song-1"));
        library.refresh();QVERIFY(library.busy());library.advance(1);QCOMPARE(play.size(),2);QCOMPARE(library.queueIndex(),1);QTRY_VERIFY(!library.busy());QVERIFY(first.isValid());QCOMPARE(library.queueCount(),3);
        library.advance(1);QCOMPARE(library.queueIndex(),2);library.advance(1);QVERIFY(!library.queueFinished());library.completed("stale-key");QVERIFY(!library.queueFinished());library.completed(play.last().at(6).toString());QVERIFY(library.queueFinished());QCOMPARE(play.size(),3);QVERIFY(!library.queueCanNext());
        MediaLibrary restored("navidrome");QCOMPARE(restored.queueCount(),3);QCOMPARE(restored.queueIndex(),2);QVERIFY(restored.queueFinished());QSignalSpy resumed(&restored,&MediaLibrary::playRequested);QCOMPARE(resumed.size(),0);restored.playQueue();QCOMPARE(restored.queueIndex(),0);QCOMPARE(resumed.size(),1);
        library.replayQueue();QCOMPARE(library.queueIndex(),0);QVERIFY(!library.queueFinished());library.advance(1);library.advance(-1);QCOMPARE(library.queueIndex(),0);QCOMPARE(QUrlQuery(play.last().at(1).toUrl()).queryItemValue("id"),QString("song-1"));
        QDirIterator files(directory.filePath("data"),{"queue-*.json"},QDir::Files,QDirIterator::Subdirectories);QVERIFY(files.hasNext());const auto path=files.next();QFile saved(path);QVERIFY(saved.open(QIODevice::ReadOnly));const auto bytes=saved.readAll();saved.close();QVERIFY(!bytes.contains("fixture-nav-key"));QVERIFY(!bytes.contains("http://"));QVERIFY(!(saved.permissions()&(QFile::ReadGroup|QFile::ReadOther)));
        auto other=navConfig();other.insert("apiKey","different-account-key");config(other,"navidrome");MediaLibrary separate("navidrome");QCOMPARE(separate.queueCount(),0);config(navConfig(),"navidrome");
        QVERIFY(QFile::rename(path,path+".bak"));QVERIFY(QDir().mkdir(path));library.clearQueue();QCOMPARE(library.queueCount(),3);QCOMPARE(cleared.size(),0);QVERIFY(!library.queueProblem().isEmpty());QVERIFY(QDir().rmdir(path));QVERIFY(QFile::rename(path+".bak",path));
        library.clearQueue();QCOMPARE(library.queueCount(),0);QCOMPARE(cleared.size(),1);const int count=play.size();library.playQueue();library.advance(1);QCOMPARE(play.size(),count);MediaLibrary empty("navidrome");QCOMPARE(empty.queueCount(),0);
        library.open(0);QVERIFY(library.enqueueAlbum());QCOMPARE(library.queueCount(),3); // One chosen track, followed by both album tracks.
    }
    void navidromeLazyLoadingFiltersAndSearch(){
        paged=true;config(navConfig(),"navidrome");MediaLibrary library("navidrome");QAbstractItemModelTester tester(library.itemModel(),QAbstractItemModelTester::FailureReportingMode::QtTest);
        library.refresh();QTRY_VERIFY(!library.busy());QTRY_COMPARE(library.genres().size(),2);QCOMPARE(library.items().size(),60);QVERIFY(library.canNext());
        QPersistentModelIndex first=library.itemModel()->index(0,0);QSignalSpy resets(library.itemModel(),&QAbstractItemModel::modelReset),inserts(library.itemModel(),&QAbstractItemModel::rowsInserted);
        library.loadMore();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),120);QCOMPARE(inserts.size(),1);QCOMPARE(resets.size(),0);QVERIFY(first.isValid());
        QCOMPARE(QUrlQuery(requests.last().url).queryItemValue("offset"),QString("60"));library.loadMore();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),130);QVERIFY(!library.canNext());QCOMPARE(resets.size(),0);
        int count=requests.size();library.loadMore();QCOMPARE(requests.size(),count);
        library.setMusicBrowse("byGenre","Jazz & Blues");QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),60);auto query=QUrlQuery(requests.last().url);QCOMPARE(query.queryItemValue("type"),QString("byGenre"));QCOMPARE(query.queryItemValue("genre",QUrl::FullyDecoded),QString("Jazz & Blues"));QVERIFY(!query.hasQueryItem("fromYear"));
        library.open(0);QTRY_VERIFY(!library.busy());QVERIFY(library.inAlbum());QVERIFY(!library.canNext());library.back();QTRY_VERIFY(!library.busy());QVERIFY(!library.inAlbum());QCOMPARE(library.genre(),QString("Jazz & Blues"));
        library.setMusicBrowse("byYear","1990");QTRY_VERIFY(!library.busy());query=QUrlQuery(requests.last().url);QCOMPARE(query.queryItemValue("fromYear"),QString("1999"));QCOMPARE(query.queryItemValue("toYear"),QString("1990"));QVERIFY(!query.hasQueryItem("genre"));
        library.setMusicBrowse("alphabeticalByArtist");QTRY_VERIFY(!library.busy());QCOMPARE(QUrlQuery(requests.last().url).queryItemValue("type"),QString("alphabeticalByArtist"));
        library.search("Fixture artist");QTRY_VERIFY(!library.busy());QVERIFY(library.searching());QCOMPARE(library.items().size(),60);library.loadMore();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),120);QCOMPARE(QUrlQuery(requests.last().url).queryItemValue("songOffset"),QString("60"));
        failure=503;count=requests.size();library.loadMore();QTRY_VERIFY(!library.busy());QCOMPARE(library.items().size(),120);QVERIFY(!library.canNext());QTest::qWait(30);QCOMPARE(requests.size(),count+1);
        MediaLibrary offline("navidrome");QCOMPARE(offline.items().size(),130);QVERIFY(offline.cached());offline.refresh();QTRY_VERIFY(!offline.busy());
    }
    void musicAgentSearchSelectionAndCancellation(){
        config(navConfig(),"navidrome");MediaLibrary library("navidrome");QSignalSpy play(&library,&MediaLibrary::playRequested);
        auto call=[&](QJsonObject request){QJsonObject result;bool done=false;library.musicRequest(request,[&](QJsonObject r){result=r;done=true;});if(!QTest::qWaitFor([&](){return done;},2000))QTest::qFail("Music tool timed out",__FILE__,__LINE__);return result;};
        const auto original=library.items();const int before=requests.size();auto bad=call({{"action","music.play"},{"id","https://foreign.invalid/stream"},{"type","song"}});QVERIFY(!bad.value("ok").toBool());QCOMPARE(requests.size(),before);
        auto search=call({{"action","music.search"},{"search","Fixture artist"}});QVERIFY(search.value("ok").toBool());QCOMPARE(search.value("items").toArray().size(),3);QVERIFY(library.knowsMusic("artist-1","artist"));QVERIFY(library.knowsMusic("album-1","album"));
        auto albums=call({{"action","music.artist"},{"id","artist-1"}});QVERIFY(albums.value("ok").toBool());QCOMPARE(albums.value("items").toArray().size(),1);
        auto tracks=call({{"action","music.album"},{"id","album-1"}});QVERIFY(tracks.value("ok").toBool());QCOMPARE(tracks.value("items").toArray().size(),2);
        auto playing=call({{"action","music.play"},{"id","album-1"},{"type","album"}});QVERIFY(playing.value("ok").toBool());QCOMPARE(library.queueCount(),2);QCOMPARE(play.size(),1);
        auto add=call({{"action","music.enqueue"},{"id","song-1"},{"type","song"}});QVERIFY(add.value("ok").toBool());QCOMPARE(library.queueCount(),3);QCOMPARE(play.size(),1);QCOMPARE(library.items(),original);
        const auto queue=call({{"action","music.queue"}});QVERIFY(queue.value("ok").toBool());QVERIFY(!QJsonDocument(queue).toJson().contains("fixture-nav-key"));QVERIFY(!QJsonDocument(queue).toJson().contains("http://"));
        bool completed=false;QJsonObject cancelled;library.musicRequest({{"action","music.play"},{"id","album-1"},{"type","album"}},[&](QJsonObject r){cancelled=r;completed=true;});library.cancelMusicRequests();QTRY_VERIFY(completed);QVERIFY(!cancelled.value("ok").toBool());QCOMPARE(library.queueCount(),3);QCOMPARE(play.size(),1);
        failure=503;auto failed=call({{"action","music.play"},{"id","album-1"},{"type","album"}});QVERIFY(!failed.value("ok").toBool());QCOMPARE(library.queueCount(),3);QCOMPARE(play.size(),1);failure=0;
        const auto genres=call({{"action","music.genres"},{"search","jazz"}});QVERIFY(genres.value("ok").toBool());QCOMPARE(genres.value("items").toArray(),QJsonArray{"Jazz & Blues"});
        QVERIFY(call({{"action","music.browse"},{"mode","byYear"},{"decade",1990}}).value("ok").toBool());QCOMPARE(QUrlQuery(requests.last().url).queryItemValue("fromYear"),QString("1999"));
        const int count=requests.size();QVERIFY(!call({{"action","music.search"},{"search",""}}).value("ok").toBool());QCOMPARE(requests.size(),count);
        QVERIFY(call({{"action","music.clear"}}).value("ok").toBool());QCOMPARE(library.queueCount(),0);
    }
    void connectionEditorChecksAndPreservesPrivateSettings(){
        const auto nav=navConfig(),jelly=jellyConfig();
        QFile file(directory.filePath("media.json"));QVERIFY(file.open(QIODevice::WriteOnly));file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument(QJsonObject{{"navidrome",nav},{"jellyfin",jelly}}).toJson());file.close();
        auto bytes=[&](){QFile f(directory.filePath("media.json"));if(!f.open(QIODevice::ReadOnly))return QByteArray();return f.readAll();};
        const auto original=bytes();MediaLibrary library("navidrome");QSignalSpy saved(&library,&MediaLibrary::connectionSaved);
        const auto view=library.connectionSettings();QVERIFY(view.value("hasSecret").toBool());QVERIFY(!QJsonDocument::fromVariant(view).toJson().contains("fixture-nav-key"));
        QVariantMap draft{{"url",base.toString()},{"authentication","token"},{"secret",""},{"allowHttp",true}};
        library.checkConnection(draft);QTRY_VERIFY(!library.connectionBusy());QVERIFY(library.connectionSucceeded());QCOMPARE(bytes(),original);QCOMPARE(saved.size(),0);
        library.checkConnection(draft,true);QTRY_VERIFY(!library.connectionBusy());QVERIFY(library.connectionSucceeded());QCOMPARE(saved.size(),1);
        auto stored=QJsonDocument::fromJson(bytes()).object();QCOMPARE(stored.value("jellyfin").toObject(),jelly);QCOMPARE(stored.value("navidrome").toObject().value("apiKey").toString(),QString("fixture-nav-key"));QVERIFY(!(QFile::permissions(file.fileName())&(QFile::ReadGroup|QFile::ReadOther|QFile::WriteGroup|QFile::WriteOther)));
        int count=requests.size();draft["url"]="https://another.invalid/";library.checkConnection(draft,true);QVERIFY(!library.connectionSucceeded());QCOMPARE(requests.size(),count);
        draft["url"]=base.toString();draft["allowHttp"]=false;library.checkConnection(draft);QCOMPARE(requests.size(),count);QVERIFY(!library.connectionSucceeded());
        draft["allowHttp"]=true;failure=401;const auto beforeFailure=bytes();library.checkConnection(draft,true);QTRY_VERIFY(!library.connectionBusy());QVERIFY(!library.connectionSucceeded());QCOMPARE(bytes(),beforeFailure);QCOMPARE(saved.size(),1);
        failure=0;library.checkConnection(draft,true);library.cancelConnectionCheck();QTest::qWait(30);QVERIFY(!library.connectionBusy());QCOMPARE(saved.size(),1);QCOMPARE(bytes(),beforeFailure);
        QFile bad(file.fileName());QVERIFY(bad.open(QIODevice::WriteOnly));bad.write("not json");bad.close();count=requests.size();library.checkConnection(draft,true);QVERIFY(!library.connectionSucceeded());QCOMPARE(requests.size(),count);QCOMPARE(bytes(),QByteArray("not json"));
    }
    void jellyfinConnectionEditorUsesLoginToken(){
        config(navConfig(),"navidrome");MediaLibrary library("jellyfin");QSignalSpy saved(&library,&MediaLibrary::connectionSaved);
        QVariantMap draft{{"url",base.toString()},{"authentication","account"},{"username","fixture-user"},{"secret","fixture-private-password"},{"allowHttp",true}};
        library.checkConnection(draft,true);QTRY_VERIFY(!library.connectionBusy());QVERIFY(library.connectionSucceeded());QCOMPARE(saved.size(),1);QCOMPARE(requests.size(),2);
        QCOMPARE(requests.first().body.value("Username").toString(),QString("fixture-user"));QCOMPARE(requests.first().body.value("Pw").toString(),QString("fixture-private-password"));QVERIFY(requests.last().authorization.contains("fixture-login-token"));
        QFile file(directory.filePath("media.json"));QVERIFY(file.open(QIODevice::ReadOnly));const auto data=file.readAll();QVERIFY(!data.contains("fixture-private-password"));const auto all=QJsonDocument::fromJson(data).object();QCOMPARE(all.value("navidrome").toObject(),navConfig());QCOMPARE(all.value("jellyfin").toObject().value("userId").toString(),QString("user-1"));QVERIFY(!QJsonDocument::fromVariant(library.connectionSettings()).toJson().contains("fixture-login-token"));
        requests.clear();draft={{"url",base.toString()},{"authentication","token"},{"allowHttp",true}};library.checkConnection(draft);QTRY_VERIFY(!library.connectionBusy());QVERIFY(library.connectionSucceeded());QCOMPARE(requests.size(),1);QVERIFY(requests.first().authorization.contains("fixture-login-token"));
    }
    void connectionEditorDetectsConcurrentChangesAndDisabledModule(){
        config(navConfig(),"navidrome");MediaLibrary library("navidrome");
        QVariantMap draft{{"url",base.toString()},{"authentication","token"},{"allowHttp",true}};
        library.checkConnection(draft,true);QVERIFY(library.connectionBusy());
        auto changed=navConfig();changed.insert("apiKey","externally-updated-key");config(changed,"navidrome");
        QTRY_VERIFY(!library.connectionBusy());QVERIFY(!library.connectionSucceeded());
        QFile file(directory.filePath("media.json"));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(QJsonDocument::fromJson(file.readAll()).object().value("navidrome").toObject(),changed);file.close();
        const auto previous=qgetenv("THINKSMART_DEVICE_CONFIG");const auto restore=qScopeGuard([&](){if(previous.isNull())qunsetenv("THINKSMART_DEVICE_CONFIG");else qputenv("THINKSMART_DEVICE_CONFIG",previous);DeviceConfig::reload();});
        QFile device(directory.filePath("disabled-device.json"));QVERIFY(device.open(QIODevice::WriteOnly));device.write(R"({"version":1,"modules":{"navidrome":false}})");device.close();qputenv("THINKSMART_DEVICE_CONFIG",device.fileName().toUtf8());DeviceConfig::reload();
        const int count=requests.size();MediaLibrary disabled("navidrome");QVERIFY(!disabled.configured());disabled.checkConnection(draft,true);QVERIFY(!disabled.connectionBusy());QVERIFY(!disabled.connectionSucceeded());QCOMPARE(requests.size(),count);
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
