#include "../display.h"
#include "../vrm.h"
#include "../library.h"
#include "../jellyfinimages.h"
#include "../weather.h"
#include "../news.h"
#include "../chessgame.h"
#include "../radio.h"
#include "radio-server.h"
#include "../controller.h"
#include <QtTest>
#include <QQuickWindow>
#include <QQuickItem>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <memory>
#include <QWebSocketServer>
#include <QHostAddress>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QDataStream>
#include <QQmlPropertyMap>
#include <QMetaProperty>

class HomeUiTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QProcess helper;
    QLocalServer wifiServer;
    QList<QJsonObject> wifiRequests;
    std::unique_ptr<ScreenController> screen;
    std::unique_ptr<Controller> media;
    std::unique_ptr<VrmClient> energy;
    std::unique_ptr<WeatherClient> weather;
    std::unique_ptr<HeadlinesClient> headlines;
    std::unique_ptr<HackerNewsClient> hackerNews;
    std::unique_ptr<ChessGame> chessGame;
    std::unique_ptr<RadioCatalog> radio;
    std::unique_ptr<MediaLibrary> music,video;
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window=nullptr;
    QStringList warnings;
    void seedNews(){
        const auto path=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/news/";QDir().mkpath(path);
        auto save=[&](const QString &name,const QJsonObject &o){QFile f(path+name+".json");if(f.open(QIODevice::WriteOnly)){f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));}};
        const auto now=QDateTime::currentSecsSinceEpoch();
        QJsonArray items;
        for(int i=0;i<8;i++)items.append(QJsonObject{{"id",QString::number(i)},{"title",i%2?"Markets look ahead to the next economic outlook":"International talks focus on cooperation and trade"},{"source",i%2?"Finance desk":"World desk"},{"section",i%2?"FINANCE":"WORLD"},{"when","13 Nov · 16:30"},{"summary","A sample feed summary, presented as native text."}});
        save("headlines-WORLD_FINANCE",{{"updated",now},{"items",items}});
        save("hn-stories",{{"updated",now},{"items",QJsonArray{QJsonObject{{"id","101"},{"title","Building small, useful software"},{"domain","example.test"},{"by","sample_author"},{"points",127},{"count",2},{"kids",QJsonArray{201}}},QJsonObject{{"id","102"},{"title","A closer look at how modern compilers work"},{"domain","example.test"},{"by","another_author"},{"points",83},{"count",0},{"kids",QJsonArray{}}}}}});
        save("hn-thread-101",{{"id","101"},{"updated",now},{"roots",10},{"nodes",QJsonObject{
            {"201",QJsonObject{{"id","201"},{"by","sample_reader"},{"when","13 Nov · 16:32"},{"text","A useful discussion with a reply below."},{"kids",QJsonArray{202}}}},
            {"202",QJsonObject{{"id","202"},{"by","another_reader"},{"when","13 Nov · 16:34"},{"text","This nested reply stays inside the native reader."},{"kids",QJsonArray{}}}}
        }}});
    }
    void write(const QString &name,const QByteArray &value) {
        QFile file(directory.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));file.write(value);
    }
    QObject *object(const char *name) {
        const auto key=QString::fromUtf8(name);if(auto found=window->findChild<QObject *>(key))return found;
        // ListView delegates have a visual parent, but need not be QObject children of the window.
        std::function<QQuickItem *(QQuickItem *)> find=[&](QQuickItem *item)->QQuickItem *{if(item->objectName()==key)return item;for(auto child:item->childItems())if(auto found=find(child))return found;return nullptr;};
        return find(window->contentItem());
    }
    void click(const char *name) {
        auto item=qobject_cast<QQuickItem *>(object(name));
        QVERIFY2(item,name);QVERIFY2(item->isVisible(),name);if(!screen->sleeping())QVERIFY2(item->isEnabled(),name);
        QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,item->mapToScene(QPointF(item->width()/2,item->height()/2)).toPoint());
        QCoreApplication::processEvents();
    }
    void prepareWakeAssets(){
        QDir().mkpath(directory.filePath("wake-model"));
        for(const auto &name:{"encoder.onnx","decoder.onnx","joiner.onnx","tokens.txt"})write("wake-model/"+QString(name),"fixture");
        write("sheila.txt","fixture");write("wake-trigger","");
    }
    QByteArray action() {
        QFile file(directory.filePath("action"));if(!file.open(QIODevice::ReadOnly))return {};return file.readAll();
    }
private slots:
    void initTestCase() {
        registerVideoTypes();
        QVERIFY(directory.isValid());QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.filePath("settings"));
        QCoreApplication::setOrganizationName("ThinkSmartTests");QCoreApplication::setApplicationName("HomeUI");
        qputenv("THINKSMART_DISABLE_BLUETOOTH","1");
        qputenv("THINKSMART_DISABLE_WEATHER","1");
        qputenv("THINKSMART_DISABLE_MARKETS","1");
        qputenv("THINKSMART_DISABLE_NEWS","1");
        qputenv("THINKSMART_DEVICE_CONFIG",directory.filePath("device.json").toUtf8());
        qputenv("THINKSMART_CHESS_ENGINE",(QCoreApplication::applicationDirPath()+"/fake-chess-engine").toUtf8());
        qputenv("XDG_CACHE_HOME",directory.filePath("cache").toUtf8());
        qputenv("XDG_DATA_HOME",directory.filePath("data").toUtf8());
        qputenv("THINKSMART_PERSONAL_DIR",directory.filePath("personal").toUtf8());
        qputenv("THINKSMART_ASSISTANT_DIR",directory.filePath("assistant").toUtf8());
        qputenv("THINKSMART_WAKEWORD_AUDIO",(QCoreApplication::applicationDirPath()+"/fake-wakeword").toUtf8());
        qputenv("THINKSMART_WAKEWORD_MODEL",directory.filePath("wake-model").toUtf8());
        qputenv("THINKSMART_WAKEWORD_KEYWORDS",directory.filePath("sheila.txt").toUtf8());
        qputenv("THINKSMART_WAKE_TEST_TRIGGER",directory.filePath("wake-trigger").toUtf8());
        qputenv("THINKSMART_VOICE_CONFIG",directory.filePath("voice.json").toUtf8());
        qputenv("THINKSMART_VOICE_AUDIO",(QCoreApplication::applicationDirPath()+"/fake-voice-audio").toUtf8());
        qputenv("THINKSMART_MEDIA_CONFIG",directory.filePath("no-media.json").toUtf8());
        qputenv("THINKSMART_VRM_CONFIG",directory.filePath("not-configured.json").toUtf8());
        qputenv("THINKSMART_WIFI_SOCKET",directory.filePath("wifi.sock").toUtf8());
        QVERIFY(wifiServer.listen(directory.filePath("wifi.sock")));
        connect(&wifiServer,&QLocalServer::newConnection,this,[this](){while(wifiServer.hasPendingConnections()){
            auto client=wifiServer.nextPendingConnection();client->setParent(this);connect(client,&QLocalSocket::disconnected,client,&QObject::deleteLater);
            connect(client,&QLocalSocket::readyRead,this,[this,client](){
                const auto bytes=client->property("input").toByteArray()+client->readAll();client->setProperty("input",bytes);if(!bytes.contains('\n'))return;
                const auto request=QJsonDocument::fromJson(bytes.trimmed()).object();wifiRequests.append(request);
                QJsonObject response{{"ok",true},{"message","Saved"}};
                if(request.value("action")=="status")response={{"ok",true},{"status","Connected"},{"addresses","192.0.2.10"},{"connecting",false},
                    {"networks",QJsonArray{QJsonObject{{"id","fixture-access-point"},{"ssid","Studio Wi-Fi"},{"strength",82},{"security","wpa-psk"},{"connected",false},{"savedId",""}}}},
                    {"saved",QJsonArray{QJsonObject{{"id","fixture-profile"},{"ssid","Saved network"},{"name","Saved network"},{"autoconnect",true},{"priority",100},{"security","wpa-psk"},{"connected",true}}}}};
                client->write(QJsonDocument(response).toJson(QJsonDocument::Compact)+'\n');client->disconnectFromServer();
            });
        }});
        qputenv("THINKSMART_POWER_SOCKET",directory.filePath("control.sock").toUtf8());
        qputenv("THINKSMART_CONTROL_SOCKET",directory.filePath("home.sock").toUtf8());
        write("fake-audio",R"SH(#!/bin/sh
printf '%s\n' '{"event":"started"}' '{"event":"levels","rawDbfs":-25,"cleanDbfs":-45,"linearDbfs":-35,"elapsed":1,"clippedPercent":0,"cpuPercent":12}'
exec sleep 20
)SH");
        QFile::setPermissions(directory.filePath("fake-audio"),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        write("fake-preview",R"SH(#!/bin/sh
printf '%s\n' "$@" > "$THINKSMART_VOICE_PREVIEW_LOG"
exec sleep 20
)SH");
        QFile::setPermissions(directory.filePath("fake-preview"),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        qputenv("THINKSMART_VOICE_PREVIEW_DIR",directory.filePath("previews").toUtf8());
        qputenv("THINKSMART_VOICE_PREVIEW_LOG",directory.filePath("preview.log").toUtf8());
        write("reference.s16",QByteArray(960,'\0'));
        qputenv("THINKSMART_AUDIO_LAB",directory.filePath("fake-audio").toUtf8());
        qputenv("THINKSMART_AUDIO_REFERENCE",directory.filePath("reference.s16").toUtf8());
        write("brightness","2048\n");write("maximum","4095\n");write("action","");
        helper.start(QCoreApplication::applicationDirPath()+"/thinksmart-power",{"--test-directory",directory.path()});
        QVERIFY(helper.waitForStarted());QTRY_VERIFY(QFile::exists(directory.filePath("control.sock")));
    }
    void init() {
        QSettings settings;settings.clear();settings.sync();warnings.clear();wifiRequests.clear();write("action","");
        qputenv("THINKSMART_DASHBOARD_CONFIG",directory.filePath("dashboard.json").toUtf8());
        QFile::remove(directory.filePath("dashboard.json"));QFile::remove(directory.filePath("dashboard.json.bak"));
        qputenv("THINKSMART_WAKE_CHIME_PLAYER","/usr/bin/true");
        qputenv("THINKSMART_WAKE_TEST_ARGUMENTS",directory.filePath("wake-arguments.json").toUtf8());
        QFile::remove(directory.filePath("assistant/wakeword.json"));
        QDir(directory.filePath("personal")).removeRecursively();
        QJsonObject modules;
        if(QByteArray(QTest::currentTestFunction())=="moduleProfiles"){
            QFETCH(QStringList,enabledModules);for(const auto &name:DeviceConfig::names())modules.insert(name,enabledModules.contains(name));
        }
        if(QByteArray(QTest::currentTestFunction())=="moduleSpeechSchemas")for(const auto &id:{"radio","weather","vrm","notes","iptv"})modules.insert(id,false);
        qputenv("THINKSMART_MARKETS_FILE",directory.filePath("markets.json").toUtf8());qputenv("THINKSMART_MARKETS_CACHE",directory.filePath("markets-cache.json").toUtf8());
        QFile::remove(directory.filePath("markets.json"));QFile::remove(directory.filePath("markets-cache.json"));
        if(QByteArray(QTest::currentTestFunction()).startsWith("markets"))modules.insert("markets",true);
        const bool iptvTest=QByteArray(QTest::currentTestFunction()).startsWith("iptv") || QByteArray(QTest::currentTestFunction())=="wifiDialogKeyboardAndSavedPreferences";
        qputenv("THINKSMART_IPTV_FILE",directory.filePath("iptv-favourites.json").toUtf8());
        qputenv("THINKSMART_IPTV_CACHE",directory.filePath("iptv-cache.json").toUtf8());
        qputenv("THINKSMART_RADIO_FAVOURITES_FILE",directory.filePath("radio-favourites.json").toUtf8());QFile::remove(directory.filePath("radio-favourites.json"));
        QFile::remove(directory.filePath("iptv-favourites.json"));QFile::remove(directory.filePath("iptv-cache.json"));
        if(iptvTest){
            modules.insert("iptv",true);
            const QJsonArray rows{QJsonObject{{"id","fixture-tv"},{"name","World News"},{"country","GB"},{"categories",QJsonArray{"news"}},{"quality","720p"},{"labels",QJsonArray{}},{"origin","custom"},{"url","http://127.0.0.1:1/private-test.m3u8?token=fixture-secret"}}};
            write("iptv-cache.json",QJsonDocument(QJsonObject{{"version",1},{"updated",double(QDateTime::currentSecsSinceEpoch())},{"channels",rows}}).toJson());
            write("iptv-favourites.json",QJsonDocument(QJsonObject{{"version",1},{"favourites",rows}}).toJson());
        }
        write("device.json",QJsonDocument(QJsonObject{{"version",1},{"modules",modules}}).toJson());DeviceConfig::reload();
        if(QByteArray(QTest::currentTestFunction())=="iptvBrowserSavesCustomStreams"){
            auto config=DashboardConfig::defaults(DeviceConfig::modules());auto pages=config.value("pages").toArray();
            pages.append(QJsonObject{{"id","tv"},{"title","TV"},{"kind","widgets"},{"widgets",QJsonArray{"iptv"}}});config.insert("pages",pages);
            QString error;QVERIFY(DashboardConfig::write(config,error));DeviceConfig::reload();
        }
        if(QByteArray(QTest::currentTestFunction()).startsWith("markets")){
            auto config=DashboardConfig::defaults(DeviceConfig::modules());auto pages=config.value("pages").toArray();pages.append(QJsonObject{{"id","markets-room"},{"title","Markets"},{"kind","widgets"},{"widgets",QJsonArray{"markets"}}});config.insert("pages",pages);QString error;QVERIFY(DashboardConfig::write(config,error));DeviceConfig::reload();
            QJsonObject quotes,charts;const auto now=QDateTime::currentSecsSinceEpoch();
            for(const auto &symbol:{"^GSPC","DX-Y.NYB","NVDA","BTC-USD","ETH-USD"}){
                const QString name=symbol;const double value=name=="NVDA"?155.0:name=="BTC-USD"?64000.0:name=="ETH-USD"?1800.0:name=="DX-Y.NYB"?103.0:7340.0;
                quotes.insert(name,QJsonObject{{"symbol",name},{"label",name=="^GSPC"?"S&P 500":name=="DX-Y.NYB"?"DXY":name},{"name",name=="NVDA"?"NVIDIA Corporation":name=="^GSPC"?"S&P 500":name},{"price",value},{"percent",1.23},{"currency","USD"},{"quoteTime",now},{"fetchedAt",now},{"instrumentType","CRYPTOCURRENCY"},{"changePeriod","day"}});
                QJsonArray points;for(int i=0;i<40;i++)points.append(QJsonObject{{"time",now-(40-i)*300},{"value",value*(0.98+i*0.0005+std::sin(i*0.6)*0.002)}});
                for(const auto &range:{"1D","1W","1M","1Y"})charts.insert(name+":"+range,QJsonObject{{"points",points},{"fetchedAt",now}});
            }
            write("markets-cache.json",QJsonDocument(QJsonObject{{"version",1},{"quotes",quotes},{"charts",charts}}).toJson());
        }
        if(QByteArray(QTest::currentTestFunction())=="configuredPagesRenderAndRouteModules"){
            const QJsonArray pages{QJsonObject{{"id","home"},{"title","Home"},{"kind","widgets"},{"widgets",QJsonArray{"radio"}}},QJsonObject{{"id","room"},{"title","Room"},{"kind","widgets"},{"widgets",QJsonArray{"assistant","weather"}}},QJsonObject{{"id","games"},{"title","Games"},{"kind","module"},{"module","chess"}},QJsonObject{{"id","games2"},{"title","Second board"},{"kind","module"},{"module","chess"}}};
            QString error;QVERIFY(DashboardConfig::write({{"version",1},{"modules",QJsonObject{}},{"pages",pages}},error));DeviceConfig::reload();
        }
        qputenv("THINKSMART_VOICE_PREVIEW_PLAYER",directory.filePath("fake-preview").toUtf8());write("preview.log","");
        QDir().mkpath(directory.filePath("previews"));
        QByteArray preview;QDataStream wav(&preview,QIODevice::WriteOnly);wav.setByteOrder(QDataStream::LittleEndian);
        wav.writeRawData("RIFF",4);wav<<quint32(36+4800);wav.writeRawData("WAVEfmt ",8);wav<<quint32(16)<<quint16(1)<<quint16(1)<<quint32(24000)<<quint32(48000)<<quint16(2)<<quint16(16);wav.writeRawData("data",4);wav<<quint32(4800);preview.append(QByteArray(4800,'\0'));
        for(const auto &name:{"cedar.wav","marin.wav","willow.wav","echo.wav","shimmer.wav"}){
            write("previews/"+QString(name),preview);
        }
        QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/radio-browser").removeRecursively();
        seedNews();
        QDir(directory.filePath("wake-model")).removeRecursively();QFile::remove(directory.filePath("sheila.txt"));
        write("voice.json",R"({"provider":"openai","apiKey":"test-local-only","model":"gpt-realtime-2.1-mini","voice":"marin"})");
        QFile::setPermissions(directory.filePath("voice.json"),QFile::ReadOwner|QFile::WriteOwner);
        QDir().mkpath(directory.filePath("assistant/personalities"));
        for(const auto &file:QDir(directory.filePath("assistant/personalities")).entryList(QDir::Files))QFile::remove(directory.filePath("assistant/personalities/"+file));
        write("assistant/system.md","# Common system\nUse local tools for device data.\n");
        write("assistant/household.md","# Household\nTest person prefers jazz.\n");
        QFile livePolicy(QFINDTESTDATA("../deploy/assistant/live.md"));QVERIFY(livePolicy.open(QIODevice::ReadOnly));write("assistant/live.md",livePolicy.readAll());
        QFile backendPolicy(QFINDTESTDATA("../deploy/assistant/backend.md"));QVERIFY(backendPolicy.open(QIODevice::ReadOnly));write("assistant/backend.md",backendPolicy.readAll());
        write("assistant/personalities/warm.md","# Warm companion\nBe warm and considerate.\n");
        write("assistant/personalities/brief.md","# Brief and practical\nBe very brief.\n");
        screen=std::make_unique<ScreenController>();media=std::make_unique<Controller>(screen.get());
        energy=std::make_unique<VrmClient>();
        weather=std::make_unique<WeatherClient>();radio=std::make_unique<RadioCatalog>();
        headlines=std::make_unique<HeadlinesClient>();hackerNews=std::make_unique<HackerNewsClient>();
        chessGame=std::make_unique<ChessGame>();chessGame->newGame();if(!chessGame->playerWhite())chessGame->switchSides();chessGame->setRating(1200);
        media->setDataSources(weather.get(),radio.get(),energy.get());
        music=std::make_unique<MediaLibrary>("navidrome");video=std::make_unique<MediaLibrary>("jellyfin");media->setVideoLibrary(video.get());
        engine=std::make_unique<QQmlApplicationEngine>();
        engine->addImageProvider("jellyfin",new JellyfinImages(video->imageSettings()));
        connect(engine.get(),&QQmlApplicationEngine::warnings,this,[this](const QList<QQmlError> &errors){for(const auto &error:errors)warnings<<error.toString();});
        engine->rootContext()->setContextProperty("home",media.get());
        engine->rootContext()->setContextProperty("weather",weather.get());
        engine->rootContext()->setContextProperty("headlines",headlines.get());
        engine->rootContext()->setContextProperty("hackerNews",hackerNews.get());
        engine->rootContext()->setContextProperty("chessGame",chessGame.get());
        engine->rootContext()->setContextProperty("radio",radio.get());
        engine->rootContext()->setContextProperty("phoneAudio",media->phone());
        engine->rootContext()->setContextProperty("audioLab",media->audioLab());
        engine->rootContext()->setContextProperty("assistant",media->assistant());
        engine->rootContext()->setContextProperty("musicLibrary",music.get());
        engine->rootContext()->setContextProperty("videoLibrary",video.get());
        engine->rootContext()->setContextProperty("energy",energy.get());
        engine->rootContext()->setContextProperty("screenControl",screen.get());
        engine->load(QUrl("qrc:/qml/Main.qml"));QVERIFY(!engine->rootObjects().isEmpty());
        window=qobject_cast<QQuickWindow *>(engine->rootObjects().first());QVERIFY(window);
        window->setVisibility(QWindow::Windowed);window->resize(800,1280);window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window));
    }
    void wakeTapDoesNotActivateHiddenControlAndScreenOffWorks() {
        QVERIFY(screen->sleeping());
        click("radioPlayButton");QVERIFY(!screen->sleeping());QCOMPARE(media->state(),QString("stopped"));
        click("screenOffButton");QVERIFY(screen->sleeping());
        click("screenOffButton");QVERIFY(!screen->sleeping());
        click("screenOffButton");QVERIFY(screen->sleeping());
    }
    void layoutEditorDraftAndApply(){
        screen->wake();click("powerButton");click("modulesSettingsButton");QCOMPARE(window->property("selectedApp").toString(),QString("layout-settings"));
        auto editor=media->layoutEditor();QCOMPARE(editor->pages().size(),4);QVERIFY(!editor->dirty());
        click("layoutAddPage");QCOMPARE(editor->pages().size(),5);click("layoutEditPage4");
        auto name=object("layoutPageName");QVERIFY(name);name->setProperty("text","My page");QVERIFY(QMetaObject::invokeMethod(name,"editingFinished"));QCOMPARE(editor->pages()[4].toMap().value("title").toString(),QString("My page"));
        click("layoutAddWidget");QCOMPARE(editor->pages()[4].toMap().value("widgets").toList().size(),1);QVERIFY(editor->modules().value("assistant").toBool());
        click("layoutCancel");QVERIFY(!editor->dirty());QCOMPARE(editor->pages().size(),4);QVERIFY(!QFile::exists(DashboardConfig::path()));
        media->editLayout();click("layoutModulesTab");auto sw=object("moduleSwitch_radio");QVERIFY(sw);QVERIFY(QMetaObject::invokeMethod(sw,"toggle")); // QML user handler is tested through setModule below.
        editor->setModule("radio",false);QSignalSpy restart(media.get(),&Controller::dashboardRestartRequested);click("layoutApply");QCOMPARE(restart.size(),1);QVERIFY(QFile::exists(DashboardConfig::path()));QVERIFY(DeviceConfig::enabled("radio")); // Until restart.
        DeviceConfig::reload();QVERIFY(!DeviceConfig::enabled("radio"));QVERIFY(!media->toolRequest({{"action","radio.play"},{"id","radio4"}}).value("ok").toBool());
    }
    void moduleSpeechSchemas_data(){QTest::addColumn<bool>("live");QTest::newRow("realtime")<<false;QTest::newRow("live")<<true;}
    void moduleSpeechSchemas(){
        QFETCH(bool,live);qputenv("THINKSMART_SHARED_AUDIO","1");auto restore=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QJsonObject session;QPointer<QWebSocket> peer;auto cleanupPeer=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &text){const auto e=QJsonDocument::fromJson(text.toUtf8()).object();if(e.value("type")=="session.update"||e.value("type")=="session.start")session=e.value("session").toObject();});if(!live)peer->sendTextMessage("{\"type\":\"session.created\"}");});
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->setModel(live?"gpt-live-1":"gpt-realtime-2.1-mini");voice->setEnabled(true);voice->start();QTRY_VERIFY(!session.isEmpty());
        const auto owner=live?session.value("delegation").toObject().value("responses").toObject():session;
        const auto instructions=session.value("instructions").toString()+owner.value("instructions").toString();for(const auto &marker:{"Radio module","Weather module","Vrm module","Notes module","IPTV module","Markets module","markets.get","iptv.search","notes.add","radio.search"})QVERIFY(!instructions.contains(marker));
        bool device=false,personal=false;
        for(const auto &v:owner.value("tools").toArray()){
            const auto f=v.toObject();const auto props=f.value("parameters").toObject().value("properties").toObject();
            if(f.value("name")=="device_action"||f.value("name")=="personal_action"){
                for(const auto &a:props.value("action").toObject().value("enum").toArray())for(const auto &prefix:{"radio.","weather.","battery.","notes.","iptv.","markets."})QVERIFY(!a.toString().startsWith(prefix));
                if(f.value("name")=="device_action")device=true;
                else{personal=true;for(const auto &a:props.value("data").toObject().value("properties").toObject().value("steps").toObject().value("items").toObject().value("properties").toObject().value("action").toObject().value("enum").toArray())QVERIFY(!a.toString().startsWith("radio."));}
            }
        }
        QVERIFY(device&&personal);voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void marketsWidgetChartWatchlistAndAlerts(){
        screen->wake();window->resize(1280,800);QTest::qWait(80);click("pageButton_markets-room");QTRY_COMPARE(object("dashboardPages")->property("currentIndex").toInt(),4);QTRY_VERIFY(object("marketsTile"));click("marketsTile");QTRY_COMPARE(window->property("selectedApp").toString(),QString("markets"));
        QCOMPARE(media->markets()->quotes().size(),5);QVERIFY(media->markets()->selected().value("available").toBool());
        click("marketQuote2");QTRY_COMPARE(media->markets()->focus(),QString("NVDA"));click("marketsRange1W");QCOMPARE(media->markets()->range(),QString("1W"));click("marketsMode");QCOMPARE(media->markets()->mode(),QString("focus"));
        click("marketsAddAlert");QTRY_VERIFY(object("marketsAlertDialog")->property("opened").toBool());object("marketsAlertPrice")->setProperty("text","200");click("marketsSaveAlert");QCOMPARE(media->markets()->alerts().size(),1);QTRY_VERIFY(!object("marketsAlertDialog")->property("visible").toBool());
        click("marketsAlerts");QTRY_VERIFY(object("marketsAlertsDialog")->property("opened").toBool());QTest::qWait(50);click("marketsRemoveAlert0");QTRY_COMPARE(media->markets()->alerts().size(),0);QVERIFY(QMetaObject::invokeMethod(object("marketsAlertsDialog"),"close"));QTRY_VERIFY(!object("marketsAlertsDialog")->property("visible").toBool());
        click("marketsEdit");QTRY_VERIFY(object("marketsWatchlistDialog")->property("opened").toBool());object("marketsSymbols")->setProperty("text","NVDA, BTC, DXY");click("marketsSaveSymbols");QCOMPARE(media->markets()->symbols(),(QStringList{"NVDA","BTC-USD","DX-Y.NYB"}));
        const auto destination=qEnvironmentVariable("THINKSMART_CAPTURE_DIR");
        for(const auto &size:{QSize(1280,800),QSize(800,1280)}){window->resize(size);QTest::qWait(100);auto chart=qobject_cast<QQuickItem *>(object("marketsChart"));QVERIFY(chart);QVERIFY(chart->width()>200);QVERIFY(chart->height()>90);if(!destination.isEmpty()){QDir().mkpath(destination);QVERIFY(window->grabWindow().save(destination+QString("/markets-%1.png").arg(size.width())));}}
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join("\n")));click("marketsBack");QCOMPARE(window->property("selectedApp").toString(),QString());QCOMPARE(object("dashboardPages")->property("currentIndex").toInt(),4);
    }
    void marketsKeyboard(){
        if(!media->virtualKeyboardEnabled())QSKIP("Run separately with Qt Virtual Keyboard");
        screen->wake();media->openAppRequested("markets");
        for(const auto &size:{QSize(1280,800),QSize(800,1280)}){
            window->resize(size);QTest::qWait(80);click("marketsEdit");QTRY_VERIFY(object("marketsWatchlistDialog")->property("opened").toBool());click("marketsSymbols");QTRY_VERIFY(qobject_cast<QQuickItem *>(object("virtualKeyboard"))->isVisible());click("virtualEnterKey");QTRY_VERIFY(!qobject_cast<QQuickItem *>(object("virtualKeyboard"))->isVisible());click("marketsSaveSymbols");QTRY_VERIFY(!object("marketsWatchlistDialog")->property("visible").toBool());
            click("marketsAddAlert");QTRY_VERIFY(object("marketsAlertDialog")->property("opened").toBool());click("marketsAlertPrice");QTRY_VERIFY(qobject_cast<QQuickItem *>(object("virtualKeyboard"))->isVisible());click("virtualEnterKey");QTRY_VERIFY(!qobject_cast<QQuickItem *>(object("virtualKeyboard"))->isVisible());click("marketsSaveAlert");QTRY_VERIFY(!object("marketsAlertDialog")->property("visible").toBool());
        }
        QCOMPARE(media->markets()->alerts().size(),2);
    }
    void marketsVoiceTools_data(){QTest::addColumn<bool>("live");QTest::newRow("realtime")<<false;QTest::newRow("live")<<true;}
    void marketsVoiceTools(){
        QFETCH(bool,live);qputenv("THINKSMART_SHARED_AUDIO","1");auto restore=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QJsonObject session;QList<QJsonObject> messages;QPointer<QWebSocket> peer;auto cleanupPeer=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &text){const auto e=QJsonDocument::fromJson(text.toUtf8()).object();messages.append(e);if(e.value("type")=="session.update"||e.value("type")=="session.start"){session=e.value("session").toObject();peer->sendTextMessage(live?"{\"type\":\"session.started\"}":"{\"type\":\"session.updated\"}");}});if(!live)peer->sendTextMessage("{\"type\":\"session.created\"}");});
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->setModel(live?"gpt-live-1":"gpt-realtime-2.1-mini");voice->setEnabled(true);voice->start();QTRY_VERIFY(!session.isEmpty());
        const auto owner=live?session.value("delegation").toObject().value("responses").toObject():session;QVERIFY(owner.value("instructions").toString().contains("Markets module"));QVERIFY(QJsonDocument(owner.value("tools").toArray()).toJson().contains("markets.alert.add"));
        const QJsonObject item{{"type","function_call"},{"name","device_action"},{"call_id","market-call"},{"arguments","{\"action\":\"markets.alert.add\",\"symbol\":\"NVDA\",\"direction\":\"above\",\"price\":200}"}};
        auto send=[&](QJsonObject e){peer->sendTextMessage(QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact)));};
        if(live){auto nested=[&](QJsonObject e){send({{"type","response.event"},{"delegation_id","d1"},{"event",e}});};nested({{"type","response.created"},{"response",QJsonObject{{"id","r-market"}}}});nested({{"type","response.output_item.done"},{"response_id","r-market"},{"item",item}});nested({{"type","response.completed"},{"response",QJsonObject{{"id","r-market"}}}});}
        else {send({{"type","response.created"},{"response",QJsonObject{{"id","r-market"}}}});send({{"type","response.done"},{"response",QJsonObject{{"id","r-market"},{"status","completed"},{"output",QJsonArray{item}}}}});}
        QTRY_COMPARE(media->markets()->alerts().size(),1);QTRY_VERIFY(std::any_of(messages.cbegin(),messages.cend(),[](auto r){return QJsonDocument(r).toJson().contains("market-call")&&QJsonDocument(r).toJson().contains("alerts");}));voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void iptvBrowserSavesCustomStreams(){
        screen->wake();
        auto musicTile=qobject_cast<QQuickItem *>(object("musicTile"));QVERIFY(musicTile);const auto musicSize=musicTile->size();
        media->openAppRequested("tv");QTRY_COMPARE(object("dashboardPages")->property("currentIndex").toInt(),4);QTRY_VERIFY(!object("dashboardPages")->property("transitioning").toBool());
        auto tvTile=qobject_cast<QQuickItem *>(object("iptvTile"));QVERIFY(tvTile);QVERIFY(std::abs(tvTile->width()-musicSize.width())<2);QVERIFY(std::abs(tvTile->height()-musicSize.height())<2);
        QVERIFY(!media->iptv()->active());click("iptvTile");QCOMPARE(window->property("selectedApp").toString(),QString("iptv"));
        QTRY_VERIFY(media->iptv()->active());QVERIFY(!media->iptv()->busy());QCOMPARE(media->iptv()->favourites().size(),1);
        auto page=object("iptvAppPage");QVERIFY(page);QCOMPARE(page->property("tab").toString(),QString("saved"));
        auto list=object("iptvChannels");QVERIFY(list);QTRY_COMPARE(list->property("count").toInt(),1);
        QTRY_VERIFY(object("iptvSave0"));click("iptvSave0");QTRY_COMPARE(list->property("count").toInt(),0);QCOMPARE(media->iptv()->favourites().size(),0);
        QTest::qWait(30);QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join("\n")));
        QVERIFY(media->iptv()->favourite("fixture-tv",true));QTRY_COMPARE(list->property("count").toInt(),1);
        page->setProperty("tab","directory");QCOMPARE(list->property("count").toInt(),1);
        click("iptvSave0");QTest::qWait(30);QCOMPARE(media->iptv()->favourites().size(),0);
        click("iptvSave0");QTest::qWait(30);QCOMPARE(media->iptv()->favourites().size(),1);

        object("iptvSearch")->setProperty("text","unmatched");QTRY_COMPARE(list->property("count").toInt(),0);
        object("iptvSearch")->setProperty("text","");QTRY_COMPARE(list->property("count").toInt(),1);
        click("iptvOptionsButton");click("iptvAddStream");QTRY_VERIFY(object("iptvCustomDialog")->property("visible").toBool());
        object("iptvCustomName")->setProperty("text","My channel");object("iptvCustomUrl")->setProperty("text","https://example.test/live.m3u8?token=private-fixture");click("iptvCustomSave");
        QTRY_VERIFY(!object("iptvCustomDialog")->property("visible").toBool());QCOMPARE(media->iptv()->favourites().size(),2);QCOMPARE(page->property("tab").toString(),QString("saved"));
        const auto json=QJsonDocument(media->toolRequest({{"action","iptv.list"}})).toJson();QVERIFY(!json.contains("private-fixture"));QVERIFY(!json.contains("fixture-secret"));
        const auto destination=qEnvironmentVariable("THINKSMART_CAPTURE_DIR");
        if(!destination.isEmpty()){QDir().mkpath(destination);for(const auto &size:{QSize(1280,800),QSize(800,1280)}){window->resize(size);QTest::qWait(100);QVERIFY(window->grabWindow().save(destination+QString("/iptv-%1.png").arg(size.width())));}}
        click("iptvBackButton");QCOMPARE(window->property("selectedApp").toString(),QString());QCOMPARE(object("dashboardPages")->property("currentIndex").toInt(),4);
        window->resize(1280,800);QTest::qWait(50);QVERIFY(tvTile->width()<window->width()/2);QVERIFY(tvTile->height()<window->height()/2);
        QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join("\n")));
    }
    void radioDirectorySearchFavouritesAndReturn(){
        RadioServer server;server.rows={RadioServer::row("Worldwide jazz","https://radio.example.test/jazz")};qputenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS",server.url().toUtf8());auto reset=qScopeGuard([](){qunsetenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS");});
        screen->wake();window->setProperty("selectedApp","radio");auto page=object("radioAppPage");QVERIFY(page);QCOMPARE(page->property("category").toString(),QString("favourites"));
        auto list=object("radioResults");QVERIFY(list);QCOMPARE(list->property("count").toInt(),3);QCOMPARE(server.searches(),0);
        object("radioSearch")->setProperty("text","Jazz");QTRY_VERIFY(server.searches()>0);QTRY_VERIFY(!radio->searchBusy());QTRY_COMPARE(list->property("count").toInt(),1);QTRY_VERIFY(object("radioFavourite0"));click("radioFavourite0");QCOMPARE(radio->favouriteCount(),4);
        click("radioCategory_favourites");QCOMPARE(list->property("count").toInt(),4);QCOMPARE(server.searches(),1);
        QCOMPARE(media->toolRequest({{"action","radio.favourites"}}).value("stations").toArray().size(),4);
        const auto capture=qEnvironmentVariable("THINKSMART_CAPTURE_DIR");
        if(!capture.isEmpty()){QDir().mkpath(capture);for(const auto &size:{QSize(1280,800),QSize(800,1280)}){window->resize(size);QTest::qWait(80);QVERIFY(window->grabWindow().save(capture+QString("/radio-%1.png").arg(size.width())));}}
        click("radioBackButton");window->setProperty("selectedApp","radio");QCOMPARE(page->property("category").toString(),QString("favourites"));
        window->resize(1280,800);QTest::qWait(50);QVERIFY(qobject_cast<QQuickItem *>(list)->height()>300);QVERIFY2(warnings.isEmpty(),qPrintable(warnings.join("\n")));
    }
    void iptvVoiceToolsAndFullscreen_data(){QTest::addColumn<bool>("live");QTest::newRow("realtime")<<false;QTest::newRow("live")<<true;}
    void iptvVoiceToolsAndFullscreen(){
        QFETCH(bool,live);qputenv("THINKSMART_SHARED_AUDIO","1");auto restore=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QJsonObject session;QList<QJsonObject> messages;QPointer<QWebSocket> peer;auto cleanupPeer=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &text){const auto e=QJsonDocument::fromJson(text.toUtf8()).object();messages.append(e);if(e.value("type")=="session.update"||e.value("type")=="session.start"){session=e.value("session").toObject();peer->sendTextMessage(live?"{\"type\":\"session.started\"}":"{\"type\":\"session.updated\"}");}});if(!live)peer->sendTextMessage("{\"type\":\"session.created\"}");});
        prepareWakeAssets();screen->wake();window->setProperty("selectedApp","assistant");auto voice=media->assistant();voice->setModel(live?"gpt-live-1":"gpt-realtime-2.1-mini");voice->setEnabled(true);voice->start();QTRY_VERIFY(!session.isEmpty());
        const auto owner=live?session.value("delegation").toObject().value("responses").toObject():session;
        QVERIFY(owner.value("instructions").toString().contains("IPTV module"));QVERIFY(QJsonDocument(owner.value("tools").toArray()).toJson().contains("iptv.play"));
        const QJsonObject item{{"type","function_call"},{"name","device_action"},{"call_id","tv-call"},{"arguments","{\"action\":\"iptv.play\",\"id\":\"fixture-tv\"}"}};
        auto send=[&](QJsonObject e){peer->sendTextMessage(QString::fromUtf8(QJsonDocument(e).toJson(QJsonDocument::Compact)));};
        if(live){
            auto nested=[&](QJsonObject e){send({{"type","response.event"},{"delegation_id","d1"},{"event",e}});};
            nested({{"type","response.created"},{"response",QJsonObject{{"id","r-tv"}}}});
            nested({{"type","response.output_item.done"},{"response_id","r-tv"},{"item",item}});
            nested({{"type","response.completed"},{"response",QJsonObject{{"id","r-tv"}}}});
        }else{
            send({{"type","response.created"},{"response",QJsonObject{{"id","r-tv"}}}});
            send({{"type","response.done"},{"response",QJsonObject{{"id","r-tv"},{"status","completed"},{"output",QJsonArray{item}}}}});
        }
        QTRY_COMPARE(media->source(),QString("iptv"));QTRY_VERIFY(media->video()->visible());QTRY_COMPARE(window->property("selectedApp").toString(),QString("iptv"));QVERIFY(voice->active());
        QTRY_VERIFY(std::any_of(messages.cbegin(),messages.cend(),[](auto r){return QJsonDocument(r).toJson().contains("tv-call")&&QJsonDocument(r).toJson().contains("accepted");}));
        media->stop();QVERIFY(!media->video()->visible());QVERIFY(voice->active());voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void configuredPagesRenderAndRouteModules(){
        screen->wake();media->assistant()->setEnabled(true);auto pages=object("dashboardPages");QVERIFY(pages);QCOMPARE(pages->property("count").toInt(),4);QCOMPARE(pages->property("currentIndex").toInt(),0);QVERIFY(!object("moreAppsPageButton"));QVERIFY(!media->systemStats()->active());
        click("pageButton_room");QTRY_COMPARE(pages->property("currentIndex").toInt(),1);QTRY_VERIFY(!pages->property("transitioning").toBool());QTRY_VERIFY(object("dashboardAssistantVisual")->property("moving").toBool());QVERIFY(media->systemStats()->active());
        media->openAppRequested("chess");QTRY_COMPARE(pages->property("currentIndex").toInt(),2);QTRY_VERIFY(!pages->property("transitioning").toBool());QVERIFY(!object("dashboardAssistantVisual")->property("moving").toBool());QVERIFY(chessGame->active());
        window->resize(1280,800);QTest::qWait(50);click("homePageButton");QTRY_COMPARE(pages->property("currentIndex").toInt(),0);
    }
    void moduleProfiles_data(){
        QTest::addColumn<QStringList>("enabledModules");
        QTest::newRow("core-only")<<QStringList{};
        QTest::newRow("radio-only")<<QStringList{"radio"};
        QTest::newRow("without-news")<<QStringList{"radio","assistant","chess","weather"};
        QTest::newRow("without-chess")<<QStringList{"radio","assistant","news","weather"};
        QTest::newRow("all")<<DeviceConfig::names();
    }
    void moduleProfiles(){
        QFETCH(QStringList,enabledModules);screen->wake();
        auto pages=object("dashboardPages");QVERIFY(pages);
        QCOMPARE(pages->property("count").toInt(),1+int(enabledModules.contains("news"))+int(enabledModules.contains("chess"))+int(enabledModules.contains("notes")));
        for(const auto &name:DeviceConfig::names())QCOMPARE(media->modules().value(name).toBool(),enabledModules.contains(name));
        const auto actions=media->toolRequest({{"action","system.capabilities"}}).value("actions").toArray();
        for(const auto &action:actions)QVERIFY(DeviceConfig::actionEnabled(action.toObject().value("action").toString()));
        if(!enabledModules.contains("radio"))QVERIFY(!media->toolRequest({{"action","radio.play"},{"id","radio4"}}).value("ok").toBool());
        if(!enabledModules.contains("weather"))QVERIFY(!media->toolRequest({{"action","weather.refresh"}}).value("ok").toBool());
        if(!enabledModules.contains("assistant")){media->assistant()->setEnabled(true);QVERIFY(!media->assistant()->enabled());QVERIFY(!media->assistant()->configured());}
        QCOMPARE(object("moreAppsPageButton")!=nullptr,enabledModules.contains("news"));
        QCOMPARE(object("chessPageButton")!=nullptr,enabledModules.contains("chess"));
        QCOMPARE(object("weatherTile")!=nullptr,enabledModules.contains("weather"));
    }
    void deviceConfigurationChangesLocationAndRejectsInvalidFlags(){
        write("device.json",R"({"weather":{"name":"Example city","latitude":1.2,"longitude":-3.4,"timezone":"UTC"}})");DeviceConfig::reload();
        auto query=QUrlQuery(WeatherClient::forecastUrl());QCOMPARE(query.queryItemValue("latitude"),QString("1.200000"));QCOMPARE(query.queryItemValue("timezone"),QString("UTC"));
        QCOMPARE(weather->location(),QString("Example city"));QVERIFY(DeviceConfig::problem().isEmpty());
        write("device.json",R"({"modules":{"assistant":"false"}})");DeviceConfig::reload();QVERIFY(!DeviceConfig::problem().isEmpty());
        for(const auto &name:DeviceConfig::names())QVERIFY(!DeviceConfig::enabled(name));
        write("device.json","{}");DeviceConfig::reload();
    }
    void dashboardPagesButtonsSwipesAndAppReturn() {
        constexpr int homePage=3,morePage=2,notesPage=1;
        screen->wake();window->resize(1280,800);QTest::qWait(50);
        auto pages=qobject_cast<QQuickItem *>(object("dashboardPages"));QVERIFY(pages);
        auto flickable=pages;
        QCOMPARE(pages->property("currentIndex").toInt(),homePage);
        click("moreAppsPageButton");QTRY_COMPARE(pages->property("currentIndex").toInt(),morePage);
        QCOMPARE(window->property("selectedApp").toString(),QString());
        click("homePageButton");QTRY_COMPARE(pages->property("currentIndex").toInt(),homePage);
        QTest::qWait(400);
        auto *touch=QTest::createTouchDevice();
        auto swipe=[&](bool right){
            const auto from=pages->mapToScene(QPointF(pages->width()*(right?.2:.8),pages->height()*.6)).toPoint();
            const auto to=pages->mapToScene(QPointF(pages->width()*(right?.8:.2),pages->height()*.6)).toPoint();
            auto sequence=QTest::touchEvent(window,touch);
            sequence.press(0,from,window).commit();QTest::qWait(20);
            for(int i=1;i<=12;i++){sequence.move(0,from+(to-from)*i/12,window).commit();QTest::qWait(20);}
            sequence.release(0,to,window).commit();
        };
        swipe(true);QTRY_COMPARE(pages->property("currentIndex").toInt(),morePage);
        QCOMPARE(window->property("selectedApp").toString(),QString());
        QTest::qWait(400);swipe(false);QTRY_COMPARE(pages->property("currentIndex").toInt(),homePage);
        QTRY_VERIFY(!pages->property("transitioning").toBool());
        // Finger left wraps Home -> Chess; finger right wraps Chess -> Home.
        swipe(false);QTRY_COMPARE(pages->property("currentIndex").toInt(),0);
        QTRY_VERIFY(!pages->property("transitioning").toBool());
        swipe(true);QTRY_COMPARE(pages->property("currentIndex").toInt(),homePage);
        QTRY_VERIFY(!pages->property("transitioning").toBool());
        // Complete the ring in both directions without jumping or duplicating pages.
        for(int expected:{0,notesPage,morePage,homePage}){swipe(false);QTRY_COMPARE(pages->property("currentIndex").toInt(),expected);QTRY_VERIFY(!pages->property("transitioning").toBool());}
        for(int expected:{morePage,notesPage,0,homePage}){swipe(true);QTRY_COMPARE(pages->property("currentIndex").toInt(),expected);QTRY_VERIFY(!pages->property("transitioning").toBool());}
        QVERIFY(hackerNews->selected().isEmpty());
        QTRY_VERIFY(!flickable->property("moving").toBool());
        click("radioTile");QCOMPARE(window->property("selectedApp").toString(),QString("radio"));
        click("moreAppsPageButton");QTRY_COMPARE(pages->property("currentIndex").toInt(),morePage);
        QCOMPARE(window->property("selectedApp").toString(),QString());
        window->resize(800,1280);QTest::qWait(50);
        auto selector=qobject_cast<QQuickItem *>(object("dashboardPageSelector"));QVERIFY(selector);
        QVERIFY(qAbs(selector->mapToScene(QPointF(selector->width()/2,0)).x()-window->width()/2.)<1);
        click("homePageButton");QTRY_VERIFY(!pages->property("transitioning").toBool());
        swipe(false);QTRY_COMPARE(pages->property("currentIndex").toInt(),0);QTRY_VERIFY(!pages->property("transitioning").toBool());
        swipe(true);QTRY_COMPARE(pages->property("currentIndex").toInt(),homePage);QTRY_VERIFY(!pages->property("transitioning").toBool());
    }
    void dashboardOrbPausesDuringNavigationAndWhileHidden(){
        screen->wake();media->assistant()->setEnabled(true);window->resize(1280,800);
        auto orb=object("dashboardAssistantVisual"),pages=object("dashboardPages");QVERIFY(orb);QVERIFY(pages);
        QTRY_VERIFY(orb->property("moving").toBool());
        click("moreAppsPageButton");QTRY_VERIFY(pages->property("transitioning").toBool());
        QVERIFY(!orb->property("moving").toBool());const auto paused=orb->property("phase").toDouble();
        QTest::qWait(80);QCOMPARE(orb->property("phase").toDouble(),paused);
        QTRY_VERIFY(!pages->property("transitioning").toBool());QVERIFY(!orb->property("moving").toBool());
        QTest::qWait(100);QCOMPARE(orb->property("phase").toDouble(),paused);
        click("homePageButton");QTRY_VERIFY(pages->property("transitioning").toBool());QVERIFY(!orb->property("moving").toBool());
        const auto returning=orb->property("phase").toDouble();QTest::qWait(80);QCOMPARE(orb->property("phase").toDouble(),returning);
        QTRY_VERIFY(!pages->property("transitioning").toBool());QTRY_VERIFY(orb->property("phase").toDouble()>returning);
        window->setProperty("selectedApp","weather");QVERIFY(!orb->property("moving").toBool());
        const auto hidden=orb->property("phase").toDouble();QTest::qWait(100);QCOMPARE(orb->property("phase").toDouble(),hidden);
        window->setProperty("selectedApp","");QTRY_VERIFY(orb->property("moving").toBool());
        screen->sleep();QVERIFY(!orb->property("moving").toBool());
    }
    void newsPanelsReaderAndReplies(){
        screen->wake();window->resize(1280,800);media->openAppRequested("news");QTest::qWait(400);
        auto left=qobject_cast<QQuickItem *>(object("headlinesPanel"));auto right=qobject_cast<QQuickItem *>(object("hackerNewsPanel"));QVERIFY(left);QVERIFY(right);
        QVERIFY(qAbs(left->y()-right->y())<1);QVERIFY(right->x()>left->x()+left->width());
        // A vertical news scroll must not turn the horizontal page ring.
        auto list=qobject_cast<QQuickItem *>(object("headlineList"));QVERIFY(list);
        auto *touch=QTest::createTouchDevice();auto sequence=QTest::touchEvent(window,touch);
        const auto from=list->mapToScene(QPointF(list->width()*.5,list->height()*.8)).toPoint();
        const auto to=list->mapToScene(QPointF(list->width()*.5,list->height()*.2)).toPoint();
        sequence.press(0,from,window).commit();QTest::qWait(20);
        for(int i=1;i<=10;i++){sequence.move(0,from+(to-from)*i/10,window).commit();QTest::qWait(20);}
        sequence.release(0,to,window).commit();
        QCOMPARE(object("dashboardPages")->property("currentIndex").toInt(),media->dashboardPages().indexOf("news"));QTRY_VERIFY(list->property("contentY").toDouble()>0);
        QTRY_VERIFY(!list->property("moving").toBool());list->setProperty("contentY",0);QCoreApplication::processEvents();
        click("headlineRow0");QCOMPARE(object("newsPage")->property("readingArticle").toBool(),true);
        click("headlinesBackButton");QCOMPARE(object("newsPage")->property("readingArticle").toBool(),false);
        click("hnStoryRow0");QCOMPARE(hackerNews->selected().value("id").toString(),QString("101"));QCOMPARE(hackerNews->comments()->rowCount(),1);
        click("hnReplies201");QCOMPARE(hackerNews->comments()->rowCount(),2);
        click("hnBackButton");QVERIFY(hackerNews->selected().isEmpty());
        window->resize(800,1280);QTest::qWait(350);QVERIFY(qAbs(left->x()-right->x())<1);QVERIFY(right->y()>left->y()+left->height());
        media->openAppRequested("home");QTRY_COMPARE(object("dashboardPages")->property("currentIndex").toInt(),3);
    }
    void chessBoardMovesUndoAndTheme(){
        screen->wake();window->resize(1280,800);click("chessPageButton");QTest::qWait(400);
        QCOMPARE(object("dashboardPages")->property("currentIndex").toInt(),0);QVERIFY(chessGame->active());
        click("chessSquare_e2");click("chessSquare_e4");QTRY_COMPARE(chessGame->moveCount(),2);QVERIFY(chessGame->error().isEmpty());
        click("chessUndoButton");QCOMPARE(chessGame->moveCount(),0);
        media->setTheme("dawn");QCoreApplication::processEvents();media->setTheme("tide");QCoreApplication::processEvents();
        window->resize(800,1280);QTest::qWait(350);auto square=qobject_cast<QQuickItem *>(object("chessSquare_e2"));QVERIFY(square);QVERIFY(square->width()>60);
        click("homePageButton");QVERIFY(!chessGame->active());QTRY_COMPARE(object("dashboardPages")->property("currentIndex").toInt(),3);
    }
    void chessSideSwitchRatingAndBlackBoardInput(){
        screen->wake();window->resize(1280,800);click("chessPageButton");QTest::qWait(400);
        click("chessSwitchSidesButton");QTRY_COMPARE(chessGame->moveCount(),1);QVERIFY(!chessGame->playerWhite());QVERIFY(chessGame->error().isEmpty());
        auto blackKing=qobject_cast<QQuickItem *>(object("chessSquare_e8"));auto whiteKing=qobject_cast<QQuickItem *>(object("chessSquare_e1"));QVERIFY(blackKing);QVERIFY(whiteKing);QVERIFY(blackKing->y()>whiteKing->y());
        click("chessRatingUpButton");QCOMPARE(chessGame->rating(),1300);click("chessRatingDownButton");QCOMPARE(chessGame->rating(),1200);
        chessGame->setRating(2400);QVERIFY(!object("chessRatingUpButton")->property("enabled").toBool());click("chessRatingDownButton");QCOMPARE(chessGame->rating(),2300);click("chessRatingUpButton");QCOMPARE(chessGame->rating(),2400);chessGame->setRating(1200);
        click("chessSquare_e7");click("chessSquare_e5");QTRY_COMPARE(chessGame->moveCount(),3);QVERIFY(chessGame->error().isEmpty());click("chessUndoButton");QCOMPARE(chessGame->moveCount(),1);
        window->resize(800,1280);QTest::qWait(350);auto button=qobject_cast<QQuickItem *>(object("chessSwitchSidesButton"));auto page=qobject_cast<QQuickItem *>(object("chessPage"));QVERIFY(button);QVERIFY(page);QVERIFY(button->mapToScene(QPointF(0,button->height())).y()<page->mapToScene(QPointF(0,page->height())).y());
    }
    void menuSettingsAndTimerSave() {
        screen->wake();click("powerButton");QTRY_VERIFY(object("powerSheet")->property("visible").toBool());
        click("displaySettingsButton");QTRY_VERIFY(object("settingsSheet")->property("visible").toBool());
        QVERIFY(object("dimChoice")->setProperty("currentIndex",3));
        QVERIFY(object("offChoice")->setProperty("currentIndex",6));
        click("settingsSaveButton");QTRY_VERIFY(!object("settingsSheet")->property("visible").toBool());
        QCOMPARE(screen->dimSeconds(),120);QCOMPARE(screen->sleepSeconds(),600);
    }
    void wifiDialogKeyboardAndSavedPreferences(){
        if(!media->virtualKeyboardEnabled())QSKIP("Run the isolated Wi-Fi keyboard test to avoid Qt input-context state crossing QML engines.");
        screen->wake();window->resize(800,1280);click("powerButton");click("wifiSettingsButton");
        QTRY_VERIFY(object("wifiDialog")->property("visible").toBool());QTRY_COMPARE(media->wifi()->networks().size(),1);
        QTRY_VERIFY(!media->wifi()->busy());click("wifiSelect0");
        auto password=qobject_cast<QQuickItem *>(object("wifiPasswordInput"));QVERIFY(password);QVERIFY(password->isVisible());
        click("wifiPasswordInput");QTRY_VERIFY(object("virtualKeyboard")->property("visible").toBool());
        QTRY_VERIFY(object("virtualKey_a"));click("virtualKey_a");QTRY_COMPARE(password->property("text").toString(),QString("a"));
        click("virtualShiftKey");QTRY_VERIFY(object("virtualKey_A"));click("virtualKey_A");QTRY_COMPARE(password->property("text").toString(),QString("aA"));
        click("virtualBackspaceKey");QTRY_COMPARE(password->property("text").toString(),QString("a"));
        click("virtualSymbolKey");QTRY_VERIFY(object("virtualKey_1"));click("virtualKey_1");QTRY_COMPARE(password->property("text").toString(),QString("a1"));
        const auto preview=qEnvironmentVariable("THINKSMART_WIFI_PREVIEW_DIR");
        if(!preview.isEmpty()){QVERIFY(QDir().mkpath(preview));QVERIFY(window->grabWindow().save(preview+"/wifi-keyboard-portrait.png"));}
        QVERIFY(password->property("inputMethodHints").toInt()&Qt::ImhNoPredictiveText);QCOMPARE(password->property("echoMode").toInt(),2);
        window->resize(1280,800);QTest::qWait(150);
        auto keyboard=qobject_cast<QQuickItem *>(object("virtualKeyboard"));QVERIFY(keyboard);
        QVERIFY(password->mapToScene(QPointF(0,password->height())).y()<=keyboard->mapToScene(QPointF(0,0)).y());
        if(!preview.isEmpty())QVERIFY(window->grabWindow().save(preview+"/wifi-keyboard-landscape.png"));
        click("wifiRevealPassword");QCOMPARE(password->property("echoMode").toInt(),0);
        password->setProperty("text","fixture-password");QTRY_VERIFY(!media->wifi()->busy());click("wifiConnectButton");
        QTRY_VERIFY(std::any_of(wifiRequests.cbegin(),wifiRequests.cend(),[](auto r){return r.value("action")=="connect"&&r.value("password")=="fixture-password";}));
        QTRY_VERIFY(password->property("text").toString().isEmpty());QTRY_VERIFY(!keyboard->isVisible());
        QTRY_COMPARE(object("wifiDialog")->property("view").toString(),QString("available"));QTRY_VERIFY(!media->wifi()->busy());
        click("wifiSavedTab");QTRY_VERIFY(!media->wifi()->busy());click("wifiSelect0");
        object("wifiPriority")->setProperty("value",10);object("wifiAutoconnect")->setProperty("checked",false);
        click("wifiSavePreferences");QTRY_VERIFY(std::any_of(wifiRequests.cbegin(),wifiRequests.cend(),[](auto r){return r.value("action")=="preferences"&&r.value("priority")==10&&!r.value("autoconnect").toBool();}));
        QTRY_VERIFY(!media->wifi()->busy());click("wifiHiddenButton");QTRY_VERIFY(object("wifiSsidInput")->property("activeFocus").toBool());
        QTRY_VERIFY(keyboard->isVisible());object("wifiSsidInput")->setProperty("text","Hidden fixture");
        object("wifiSecurityChoice")->setProperty("currentIndex",1);QTRY_VERIFY(!media->wifi()->busy());click("wifiConnectButton");
        QTRY_VERIFY(std::any_of(wifiRequests.cbegin(),wifiRequests.cend(),[](auto r){return r.value("action")=="connect"&&r.value("hidden").toBool()&&r.value("ssid")=="Hidden fixture"&&r.value("security")=="open";}));
        QTRY_VERIFY(!media->wifi()->busy());
        click("wifiCloseButton");QVERIFY(!media->wifi()->active());
        const auto count=wifiRequests.size();QTest::qWait(200);QCOMPARE(wifiRequests.size(),count);
        media->editLayout();media->layoutEditor()->addPage();object("layoutSettingsPage")->setProperty("selected",4);
        click("layoutPageName");QTRY_VERIFY(keyboard->isVisible());
        auto pageName=qobject_cast<QQuickItem *>(object("layoutPageName"));QVERIFY(pageName);
        QVERIFY(pageName->mapToScene(QPointF(0,pageName->height())).y()<=keyboard->mapToScene(QPointF(0,0)).y());
        const int oldLength=pageName->property("text").toString().size();
        if(!object("virtualKey_a"))click("virtualSymbolKey");QTRY_VERIFY(object("virtualKey_a"));click("virtualKey_a");QTRY_COMPARE(pageName->property("text").toString().size(),oldLength+1);
        click("layoutCancel");QTRY_VERIFY(!keyboard->isVisible());QVERIFY(!QFile::exists(DashboardConfig::path()));
        media->openAppRequested("iptv");
        for(const auto &size:{QSize(1280,800),QSize(800,1280)}){
            window->resize(size);QTest::qWait(80);click("iptvSearch");QTRY_VERIFY(keyboard->isVisible());
            object("iptvSearch")->setProperty("text","World News");QTRY_COMPARE(object("iptvChannels")->property("count").toInt(),1);
            click("virtualEnterKey");QTRY_VERIFY(!keyboard->isVisible());QVERIFY(!object("iptvSearch")->property("activeFocus").toBool());
            auto channels=qobject_cast<QQuickItem *>(object("iptvChannels"));QVERIFY(channels);QTRY_VERIFY(channels->height()>window->height()*0.65);
        }

    }
    void shutdownRequiresConfirmationAndUsesHalt() {
        screen->wake();click("powerButton");QTRY_VERIFY(object("powerSheet")->property("visible").toBool());
        click("shutdownMenuButton");QTRY_VERIFY(object("confirmSheet")->property("visible").toBool());
        QVERIFY(action().isEmpty());click("confirmCancelButton");QVERIFY(!screen->powerPending());QVERIFY(action().isEmpty());
        click("powerButton");QTRY_VERIFY(object("powerSheet")->property("visible").toBool());
        click("shutdownMenuButton");QTRY_VERIFY(object("confirmSheet")->property("visible").toBool());
        click("confirmPowerButton");QVERIFY(screen->powerPending());QTRY_COMPARE(action(),QByteArray("halt\n"));
    }
    void jsonToolsExposeCatalogAndStampedData() {
        auto stations=media->toolRequest({{"action","radio.list"},{"genre","folk"}});
        QVERIFY(stations.value("ok").toBool());QVERIFY(stations.value("stations").toArray().size()>=2);
        for(const auto &value:stations.value("stations").toArray()){
            const auto row=value.toObject();QVERIFY(!row.contains("url"));QVERIFY(row.value("genres").toArray().contains("folk"));
        }
        auto invalid=media->toolRequest({{"action","radio.play"},{"id","https://arbitrary.invalid/stream"}});
        QVERIFY(!invalid.value("ok").toBool());QCOMPARE(media->state(),QString("stopped"));
        auto weatherResult=media->toolRequest({{"action","weather.get"},{"requestId","forecast-1"}});
        QVERIFY(weatherResult.value("ok").toBool());QCOMPARE(weatherResult.value("requestId").toString(),QString("forecast-1"));
        QVERIFY(weatherResult.value("weather").toObject().value("stale").toBool());
        auto battery=media->toolRequest({{"action","battery.get"}}).value("battery").toObject();
        QVERIFY(!battery.value("available").toBool());
        const auto soc=battery.value("metrics").toObject().value("soc").toObject();
        QVERIFY(!soc.value("available").toBool());QVERIFY(soc.value("value").isNull());QCOMPARE(soc.value("unit").toString(),QString("%"));
        QVERIFY(!media->toolRequest({{"action","media.volume"},{"percent",101}}).value("ok").toBool());
        window->setProperty("selectedApp","radio");QCoreApplication::processEvents();
        window->setProperty("selectedApp","weather");QCoreApplication::processEvents();
    }
    void audioCaptureStopsOnPageExitAndScreenSleep(){
        screen->wake();window->setProperty("selectedApp","assistant");QTest::qWait(70);
        click("assistantSettingsButton");QTest::qWait(30);click("assistantAudioTestButton");QTest::qWait(30);click("audioLabStartButton");QTRY_VERIFY2(!media->audioLab()->levels().isEmpty(),qPrintable(media->audioLab()->status()+" "+media->audioLab()->problem()));
        QCOMPARE(media->audioLab()->levels().value("rawDbfs").toInt(),-25);
        QVERIFY(media->audioLab()->running());QCOMPARE(media->state(),QString("stopped"));
        click("assistantBackButton");QTRY_VERIFY(!media->audioLab()->running());
        QFile report(media->audioLab()->reportPath());QVERIFY(report.open(QIODevice::ReadOnly));
        const auto saved=QJsonDocument::fromJson(report.readAll()).object();
        QCOMPARE(saved.value("levels").toArray().size(),1);
        QCOMPARE(saved.value("summary").toObject().value("meanClippedPercent").toDouble(),0.0);
        QVERIFY(!saved.contains("audio"));QVERIFY(!(report.permissions()&QFile::ReadOther));
        window->setProperty("selectedApp","assistant");QTest::qWait(70);
        click("assistantSettingsButton");QTest::qWait(30);click("assistantAudioTestButton");QTest::qWait(30);click("audioLabStartButton");QTRY_VERIFY(!media->audioLab()->levels().isEmpty());
        screen->sleep();QTRY_VERIFY(!media->audioLab()->running());
    }
    void assistantSettingsSaveEnableAndVoice(){
        screen->wake();window->setProperty("selectedApp","assistant");QTest::qWait(50);
        click("assistantSettingsButton");QTest::qWait(30);
        QVERIFY(!media->assistant()->enabled());click("assistantEnableSwitch");QVERIFY(media->assistant()->enabled());
        auto choice=object("assistantVoiceChoice");QVERIFY(choice);choice->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,1)));
        QCOMPARE(media->assistant()->voice(),QString("cedar"));QCOMPARE(QSettings().value("assistant/voices/gpt-realtime-2.1-mini").toString(),QString("cedar"));
        auto model=object("assistantModelChoice");QVERIFY(model);
        QCOMPARE(media->assistant()->model(),QString("gpt-realtime-2.1-mini"));
        model->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(model,"activated",Q_ARG(int,1)));
        QCOMPARE(media->assistant()->model(),QString("gpt-realtime-2.1"));QCOMPARE(QSettings().value("assistant/model").toString(),QString("gpt-realtime-2.1"));
        media->assistant()->reloadConfiguration();QCOMPARE(media->assistant()->model(),QString("gpt-realtime-2.1"));QCOMPARE(model->property("currentIndex").toInt(),1);
        media->assistant()->setModel("not-a-configured-model");QCOMPARE(media->assistant()->model(),QString("gpt-realtime-2.1"));
        auto profile=object("assistantPersonalityChoice");QVERIFY(profile);profile->setProperty("currentIndex",0);QVERIFY(QMetaObject::invokeMethod(profile,"activated",Q_ARG(int,0)));
        QCOMPARE(media->assistant()->personality(),QString("brief"));QCOMPARE(QSettings().value("assistant/personality").toString(),QString("brief"));
        QCOMPARE(QSettings().value("assistant/enabled").toBool(),true);QVERIFY(!media->assistant()->active());
        click("assistantEnableSwitch");QVERIFY(!media->assistant()->enabled());QTRY_VERIFY(media->assistant()->released());
    }
    void voiceChoicesPersistPerModelAndMigrate(){
        auto voice=media->assistant();QSettings settings;
        settings.setValue("assistant/voice","shimmer");voice->reloadConfiguration();
        QCOMPARE(voice->voice(),QString("shimmer"));QVERIFY(!settings.contains("assistant/voice"));
        QCOMPARE(settings.value("assistant/voices/gpt-realtime-2.1-mini").toString(),QString("shimmer"));
        voice->setModel("gpt-realtime-2.1");QCOMPARE(voice->voice(),QString("marin"));voice->setVoice("cedar");
        voice->setModel("gpt-live-1");QCOMPARE(voice->voice(),QString("marin"));QVERIFY(!voice->voices().contains("shida"));voice->setVoice("willow");
        voice->setModel("gpt-realtime-2.1-mini");QCOMPARE(voice->voice(),QString("shimmer"));
        voice->setModel("gpt-realtime-2.1");QCOMPARE(voice->voice(),QString("cedar"));
        voice->setModel("gpt-live-1");voice->reloadConfiguration();QCOMPARE(voice->voice(),QString("willow"));
        {VoiceAssistant restored(media.get(),screen.get());QCOMPARE(restored.model(),QString("gpt-live-1"));QCOMPARE(restored.voice(),QString("willow"));}
        // A stale global choice cannot overwrite an existing model preference.
        settings.setValue("assistant/voice","ash");voice->reloadConfiguration();QCOMPARE(voice->voice(),QString("willow"));QVERIFY(!settings.contains("assistant/voice"));
        // Live-only and unrelated TTS voices must not leak into Realtime choices.
        voice->setVoice("fable");QCOMPARE(voice->voice(),QString("willow"));
        settings.setValue("assistant/voices/gpt-realtime-2.1-mini","willow");
        voice->setModel("gpt-realtime-2.1-mini");QCOMPARE(voice->voice(),QString("marin"));
        voice->setModel("gpt-live-1");QCOMPARE(voice->voice(),QString("willow"));
    }
    void voicePreviewsAreLocalReplaceAndReleaseAudio(){
        qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});auto voice=media->assistant();
        auto log=[&](){QFile file(directory.filePath("preview.log"));return file.open(QIODevice::ReadOnly)?file.readAll():QByteArray();};
        screen->wake();window->setProperty("selectedApp","assistant");QTest::qWait(40);click("assistantSettingsButton");
        QVERIFY(!voice->enabled());QVERIFY(voice->previewAvailable());
        auto choice=object("assistantVoiceChoice");QVERIFY(choice);
        choice->setProperty("currentIndex",1);QVERIFY(QMetaObject::invokeMethod(choice,"activated",Q_ARG(int,1)));
        QTRY_VERIFY(voice->previewPlaying()&&log().contains("cedar.wav"));
        QVERIFY(!voice->active());QVERIFY(!voice->snapshot().value("microphoneOpen").toBool());
        QVERIFY(voice->snapshot().value("mediaDucked").toBool());QVERIFY(log().contains("--audio-device=alsa/thinksmart_voice"));
        // A rapid change replaces the pending sample; only the latest is played.
        voice->setModel("gpt-live-1");voice->setVoice("willow");voice->previewVoice();voice->previewVoice();
        QTRY_VERIFY(voice->previewPlaying()&&log().contains("willow.wav"));
        click("assistantVoicePreviewButton");QTRY_VERIFY(!voice->previewPlaying());QVERIFY(!voice->snapshot().value("mediaDucked").toBool());
        voice->previewVoice();QTRY_VERIFY(voice->previewPlaying());click("assistantSettingsButton");QTRY_VERIFY(!voice->previewPlaying());
        voice->previewVoice();QTRY_VERIFY(voice->previewPlaying());screen->sleep();QTRY_VERIFY(!voice->previewPlaying());
        screen->wake();voice->setVoice("echo");QVERIFY(voice->previewAvailable());voice->setVoice("shimmer");QVERIFY(voice->previewAvailable());
        voice->setVoice("ash");QVERIFY(!voice->previewAvailable());voice->previewVoice();
        QVERIFY(!voice->previewPlaying());QVERIFY(voice->previewProblem().contains("No saved preview"));
        voice->setVoice("willow");qputenv("THINKSMART_VOICE_PREVIEW_PLAYER",directory.filePath("missing-player").toUtf8());
        voice->previewVoice();QTRY_VERIFY(!voice->previewPlaying()&&!voice->previewProblem().isEmpty());
        QVERIFY(!voice->snapshot().value("mediaDucked").toBool());QVERIFY(voice->problem().isEmpty());
        qputenv("THINKSMART_VOICE_PREVIEW_PLAYER","/usr/bin/true");voice->previewVoice();
        QTRY_VERIFY(!voice->previewPlaying());QVERIFY(voice->previewProblem().isEmpty());
        QVERIFY(!voice->snapshot().value("mediaDucked").toBool());QVERIFY(!voice->active());
    }
    void wakeSensitivityAndActivationSound(){
        qputenv("THINKSMART_SHARED_AUDIO","1");
        auto restore=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        write("assistant/wakeword.json",R"({"word":"Sheila","threshold":0.97,"score":2.0,"trailingBlanks":2,"extra":"preserved"})");
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->reloadConfiguration();
        QCOMPARE(voice->wakeThreshold(),.97);QVERIFY(!voice->wakeThresholdOverridden());
        QCOMPARE(voice->wakeSensitivity(),50.);
        for(double sensitivity:{0.,25.,50.,75.,100.}){
            voice->setWakeSensitivity(sensitivity);QVERIFY(std::abs(voice->wakeSensitivity()-sensitivity)<.000001);
            QVERIFY(voice->wakeThreshold()>=.94&&voice->wakeThreshold()<=.995);
        }
        voice->setWakeSensitivity(0);QCOMPARE(voice->wakeThreshold(),.995);
        voice->setWakeSensitivity(100);QCOMPARE(voice->wakeThreshold(),.94);
        voice->setWakeSensitivity(50);QCOMPARE(voice->wakeThreshold(),.97);
        voice->setWakeSensitivity(-1);voice->setWakeSensitivity(101);QCOMPARE(voice->wakeSensitivity(),50.);
        voice->resetWakeThreshold();QVERIFY(!voice->wakeThresholdOverridden());
        voice->setWakeThreshold(.985);QCOMPARE(voice->wakeThreshold(),.985);QVERIFY(voice->wakeThresholdOverridden());
        voice->setWakeThreshold(std::nan(""));voice->setWakeThreshold(1);QCOMPARE(voice->wakeThreshold(),.985);
        {WakeWord restored;QCOMPARE(restored.threshold(),.985);}
        voice->setEnabled(true);QTRY_VERIFY(voice->wakeListening());
        auto arguments=[&](){QFile f(directory.filePath("wake-arguments.json"));if(!f.open(QIODevice::ReadOnly))return QStringList{};QStringList result;for(const auto &v:QJsonDocument::fromJson(f.readAll()).array())result<<v.toString();return result;};
        QTRY_VERIFY(arguments().contains("0.985"));
        voice->setWakeThreshold(.98);QTRY_VERIFY(arguments().contains("0.98"));QTRY_VERIFY(voice->wakeListening());
        QFile file(directory.filePath("assistant/wakeword.json"));QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("preserved"));file.close();
        voice->resetWakeThreshold();QCOMPARE(voice->wakeThreshold(),.97);QVERIFY(!voice->wakeThresholdOverridden());
        QTRY_VERIFY(arguments().contains("0.97"));QTRY_VERIFY(voice->wakeListening());
        window->setProperty("selectedApp","assistant");click("assistantSettingsButton");
        QVERIFY(object("wakeThresholdSlider"));QCOMPARE(object("wakeThresholdSlider")->property("value").toDouble(),50.);
        voice->setWakeSoundEnabled(false);QVERIFY(!QSettings().value("assistant/wakeSoundEnabled").toBool());
        voice->setWakeSoundEnabled(true);
        write("fake-chime","#!/bin/sh\nprintf '%s\\n' \"$@\" > \"$THINKSMART_WAKE_CHIME_LOG\"\ncat > \"$THINKSMART_WAKE_CHIME_PCM\"\nsleep 0.1\n");
        QFile::setPermissions(directory.filePath("fake-chime"),QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner);
        qputenv("THINKSMART_WAKE_CHIME_PLAYER",directory.filePath("fake-chime").toUtf8());
        qputenv("THINKSMART_WAKE_CHIME_LOG",directory.filePath("chime.log").toUtf8());qputenv("THINKSMART_WAKE_CHIME_PCM",directory.filePath("chime.pcm").toUtf8());
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        int connections=0;connect(&server,&QWebSocketServer::newConnection,this,[&](){connections++;auto peer=server.nextPendingConnection();connect(peer,&QWebSocket::disconnected,peer,&QObject::deleteLater);peer->sendTextMessage("{\"type\":\"session.created\"}");});
        auto chime=[&](){QFile f(directory.filePath("chime.pcm"));return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();};
        // Merely enabling, changing settings or starting manually makes no tone.
        QCOMPARE(connections,0);QVERIFY(chime().isEmpty());
        write("wake-trigger","Sheila");QTRY_VERIFY(voice->active());QTRY_COMPARE(chime().size(),17280);QTRY_COMPARE(connections,1);
        QFile log(directory.filePath("chime.log"));QVERIFY(log.open(QIODevice::ReadOnly));QVERIFY(log.readAll().contains("thinksmart_voice"));
        const auto pcm=chime();QVERIFY(pcm!=QByteArray(pcm.size(),'\0'));QCOMPARE(pcm.left(2),QByteArray(2,'\0'));QCOMPARE(pcm.right(2),QByteArray(2,'\0'));
        voice->setWakeThreshold(.99);QCOMPARE(voice->wakeThreshold(),.97); // Cannot restart capture mid-conversation.
        voice->stop();QFile::remove(directory.filePath("chime.pcm"));voice->setWakeSoundEnabled(false);
        write("wake-trigger","Sheila");QTRY_VERIFY(voice->active());QTRY_COMPARE(connections,2);QVERIFY(chime().isEmpty());
        voice->stop();voice->start();QTRY_COMPARE(connections,3);QVERIFY(chime().isEmpty());voice->stop();
        voice->setEnabled(false);QTRY_VERIFY(voice->released());voice->setWakeThreshold(.99);QTest::qWait(350);QVERIFY(voice->released());QCOMPARE(connections,3);
    }
    void wakeWordMasterSwitchAndSleepingScreenHandoff(){
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;int connections=0;
        auto cleanupPeer=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){
            connections++;peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
                if(QJsonDocument::fromJson(message.toUtf8()).object().value("type")=="session.update")peer->sendTextMessage("{\"type\":\"session.updated\"}");
            });peer->sendTextMessage("{\"type\":\"session.created\"}");
        });
        prepareWakeAssets();auto voice=media->assistant();QVERIFY(screen->sleeping());
        QTest::qWait(300);QVERIFY(!voice->wakeListening());QVERIFY(voice->released());QCOMPARE(connections,0);
        voice->setEnabled(true);QTRY_VERIFY(voice->wakeListening());QVERIFY(screen->sleeping());QCOMPARE(connections,0);
        QCOMPARE(voice->status(),QString("Listening locally for Sheila"));QVERIFY(!voice->wakePreparing());
        // A queued/spoken keyword after disable cannot wake the screen or open the API.
        write("wake-trigger","Sheila");voice->setEnabled(false);QTRY_VERIFY(voice->released());QTest::qWait(80);
        QVERIFY(screen->sleeping());QVERIFY(!voice->active());QCOMPARE(connections,0);
        write("wake-trigger","");voice->setEnabled(true);QTRY_VERIFY(voice->wakeListening());
        write("wake-trigger","Sheila");QTRY_VERIFY(voice->active());QTRY_COMPARE(connections,1);
        QTRY_VERIFY(!voice->released());QVERIFY(!voice->wakeListening());QVERIFY(!screen->sleeping());QCOMPARE(window->property("selectedApp").toString(),QString("assistant"));
        voice->setEnabled(false);QTRY_VERIFY(voice->released());QVERIFY(!voice->wakeListening());QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);
    }
    void sharedWakeSurvivesMediaAndConversations(){
        qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QList<QWebSocket *> peers;int connections=0;
        auto cleanup=qScopeGuard([&](){for(auto p:peers){p->disconnect(this);delete p;}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){
            connections++;auto peer=server.nextPendingConnection();peers.append(peer);
            connect(peer,&QWebSocket::textMessageReceived,this,[peer](const QString &message){if(QJsonDocument::fromJson(message.toUtf8()).object().value("type")=="session.update")peer->sendTextMessage("{\"type\":\"session.updated\"}");});
            peer->sendTextMessage("{\"type\":\"session.created\"}");
        });
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->setEnabled(true);QTRY_VERIFY(voice->wakeListening());
        media->useBluetooth();QTRY_VERIFY(media->wakeWordAllowed());QVERIFY(voice->wakeListening());
        media->stop();QVERIFY(voice->wakeListening());
        for(int i=1;i<=2;i++){
            write("wake-trigger","Sheila");QTRY_VERIFY(voice->active());QTRY_COMPARE(connections,i);QVERIFY(voice->wakeListening());
            QTRY_VERIFY(voice->inputLevel()>0);voice->stop();QVERIFY(voice->wakeListening());QTRY_VERIFY(!voice->active());
            QTest::qWait(60);
        }
        voice->setEnabled(false);QTRY_VERIFY(voice->released());QVERIFY(!voice->wakeListening());
    }
    void sharedVoiceStopToolsReachPlayer(){
        qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;
        auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
            auto event=QJsonDocument::fromJson(message.toUtf8()).object();requests.append(event);
            if(event.value("type")=="session.update")peer->sendTextMessage("{\"type\":\"session.updated\"}");
        });peer->sendTextMessage("{\"type\":\"session.created\"}");});
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->setEnabled(true);voice->start();QTRY_VERIFY(peer&&voice->wakeListening());QTRY_VERIFY(voice->inputLevel()>0);
        auto session=requests.first().value("session").toObject();auto tools=session.value("tools").toArray();
        QVERIFY(std::any_of(tools.cbegin(),tools.cend(),[](auto t){return t.toObject().value("name")=="stop_media";}));
        for(const auto &name:{QString("device_action"),QString("stop_media")}){
            media->playRadio("radio4",QUrl("http://127.0.0.1:9/test"),"Test station","");QVERIFY(media->state()!="stopped");
            const QJsonObject args=name=="stop_media"?QJsonObject{}:QJsonObject{{"action","media.stop"}};
            peer->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"type","response.done"},{"response",QJsonObject{{"id",name},{"status","completed"},{"output",QJsonArray{QJsonObject{{"type","function_call"},{"call_id",name},{"name",name},{"arguments",QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}}}}}}}).toJson(QJsonDocument::Compact)));
            QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[&](auto r){auto item=r.value("item").toObject();auto result=QJsonDocument::fromJson(item.value("output").toString().toUtf8()).object();return item.value("call_id")==name&&result.value("ok").toBool()&&result.value("media").toObject().value("state")=="stopped";}));
            QCOMPARE(media->state(),QString("stopped"));QVERIFY(voice->active());QVERIFY(voice->wakeListening());
            auto diagnostic=voice->snapshot().value("lastTool").toObject();QCOMPARE(diagnostic.value("action").toString(),QString("media.stop"));QVERIFY(diagnostic.value("ok").toBool());QVERIFY(!diagnostic.contains("arguments"));
        }
        voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void sharedMediaReturnsAfterReplyBeforeSessionTimeout(){
        qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject event){peer->sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){if(QJsonDocument::fromJson(message.toUtf8()).object().value("type")=="session.update")send({{"type","session.updated"}});});send({{"type","session.created"}});});
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->setEnabled(true);voice->start();QTRY_VERIFY(peer&&voice->inputLevel()>0);
        send({{"type","response.created"},{"response",QJsonObject{{"id","reply"}}}});
        QTRY_VERIFY(voice->snapshot().value("mediaDucked").toBool());
        send({{"type","response.output_audio.delta"},{"response_id","reply"},{"item_id","reply-audio"},{"delta",QString::fromLatin1(QByteArray(960,'\0').toBase64())}});
        QTRY_VERIFY(voice->outputLevel()>0);
        send({{"type","response.done"},{"response",QJsonObject{{"id","reply"},{"status","completed"},{"output",QJsonArray{}}}}});
        QVERIFY(voice->snapshot().value("mediaDucked").toBool()); // Still playing the reply.
        QTRY_VERIFY_WITH_TIMEOUT(!voice->snapshot().value("mediaDucked").toBool(),2000);
        QVERIFY(voice->active());QVERIFY(voice->wakeListening()); // Follow-up window stays open.
        send({{"type","input_audio_buffer.speech_started"}});QTRY_VERIFY(voice->snapshot().value("mediaDucked").toBool());
        QTest::qWait(500);QVERIFY(voice->snapshot().value("mediaDucked").toBool()); // User speech cancels restoration.
        voice->stop();QVERIFY(!voice->snapshot().value("mediaDucked").toBool());QVERIFY(voice->wakeListening());
        voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void wakeWordYieldsToMediaAndAudioTest(){
        prepareWakeAssets();screen->wake();auto voice=media->assistant();voice->setEnabled(true);QTRY_VERIFY(voice->wakeListening());
        media->useBluetooth();QTRY_VERIFY(!voice->wakeListening());QVERIFY(!media->wakeWordAllowed());
        QCOMPARE(voice->wakeIndicator(),QString("Sheila · paused for media"));QVERIFY(voice->status().contains("paused during media"));
        media->stop();QTRY_VERIFY(voice->wakeListening());
        media->audioLab()->start(false);QTRY_VERIFY(media->audioLab()->levels().contains("rawDbfs"));QVERIFY(!voice->wakeListening());
        media->audioLab()->stop();QTRY_VERIFY(voice->wakeListening());
        voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void assistantPromptsReloadAndValidateEdits(){
        auto voice=media->assistant();QVERIFY(voice->promptsReady());QCOMPARE(voice->personality(),QString("warm"));
        write("assistant/personalities/custom.md","# My custom personality\nBe a curious guide.\n");
        voice->reloadConfiguration();QCOMPARE(voice->personalities().size(),3);
        voice->setPersonality("custom");QVERIFY(voice->promptsReady());
        write("assistant/household.md","# Household\nNew shared preference from SSH.\n");
        write("assistant/personalities/custom.md","# Renamed guide\nGive thoughtful explanations.\n");
        voice->reloadConfiguration();QCOMPARE(voice->personality(),QString("custom"));
        AssistantPrompts compiled;compiled.reload("custom");QVERIFY(compiled.ready());
        QVERIFY(compiled.instructions().contains("New shared preference from SSH."));QVERIFY(compiled.instructions().contains("Give thoughtful explanations."));
        QVERIFY(!compiled.instructions().contains("Be warm and considerate."));
        QVERIFY(!QJsonDocument(voice->snapshot()).toJson().contains("New shared preference"));
        const auto choices=compiled.personalities();QVERIFY(std::any_of(choices.cbegin(),choices.cend(),[](const QVariant &v){return v.toMap().value("id")=="custom"&&v.toMap().value("name")=="custom.md";}));
        QFile::remove(directory.filePath("assistant/personalities/custom.md"));voice->reloadConfiguration();
        QVERIFY(!voice->promptsReady());QVERIFY(!voice->promptProblem().isEmpty());
        voice->setPersonality("brief");QVERIFY(voice->promptsReady());
        write("assistant/personalities/brief.md",QByteArray(32769,'x'));voice->reloadConfiguration();QVERIFY(!voice->promptsReady());
        write("assistant/personalities/brief.md","# Brief\nBe concise.\n");write("assistant/household.md",QByteArray(1,char(0xff)));voice->reloadConfiguration();QVERIFY(!voice->promptsReady());
        write("assistant/household.md","# Household\n");voice->reloadConfiguration();QVERIFY(voice->promptsReady());
        voice->setPersonality("../system");QCOMPARE(voice->personality(),QString("brief"));
    }
    void livePoliciesReloadValidateAndStaySeparate(){
        auto voice=media->assistant();voice->setModel("gpt-live-1");QVERIFY(voice->promptsReady());
        write("assistant/live.md","Custom Live policy edited over SSH.\n");
        write("assistant/backend.md","Custom backend procedure edited over SSH.\n");
        voice->reloadConfiguration();QVERIFY(voice->promptsReady());
        AssistantPrompts compiled;compiled.reload("warm",true);QVERIFY(compiled.ready());
        QVERIFY(compiled.liveInstructions().contains("Custom Live policy"));
        QVERIFY(compiled.liveInstructions().contains("Be warm and considerate."));
        QVERIFY(compiled.liveInstructions().contains("Test person prefers jazz."));
        QVERIFY(!compiled.liveInstructions().contains("Custom backend procedure"));
        QVERIFY(compiled.backendInstructions().contains("Custom backend procedure"));
        QVERIFY(compiled.backendInstructions().contains("Test person prefers jazz."));
        QVERIFY(!compiled.backendInstructions().contains("Be warm and considerate."));
        QVERIFY(!compiled.backendInstructions().contains("Use local tools for device data."));
        const auto backend=compiled.backendInstructions();voice->setPersonality("brief");QVERIFY(voice->promptsReady());
        compiled.reload("brief",true);QCOMPARE(compiled.backendInstructions(),backend);
        QVERIFY(compiled.liveInstructions().contains("Be very brief."));
        QVERIFY(!QJsonDocument(voice->snapshot()).toJson().contains("Custom backend procedure"));

        write("assistant/live.md",QByteArray(1,char(0xff)));voice->reloadConfiguration();
        QVERIFY(!voice->promptsReady());QVERIFY(voice->promptProblem().contains("live.md"));
        // A broken Live-only file does not block Realtime or leak Live instructions to it.
        voice->setModel("gpt-realtime-2.1-mini");QVERIFY(voice->promptsReady());
        compiled.reload("brief");QVERIFY(compiled.ready());QVERIFY(compiled.instructions().contains("Use local tools for device data."));
        QVERIFY(!compiled.instructions().contains("Custom backend procedure"));
        voice->setModel("gpt-live-1");QVERIFY(!voice->promptsReady());
        write("assistant/live.md","Live policy\n");write("assistant/backend.md",QByteArray("bad\0policy",10));voice->reloadConfiguration();
        QVERIFY(!voice->promptsReady());QVERIFY(voice->promptProblem().contains("backend.md"));
        write("assistant/backend.md",QByteArray(32769,'x'));voice->reloadConfiguration();QVERIFY(!voice->promptsReady());
        write("assistant/backend.md","\n ");voice->reloadConfiguration();QVERIFY(!voice->promptsReady());
        QFile::remove(directory.filePath("assistant/backend.md"));voice->reloadConfiguration();
        QVERIFY(!voice->promptsReady());QVERIFY(voice->promptProblem().contains("backend.md"));
        write("assistant/backend.md","Backend procedure\n");
        write("assistant/live.md",QByteArray(32768,'x'));write("assistant/personalities/brief.md",QByteArray(32768,'y'));
        voice->reloadConfiguration();QVERIFY(!voice->promptsReady());QVERIFY(voice->promptProblem().contains("64 KiB"));
        write("assistant/live.md","Live policy\n");write("assistant/personalities/brief.md","Brief delivery\n");
        QFile::remove(directory.filePath("assistant/system.md"));voice->reloadConfiguration();QVERIFY(voice->promptsReady());
        voice->setModel("gpt-realtime-2.1-mini");QVERIFY(!voice->promptsReady());QVERIFY(voice->promptProblem().contains("system.md"));
    }
    void voiceSessionStreamsInterruptsAndDisables(){
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;
        auto cleanupPeer=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject o){QVERIFY(peer);peer->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){
            peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
                auto o=QJsonDocument::fromJson(message.toUtf8()).object();requests.append(o);
                if(o.value("type")=="session.update")send({{"type","session.updated"}});
            });send({{"type","session.created"}});
        });
        screen->wake();auto voice=media->assistant();voice->start();QVERIFY(!voice->active()); // master disable is enforced in C++
        voice->setEnabled(true);QVERIFY(voice->configured());voice->setModel("gpt-realtime-2.1");
        voice->previewVoice();QTRY_VERIFY(voice->previewPlaying());voice->start();QTRY_VERIFY(peer&&!voice->released()&&!voice->previewPlaying());
        QCOMPARE(QUrlQuery(peer->requestUrl()).queryItemValue("model"),QString("gpt-realtime-2.1"));
        voice->setModel("gpt-realtime-2.1-mini");QCOMPARE(voice->model(),QString("gpt-realtime-2.1")); // active sessions keep their model
        QVERIFY(!object("assistantModelChoice")->property("enabled").toBool());
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="input_audio_buffer.append";}));
        auto session=requests.first().value("session").toObject();
        const QString prompt=session.value("instructions").toString();QVERIFY(prompt.contains("Use local tools for device data."));QVERIFY(prompt.contains("Be warm and considerate."));QVERIFY(prompt.contains("Test person prefers jazz."));
        voice->setPersonality("brief");QCOMPARE(voice->personality(),QString("warm")); // active sessions keep their prompt
        QCOMPARE(session.value("audio").toObject().value("output").toObject().value("voice").toString(),QString("marin"));
        QCOMPARE(session.value("audio").toObject().value("input").toObject().value("turn_detection").toObject().value("type").toString(),QString("semantic_vad"));
        send({{"type","response.created"},{"response",QJsonObject{{"id","response-1"}}}});
        send({{"type","response.output_audio.delta"},{"response_id","response-1"},{"item_id","item-1"},{"content_index",0},{"delta",QString::fromLatin1(QByteArray(24000,'\0').toBase64())}});
        send({{"type","response.output_audio_transcript.delta"},{"response_id","response-1"},{"item_id","item-1"},{"delta","Hello there."}});
        QTRY_VERIFY(!voice->transcript().isEmpty());QTRY_VERIFY(voice->outputLevel()>0);
        const auto destination=qEnvironmentVariable("THINKSMART_CAPTURE_DIR");
        if(!destination.isEmpty()){
            QDir().mkpath(destination);window->resize(1280,800);window->setProperty("selectedApp","assistant");QTest::qWait(80);
            QVERIFY(window->grabWindow().save(destination+"/assistant-speaking.png"));
        }
        send({{"type","input_audio_buffer.speech_started"}});
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="conversation.item.truncate"&&r.value("audio_end_ms")==137&&r.value("item_id")=="item-1";}));
        // Late chunks from the interrupted response must not enter the caption or playback path.
        send({{"type","response.output_audio_transcript.delta"},{"response_id","response-1"},{"item_id","item-1"},{"delta","SHOULD NOT APPEAR"}});
        QTest::qWait(40);QCOMPARE(voice->transcript().last().toMap().value("text").toString(),QString("Hello there."));
        voice->setEnabled(false);QVERIFY(!voice->active());QTRY_VERIFY(voice->released());QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);
        int count=requests.size();QTest::qWait(60);QCOMPARE(requests.size(),count);
    }
    void voiceManualTurnsToolsAndSleepCleanup(){
        QWebSocketServer server("mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;
        auto cleanupPeer=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject o){QVERIFY(peer);peer->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
            auto o=QJsonDocument::fromJson(message.toUtf8()).object();requests.append(o);if(o.value("type")=="session.update")send({{"type","session.updated"}});
        });send({{"type","session.created"}});});
        screen->wake();auto voice=media->assistant();voice->setEnabled(true);voice->setVad("manual");voice->start();QTRY_VERIFY(!voice->released());
        QTRY_VERIFY(peer);QCOMPARE(QUrlQuery(peer->requestUrl()).queryItemValue("model"),QString("gpt-realtime-2.1-mini"));
        QTest::qWait(60);QVERIFY(!std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="input_audio_buffer.append";}));
        voice->beginTurn();QTest::qWait(160);voice->endTurn();
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="input_audio_buffer.commit";}));
        auto call=[&](const QString &id,const QString &name,const QJsonObject &args){
            send({{"type","response.done"},{"response",QJsonObject{{"id",id},{"status","completed"},{"output",QJsonArray{QJsonObject{{"type","function_call"},{"call_id",id},{"name",name},{"arguments",QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}}}}}}});
        };
        call("battery-call","device_action",{{"action","battery.get"}});
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){auto i=r.value("item").toObject();return i.value("call_id")=="battery-call"&&QJsonDocument::fromJson(i.value("output").toString().toUtf8()).object().value("battery").isObject();}));
        call("bad-call","device_action",{{"action","shutdown"}});
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){auto i=r.value("item").toObject();return i.value("call_id")=="bad-call"&&!QJsonDocument::fromJson(i.value("output").toString().toUtf8()).object().value("ok").toBool();}));
        auto count=std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("item").toObject().value("call_id")=="battery-call";});
        call("battery-call","device_action",{{"action","battery.get"}});QTest::qWait(40);
        QCOMPARE(std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("item").toObject().value("call_id")=="battery-call";}),count);
        screen->sleep();QVERIFY(!voice->active());QTRY_VERIFY(voice->released());QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);
    }
    void liveManagedToolsAndFullDuplex(){
        qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        prepareWakeAssets();screen->wake();
        QWebSocketServer server("live-mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;
        auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject o){QVERIFY(peer);peer->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
            const auto o=QJsonDocument::fromJson(message.toUtf8()).object();requests.append(o);
            if(o.value("type")=="session.start"){
                send({{"type","session.started"},{"session",QJsonObject{{"id","live-test"}}}});
                send({{"type","session.output_audio.delta"},{"delta",QString::fromLatin1(QByteArray(4800,'\0').toBase64())}});
            }
            if(o.value("type")=="session.close"){
                // Retiring sockets must not execute late backend calls.
                send({{"type","response.event"},{"delegation_id","d1"},{"event",QJsonObject{{"type","response.output_item.done"},{"item",QJsonObject{{"type","function_call"},{"name","device_action"},{"call_id","late"},{"arguments","{\"action\":\"media.volume\",\"percent\":1}"}}}}}});
                send({{"type","session.closed"},{"usage",QJsonObject{{"seconds",7.5}}}});
            }
        });});
        auto voice=media->assistant();voice->setModel("gpt-live-1");QVERIFY(voice->isLive());QVERIFY(voice->voices().contains("willow"));
        QCOMPARE(voice->backendModel(),QString("gpt-6-luna"));voice->setBackendModel("gpt-6-sol");voice->setBackendModel("gpt-6-luna");
        QCOMPARE(QSettings().value("assistant/liveBackendModel").toString(),QString("gpt-6-luna"));
        voice->setVoice("willow");voice->setVad("manual");voice->setEnabled(true);voice->start();QTRY_VERIFY(peer&&voice->active()&&voice->inputLevel()>0);
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="session.input_audio.append";}));
        const auto session=requests.first().value("session").toObject();QCOMPARE(session.value("model").toString(),QString("gpt-live-1"));
        QVERIFY(peer->requestUrl().query().isEmpty());QVERIFY(!session.value("store").toBool());
        const auto backend=session.value("delegation").toObject().value("responses").toObject();
        QCOMPARE(backend.value("model").toString(),QString("gpt-6-luna"));QCOMPARE(backend.value("reasoning").toObject().value("effort").toString(),QString("low"));
        QVERIFY(backend.value("instructions").toString().contains("Test person prefers jazz."));
        const auto frontendPrompt=session.value("instructions").toString(),backendPrompt=backend.value("instructions").toString();
        for(const auto &label:{"Backchannel policy:","Interruption policy:","Delegation policy:","Backend tools:","Generate <|SpawnThinking|> when:","Do not generate <|SpawnThinking|> when:"})QVERIFY(frontendPrompt.contains(label));
        QVERIFY(frontendPrompt.contains("Be warm and considerate."));QVERIFY(frontendPrompt.contains("Test person prefers jazz."));
        QVERIFY(!frontendPrompt.contains("Use local tools for device data."));QVERIFY(!frontendPrompt.contains("radio.search"));
        QVERIFY(backendPrompt.contains("radio.search"));QVERIFY(backendPrompt.contains("radio4"));QVERIFY(backendPrompt.contains("Do not ask for a station or genre."));
        QVERIFY(!backendPrompt.contains("Be warm and considerate."));QVERIFY(!backendPrompt.contains("Use local tools for device data."));
        QCOMPARE(session.value("audio").toObject().value("output").toObject().value("voice").toString(),QString("willow"));
        voice->setBackendModel("gpt-6-sol");QCOMPARE(voice->backendModel(),QString("gpt-6-luna"));
        const auto destination=qEnvironmentVariable("THINKSMART_CAPTURE_DIR");
        if(!destination.isEmpty()){QDir().mkpath(destination);QFile f(destination+"/live-session-probe.json");QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(requests.first()).toJson());}
        voice->beginTurn();voice->endTurn();
        QVERIFY(!std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="input_audio_buffer.commit"||r.value("type")=="session.update";}));
        auto transcript=[&](const QString &type,const QString &id,const QString &text,int start,int end){send({{"type",type},{"event_id",id},{"delta",text},{"start_ms",start},{"end_ms",end}});};
        transcript("session.input_transcript.delta","u1","Play ",0,300);transcript("session.output_transcript.delta","a1","I’ll check.",100,800);
        transcript("session.input_transcript.delta","u2","BBC Radio 4.",300,900);transcript("session.input_transcript.delta","u1","Play ",0,300);
        QTRY_COMPARE(voice->transcript().size(),2);QTRY_COMPARE(voice->transcript()[0].toMap().value("text").toString(),QString("Play BBC Radio 4."));
        send({{"type","session.output_audio.delta"},{"delta",QString::fromLatin1(QByteArray(24000,'\0').toBase64())}});QTRY_VERIFY(voice->outputLevel()>0);
        auto nested=[&](QJsonObject e){send({{"type","response.event"},{"delegation_id","d1"},{"event",e}});};
        send({{"type","session.delegation.created"},{"delegation",QJsonObject{{"id","d1"},{"target","responses"},{"response_id","r1"}}}});
        nested({{"type","response.created"},{"response",QJsonObject{{"id","r1"}}}});
        auto call=[&](const QString &id,const QString &action){nested({{"type","response.output_item.done"},{"item",QJsonObject{{"type","function_call"},{"call_id",id},{"name","device_action"},{"arguments",QString::fromUtf8(QJsonDocument(QJsonObject{{"action",action}}).toJson(QJsonDocument::Compact))}}}});};
        call("battery","battery.get");call("blocked","shutdown");call("battery","battery.get");
        nested({{"type","response.completed"},{"response",QJsonObject{{"id","r1"},{"output",QJsonArray{}}}}});
        QTRY_COMPARE(std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.item.create";}),2);
        QTRY_COMPARE(std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.create";}),1);
        for(const auto &r:requests){if(r.value("type")!="response.item.create")continue;const auto item=r.value("item").toObject();const auto result=QJsonDocument::fromJson(item.value("output").toString().toUtf8()).object();QCOMPARE(result.value("ok").toBool(),item.value("call_id")=="battery");}
        nested({{"type","response.completed"},{"response",QJsonObject{{"id","r1"}}}});QTest::qWait(40);
        QCOMPARE(std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.create";}),1);
        nested({{"type","response.created"},{"response",QJsonObject{{"id","r2"}}}});nested({{"type","response.completed"},{"response",QJsonObject{{"id","r2"}}}});
        voice->interrupt();QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="session.instructions.append";}));
        QVERIFY(!std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.cancel"||r.value("type")=="conversation.item.truncate";}));
        const auto volume=media->volume();voice->setEnabled(false);QTRY_VERIFY(voice->released());QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);
        QCOMPARE(media->volume(),volume);QCOMPARE(voice->snapshot().value("liveUsageSeconds").toDouble(),7.5);QVERIFY(voice->snapshot().value("liveUsageFinalized").toBool());
    }
    void liveSilenceDoesNotHoldMediaOrIdleSession(){
        qputenv("THINKSMART_SHARED_AUDIO","1");qputenv("THINKSMART_VOICE_TEST_SILENT","1");
        auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");qunsetenv("THINKSMART_VOICE_TEST_SILENT");});
        prepareWakeAssets();screen->wake();
        QWebSocketServer server("live-silence",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &m){
            auto type=QJsonDocument::fromJson(m.toUtf8()).object().value("type").toString();
            if(type=="session.start")peer->sendTextMessage("{\"type\":\"session.started\"}");
            if(type=="session.close")peer->sendTextMessage("{\"type\":\"session.closed\",\"usage\":{\"seconds\":20}}");
        });});
        auto voice=media->assistant();voice->setModel("gpt-live-1");voice->setEnabled(true);voice->start();QTRY_VERIFY(peer&&voice->active());
        QTimer silence;silence.setInterval(50);connect(&silence,&QTimer::timeout,this,[&](){if(peer&&peer->state()==QAbstractSocket::ConnectedState)peer->sendTextMessage(QString::fromUtf8(QJsonDocument(QJsonObject{{"type","session.output_audio.delta"},{"delta",QString::fromLatin1(QByteArray(2400,'\0').toBase64())}}).toJson(QJsonDocument::Compact)));});silence.start();
        QTRY_VERIFY_WITH_TIMEOUT(!voice->snapshot().value("mediaDucked").toBool(),2000);
        QTRY_VERIFY_WITH_TIMEOUT(!voice->active(),23000);silence.stop();
        QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);QVERIFY(voice->wakeListening());QVERIFY(!voice->snapshot().value("mediaDucked").toBool());
    }
    void radioDirectoryToolsWaitForResults_data(){QTest::addColumn<bool>("live");QTest::newRow("realtime")<<false;QTest::newRow("live")<<true;}
    void radioDirectoryToolsWaitForResults(){
        QFETCH(bool,live);qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");qunsetenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS");});
        prepareWakeAssets();screen->wake();RadioServer directory;directory.delay=180;directory.rows={RadioServer::row()};
        qputenv("THINKSMART_RADIO_BROWSER_TEST_SERVERS",directory.url().toUtf8());
        QWebSocketServer server("radio-mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject o){QVERIFY(peer);peer->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &m){
            const auto o=QJsonDocument::fromJson(m.toUtf8()).object();requests.append(o);
            if(o.value("type")=="session.update")send({{"type","session.updated"}});
            if(o.value("type")=="session.start")send({{"type","session.started"}});
            if(o.value("type")=="session.close")send({{"type","session.closed"},{"usage",QJsonObject{{"seconds",1}}}});
        });if(!live)send({{"type","session.created"}});});
        auto voice=media->assistant();voice->setModel(live?"gpt-live-1":"gpt-realtime-2.1-mini");voice->setEnabled(true);voice->start();QTRY_VERIFY(peer&&voice->inputLevel()>0);
        auto call=[](const QString &id,const QJsonObject &args){return QJsonObject{{"type","function_call"},{"call_id",id},{"name","device_action"},{"arguments",QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}};};
        auto dispatch=[&](const QString &response,const QJsonArray &calls){
            if(live){
                auto nested=[&](QJsonObject e){send({{"type","response.event"},{"delegation_id","directory"},{"event",e}});};
                nested({{"type","response.created"},{"response",QJsonObject{{"id",response}}}});
                for(const auto &item:calls)nested({{"type","response.output_item.done"},{"item",item}});
                nested({{"type","response.completed"},{"response",QJsonObject{{"id",response},{"output",QJsonArray{}}}}});
            }else{send({{"type","response.created"},{"response",QJsonObject{{"id",response}}}});send({{"type","response.done"},{"response",QJsonObject{{"id",response},{"status","completed"},{"output",calls}}}});}
        };
        dispatch("search",{call("search-call",{{"action","radio.search"},{"country","CO"},{"language","es"},{"tag","cumbia"}}),call("battery-call",{{"action","battery.get"}})});
        QTRY_COMPARE(directory.searches(),1);QTest::qWait(40);
        QCOMPARE(std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.create";}),0);
        QTRY_COMPARE(std::count_if(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.create";}),1);
        QJsonObject result;for(const auto &request:requests){const auto item=request.value("item").toObject();if(item.value("call_id")=="search-call")result=QJsonDocument::fromJson(item.value("output").toString().toUtf8()).object();}
        QVERIFY(result.value("ok").toBool());QCOMPARE(result.value("stations").toArray().size(),1);QCOMPARE(result.value("stations").toArray().first().toObject().value("url").toString(),QString("https://stream.example.test/radio"));
        dispatch("late-search",{call("late-call",{{"action","radio.search"},{"tag","ambient"}})});QTRY_COMPARE(directory.searches(),2);
        voice->setEnabled(false);QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);const auto count=requests.size();QTest::qWait(230);QCOMPARE(requests.size(),count);
        QVERIFY(!std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("item").toObject().value("call_id")=="late-call";}));
        if(live){
            // Local callers must also receive slow searches after the old 3s
            // request-input deadline, without blocking other UI commands.
            directory.delay=3150;QLocalSocket client;client.connectToServer(Controller::socketPath());QVERIFY(client.waitForConnected(1000));
            client.write("{\"action\":\"radio.search\",\"tag\":\"slow-query\"}\n");
            QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable()>0,5000);QVERIFY(QJsonDocument::fromJson(client.readAll()).object().value("ok").toBool());
        }
    }
    void voiceDismissal_data(){
        QTest::addColumn<bool>("live");QTest::addColumn<QString>("method");
        for(bool live:{false,true})for(const auto &method:{"transcript","early-tool","batched-tool","false-activation","invalid-arguments"})
            QTest::newRow(qPrintable(QString(live?"live-":"realtime-")+method))<<live<<QString(method);
    }
    void voiceDismissal(){
        QFETCH(bool,live);QFETCH(QString,method);
        qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        prepareWakeAssets();screen->wake();
        QWebSocketServer server("dismissal-mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;
        auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject event){QVERIFY(peer);peer->sendTextMessage(QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
            const auto event=QJsonDocument::fromJson(message.toUtf8()).object();requests.append(event);
            if(event.value("type")=="session.update")send({{"type","session.updated"}});
            if(event.value("type")=="session.start")send({{"type","session.started"}});
            if(event.value("type")=="session.close"){
                // Calls/audio on a retiring connection must never revive it.
                send({{"type","response.event"},{"delegation_id","dismiss"},{"event",QJsonObject{{"type","response.output_item.done"},{"item",QJsonObject{{"type","function_call"},{"name","device_action"},{"call_id","late-volume"},{"arguments","{\"action\":\"media.volume\",\"percent\":1}"}}}}}});
                send({{"type","session.closed"},{"usage",QJsonObject{{"seconds",2.5}}}});
            }
        });if(!live)send({{"type","session.created"}});});
        auto voice=media->assistant();voice->setModel(live?"gpt-live-1":"gpt-realtime-2.1-mini");voice->setEnabled(true);voice->start();
        QTRY_VERIFY(peer&&voice->active()&&voice->inputLevel()>0);
        const auto session=requests.first().value("session").toObject();
        const auto toolOwner=live?session.value("delegation").toObject().value("responses").toObject():session;
        const auto tools=toolOwner.value("tools").toArray();
        QVERIFY(std::any_of(tools.cbegin(),tools.cend(),[](auto v){return v.toObject().value("name")=="end_conversation";}));
        QVERIFY(session.value("instructions").toString().contains("go to sleep"));QVERIFY(toolOwner.value("instructions").toString().contains("TV, radio or background conversation"));
        const auto volume=media->volume();const auto source=media->source(),station=media->radioId();
        QSignalSpy stopMedia(media.get(),&Controller::cancelDiscoveryRequested);
        const auto audio=QString::fromLatin1(QByteArray(24000,'\0').toBase64());
        if(live)send({{"type","session.output_audio.delta"},{"delta",audio}});
        else{send({{"type","response.created"},{"response",QJsonObject{{"id","speech"}}}});send({{"type","response.output_audio.delta"},{"response_id","speech"},{"item_id","speech-item"},{"delta",audio}});}
        QTRY_VERIFY(voice->outputLevel()>0);QVERIFY(voice->snapshot().value("mediaDucked").toBool());
        auto nested=[&](QJsonObject event){send({{"type","response.event"},{"delegation_id","dismiss"},{"event",event}});};
        auto call=[](const QString &id,const QString &name,const QString &args){return QJsonObject{{"type","function_call"},{"call_id",id},{"name",name},{"arguments",args}};};
        auto transcript=[&](const QString &id,const QString &text,int start){
            if(live)send({{"type","session.input_transcript.delta"},{"event_id",id},{"delta",text},{"start_ms",start},{"end_ms",start+300}});
            else send({{"type","conversation.item.input_audio_transcription.completed"},{"item_id",id},{"transcript",text}});
        };
        if(method=="transcript"){
            transcript("quoted","The phrase is 'go to sleep'.",0);QTest::qWait(300);QVERIFY(voice->active());
            if(live){
                transcript("longer-a","Go to sleep",3000);QTest::qWait(40);transcript("longer-b"," is just an example.",3300);
                QTest::qWait(300);QVERIFY(voice->active());
                transcript("command-a","Sheila, please go to ",6000);QTest::qWait(30);QVERIFY(voice->active());
                transcript("command-b","sleep.",6300);
            }else{
                send({{"type","response.output_audio_transcript.delta"},{"response_id","speech"},{"item_id","speech-item"},{"delta","Go to sleep."}});
                QTest::qWait(60);QVERIFY(voice->active());transcript("command","Sheila, please go to sleep.",6000);
            }
        }else{
            const QString args=method=="false-activation"?"{\"reason\":\"false_activation\"}":"{\"reason\":\"user_request\"}";
            if(method=="invalid-arguments"){
                auto invalid=call("invalid","end_conversation","{\"reason\":\"shutdown\"}");
                if(live){nested({{"type","response.created"},{"response",QJsonObject{{"id","invalid"}}}});nested({{"type","response.output_item.done"},{"item",invalid}});nested({{"type","response.completed"},{"response",QJsonObject{{"id","invalid"}}}});}
                else send({{"type","response.done"},{"response",QJsonObject{{"id","invalid"},{"status","completed"},{"output",QJsonArray{invalid}}}}});
                QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("item").toObject().value("call_id")=="invalid";}));
                QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.create";}));
                QVERIFY(voice->active());requests.clear();
            }
            const auto ending=call("end","end_conversation",args);
            const auto other=call("volume","device_action","{\"action\":\"media.volume\",\"percent\":1}");
            if(live){
                nested({{"type","response.created"},{"response",QJsonObject{{"id","dismiss"}}}});
                if(method=="batched-tool")nested({{"type","response.output_item.done"},{"item",other}});
                nested({{"type","response.output_item.done"},{"item",ending}});
                // No response.completed: terminal tools must work immediately.
            }else if(method=="batched-tool")send({{"type","response.done"},{"response",QJsonObject{{"id","dismiss"},{"status","completed"},{"output",QJsonArray{other,ending}}}}});
            else send({{"type","response.output_item.done"},{"response_id","dismiss"},{"item",ending}});
        }
        QTRY_VERIFY_WITH_TIMEOUT(!voice->active(),1500);QVERIFY(voice->enabled());QVERIFY(voice->wakeListening());
        QVERIFY(!screen->sleeping());QCOMPARE(media->volume(),volume);QCOMPARE(media->source(),source);QCOMPARE(media->radioId(),station);QCOMPARE(stopMedia.count(),0);
        QCOMPARE(voice->outputLevel(),0.);QVERIFY(!voice->snapshot().value("mediaDucked").toBool());
        const auto diagnostic=voice->snapshot().value("lastTool").toObject();QCOMPARE(diagnostic.value("action").toString(),QString("conversation.end"));
        QCOMPARE(diagnostic.value("reason").toString(),method=="false-activation"?QString("false_activation"):QString("user_request"));
        QTRY_VERIFY(peer->state()==QAbstractSocket::UnconnectedState);
        QVERIFY(!std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("type")=="response.create";}));
        const auto count=requests.size();QTest::qWait(80);QCOMPARE(requests.size(),count);
        voice->setEnabled(false);QTRY_VERIFY(voice->released());
    }
    void personalNotesThemesRoutinesAndTouchControls(){
        screen->wake();click("notesPageButton");QTest::qWait(300);
        QVERIFY(object("notesPage"));QVERIFY(!object("dictateNoteButton")->property("enabled").toBool());
        auto added=media->toolRequest({{"action","notes.add"},{"data",QJsonObject{{"text","Remember <b>milk</b> and coffee"}}}});QVERIFY(added.value("ok").toBool());
        QTRY_COMPARE(object("personalList")->property("count").toInt(),1);
        window->resize(1280,800);QTest::qWait(50);click("deletePersonal0");QTRY_COMPARE(media->personal()->notes().size(),0);
        auto theme=media->personalAction("theme.save",{{"id","plum"},{"base","dusk"},{"colors",QVariantMap{{"accent","#e8b568"}}}});QVERIFY(theme.value("ok").toBool());
        QCOMPARE(media->themeName(),QString("plum"));QCOMPARE(media->themePalette().value("accent").toString(),QString("#e8b568"));
        QJsonArray steps{QJsonObject{{"action","theme.apply"},{"id","autumn"}},QJsonObject{{"action","media.volume"},{"percent",27}}};
        QVERIFY(media->toolRequest({{"action","routine.save"},{"data",QJsonObject{{"id","evening"},{"title","Evening"},{"steps",steps}}}}).value("ok").toBool());
        click("personalSection1");QTest::qWait(50);click("runRoutine0");QCOMPARE(media->volume(),27);QCOMPARE(media->themeName(),QString("autumn"));
        QVERIFY(media->toolRequest({{"action","schedule.save"},{"data",QJsonObject{{"title","Later"},{"routineId","evening"},{"afterSeconds",600}}}}).value("ok").toBool());
        click("personalSection2");QTest::qWait(50);click("toggleSchedule0");QVERIFY(!media->personal()->schedules().first().toMap().value("enabled").toBool());
        click("deletePersonal0");QVERIFY(media->personal()->schedules().isEmpty());
        click("personalSection1");QTest::qWait(50);click("deletePersonal0");QVERIFY(media->personal()->routines().isEmpty());
        window->resize(800,1280);QTest::qWait(50);click("personalSection0");
        QVERIFY(warnings.isEmpty());
    }
    void schedulerRunsWithoutAssistantAndAlarmWakesScreen(){
        QProcess scheduler;scheduler.start(QCoreApplication::applicationDirPath()+"/thinksmart-actions");QVERIFY(scheduler.waitForStarted());
        auto cleanup=qScopeGuard([&](){scheduler.terminate();scheduler.waitForFinished(2000);});
        QVERIFY(!media->assistant()->enabled());
        QJsonArray steps{QJsonObject{{"action","theme.apply"},{"id","autumn"}},QJsonObject{{"action","screen.wake"}}};
        QVERIFY(media->toolRequest({{"action","schedule.save"},{"data",QJsonObject{{"title","Wake later"},{"steps",steps},{"afterSeconds",1}}}}).value("ok").toBool());
        screen->sleep();QVERIFY(screen->sleeping());
        QTRY_VERIFY_WITH_TIMEOUT(!screen->sleeping(),5000);QCOMPARE(media->themeName(),QString("autumn"));
        QTRY_COMPARE(media->personal()->schedules().first().toMap().value("status").toString(),QString("completed"));
        QVERIFY(!media->assistant()->active());
        QTest::qWait(1100);QCOMPARE(media->personal()->schedules().size(),1);
        const QJsonArray alarm{QJsonObject{{"action","alarm.ring"},{"text","Check the oven"}}};
        screen->sleep();QVERIFY(media->toolRequest({{"action","automation.execute"},{"steps",alarm}}).value("ok").toBool());
        QVERIFY(!screen->sleeping());QCOMPARE(media->alarmText(),QString("Check the oven"));QTRY_VERIFY(object("alarmPopup")->property("visible").toBool());
        click("dismissAlarmButton");QVERIFY(media->alarmText().isEmpty());
    }
    void dictationOnlySavesNotes_data(){QTest::addColumn<bool>("live");QTest::newRow("realtime")<<false;QTest::newRow("live")<<true;}
    void dictationOnlySavesNotes(){
        QFETCH(bool,live);qputenv("THINKSMART_SHARED_AUDIO","1");auto reset=qScopeGuard([](){qunsetenv("THINKSMART_SHARED_AUDIO");});
        prepareWakeAssets();screen->wake();click("notesPageButton");QTest::qWait(300);
        QWebSocketServer server("dictation-mock",QWebSocketServer::NonSecureMode);QVERIFY(server.listen(QHostAddress::LocalHost));
        qputenv("THINKSMART_VOICE_TEST_URL",QString("ws://127.0.0.1:%1").arg(server.serverPort()).toUtf8());
        QPointer<QWebSocket> peer;QList<QJsonObject> requests;
        auto cleanup=qScopeGuard([&](){if(peer){peer->disconnect(this);delete peer.data();}});
        auto send=[&](QJsonObject o){QVERIFY(peer);peer->sendTextMessage(QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact)));};
        connect(&server,&QWebSocketServer::newConnection,this,[&](){peer=server.nextPendingConnection();connect(peer,&QWebSocket::textMessageReceived,this,[&](const QString &message){
            const auto event=QJsonDocument::fromJson(message.toUtf8()).object();requests.append(event);
            if(event.value("type")=="session.update")send({{"type","session.updated"}});
            if(event.value("type")=="session.start")send({{"type","session.started"}});
            if(event.value("type")=="session.close")send({{"type","session.closed"},{"usage",QJsonObject{{"seconds",1}}}});
        });if(!live)send({{"type","session.created"}});});
        auto voice=media->assistant();voice->setModel(live?"gpt-live-1":"gpt-realtime-2.1-mini");voice->setEnabled(true);
        QTRY_VERIFY(voice->wakeListening());click("dictateNoteButton");QTRY_VERIFY(peer&&voice->noteDictation()&&voice->inputLevel()>0);
        const auto session=requests.first().value("session").toObject();const auto toolOwner=live?session.value("delegation").toObject().value("responses").toObject():session;
        const auto tools=toolOwner.value("tools").toArray();QCOMPARE(tools.size(),1);QCOMPARE(tools.first().toObject().value("name").toString(),QString("personal_action"));
        QVERIFY(session.value("instructions").toString().contains("DICTATION MODE"));QVERIFY(toolOwner.value("instructions").toString().contains("DICTATION MODE"));
        if(live)send({{"type","session.input_transcript.delta"},{"event_id","dictated-command"},{"delta","Go to sleep."},{"start_ms",0},{"end_ms",500}});
        else send({{"type","conversation.item.input_audio_transcription.completed"},{"item_id","dictated-command"},{"transcript","Go to sleep."}});
        QTest::qWait(300);QVERIFY(voice->active());QVERIFY(voice->noteDictation());
        auto dispatch=[&](const QString &id,const QString &name,const QJsonObject &args){
            const QJsonObject call{{"type","function_call"},{"call_id",id},{"name",name},{"arguments",QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact))}};
            if(live){
                auto nested=[&](QJsonObject e){send({{"type","response.event"},{"delegation_id","notes"},{"event",e}});};
                nested({{"type","response.created"},{"response",QJsonObject{{"id",id}}}});
                nested({{"type","response.output_item.done"},{"item",call}});
                nested({{"type","response.completed"},{"response",QJsonObject{{"id",id}}}});
            }else send({{"type","response.done"},{"response",QJsonObject{{"id",id},{"status","completed"},{"output",QJsonArray{call}}}}});
        };
        const auto volume=media->volume();dispatch("blocked","device_action",{{"action","media.volume"},{"percent",1}});
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("item").toObject().value("call_id")=="blocked";}));QCOMPARE(media->volume(),volume);
        dispatch("save","personal_action",{{"action","notes.add"},{"data",QJsonObject{{"text","Stop the radio and remember coffee"}}}});
        QTRY_COMPARE(media->personal()->notes().size(),1);QCOMPARE(media->personal()->notes().first().toMap().value("text").toString(),QString("Stop the radio and remember coffee"));
        QTRY_COMPARE(object("personalList")->property("count").toInt(),1);
        dispatch("duplicate","personal_action",{{"action","notes.add"},{"data",QJsonObject{{"text","Duplicate"}}}});
        QTRY_VERIFY(std::any_of(requests.cbegin(),requests.cend(),[](auto r){return r.value("item").toObject().value("call_id")=="duplicate";}));QCOMPARE(media->personal()->notes().size(),1);
        voice->setEnabled(false);QVERIFY(!voice->noteDictation());QTRY_VERIFY(voice->released());
    }
    void capturePublicScreenshots(){
        const QString destination=qEnvironmentVariable("THINKSMART_PUBLIC_SCREENSHOTS");
        if(destination.isEmpty())QSKIP("Set THINKSMART_PUBLIC_SCREENSHOTS to render public demonstration screenshots");
        QVERIFY(QDir().mkpath(destination));
        auto demo=[&](QObject *source,const QVariantMap &overrides){
            auto values=new QQmlPropertyMap(this);const auto meta=source->metaObject();
            for(int i=meta->propertyOffset();i<meta->propertyCount();++i){const auto property=meta->property(i);values->insert(property.name(),property.read(source));}
            for(auto i=overrides.begin();i!=overrides.end();++i)values->insert(i.key(),i.value());
            return values;
        };
        QVariantList hours;for(int i=0;i<6;i++)hours.append(QVariantMap{{"label",QString("%1:00").arg(14+i)},{"temperature_2m",17-i/2},{"condition","Partly cloudy"}});
        engine->rootContext()->setContextProperty("weather",demo(weather.get(),{{"location","London"},{"current",QVariantMap{{"temperature_2m",17},{"condition","Partly cloudy"},{"apparent_temperature",16},{"relative_humidity_2m",64},{"wind_speed_10m",7}}},{"hourly",hours},{"stale",false},{"status","Updated · demonstration data"}}));
        QVariantMap metrics;for(const auto &reading:QList<QPair<QString,QString>>{{"soc","72.4%"},{"solar","420 W"},{"batteryPower","180 W"},{"dcLoad","95 W"},{"acLoad","145 W"},{"voltage","13.2 V"},{"current","13.6 A"},{"yieldToday","1.8 kWh"},{"yieldYesterday","2.4 kWh"}})
            metrics.insert(reading.first,QVariantMap{{"available",true},{"value",reading.first=="soc"?72.4:reading.first=="batteryPower"?180.:0.},{"text",reading.second},{"age","Just now"},{"when","Demonstration data"},{"stale",false}});
        engine->rootContext()->setContextProperty("energy",demo(energy.get(),{{"metrics",metrics},{"hasData",true},{"stale",false},{"problem",""},{"status","Demonstration data"}}));
        prepareWakeAssets();media->assistant()->setEnabled(true);QTRY_VERIFY(media->assistant()->wakeListening());
        screen->wake();media->setTheme("spring");
        auto capture=[&](const QString &name,int width=1280,int height=800){window->resize(width,height);QTest::qWait(350);QVERIFY(window->grabWindow().save(destination+"/"+name+".png"));};
        capture("home-landscape");capture("home-portrait",800,1280);
        window->resize(1280,800);media->openAppRequested("news");capture("news");
        media->openAppRequested("chess");capture("chess");
        media->openAppRequested("assistant");capture("assistant");
    }
    void captureMockup() {
        const QString destination=qEnvironmentVariable("THINKSMART_CAPTURE_DIR");
        if(destination.isEmpty())QSKIP("Set THINKSMART_CAPTURE_DIR to export design previews");
        QVERIFY(QDir().mkpath(destination));
        screen->wake();
        auto capture=[&](const QString &name,int width,int height) {
            window->resize(width,height);
            QTest::qWait(350);
            QVERIFY(window->grabWindow().save(destination+"/"+name+".png"));
        };
        const QString selected=qEnvironmentVariable("THINKSMART_CAPTURE_APP");
        if(selected=="personal"){
            media->toolRequest({{"action","notes.add"},{"data",QJsonObject{{"title","A thought for the weekend"},{"text","Take a walk if the weather clears.\nBring the camera and a flask of coffee."}}}});
            media->toolRequest({{"action","notes.add"},{"data",QJsonObject{{"title","To pick up"},{"text","Coffee beans, oat milk and something for dinner."}}}});
            media->toolRequest({{"action","routine.save"},{"data",QJsonObject{{"id","evening"},{"title","Evening radio"},{"steps",QJsonArray{QJsonObject{{"action","theme.apply"},{"id","autumn"}},QJsonObject{{"action","media.volume"},{"percent",30}},QJsonObject{{"action","radio.play"},{"id","radio4"}}}}}}});
            media->toolRequest({{"action","schedule.save"},{"data",QJsonObject{{"title","Weekday radio"},{"time","09:00"},{"weekdays",QJsonArray{1,2,3,4,5}},{"routineId","evening"}}}});
            media->openAppRequested("notes");
            capture("notes-landscape",1280,800);capture("notes-portrait",800,1280);
            click("personalSection1");capture("routines-landscape",1280,800);
            click("personalSection2");capture("schedules-portrait",800,1280);return;
        }
        if(selected=="chess"){
            media->openAppRequested("chess");
            capture("chess-landscape",1280,800);capture("chess-portrait",800,1280);
            media->setTheme("dawn");capture("chess-dawn",1280,800);media->setTheme("tide");capture("chess-tide",1280,800);
            chessGame->setRating(1600);chessGame->switchSides();QTRY_COMPARE(chessGame->moveCount(),1);
            capture("chess-black-landscape",1280,800);capture("chess-black-portrait",800,1280);return;
        }
        if(selected=="news"){
            object("dashboardPages")->setProperty("currentIndex",media->dashboardPages().indexOf("news"));
            capture("news-landscape",1280,800);capture("news-portrait",800,1280);
            hackerNews->openStory(0);hackerNews->toggleReplies("201");
            capture("hn-discussion-landscape",1280,800);capture("hn-discussion-portrait",800,1280);return;
        }
        if(!selected.isEmpty()){
            if(selected=="assistant"){
                media->assistant()->setEnabled(true);
                const auto model=qEnvironmentVariable("THINKSMART_CAPTURE_MODEL");if(!model.isEmpty())media->assistant()->setModel(model);
            }
            window->setProperty("selectedApp",selected);
            if(selected=="video"&&video->configured()){video->refresh();QTRY_VERIFY_WITH_TIMEOUT(!video->busy(),15000);QTest::qWait(1000);}
            capture(selected+"-landscape",1280,800);capture(selected+"-portrait",800,1280);
            if(selected=="video"){video->inspect(0);capture("video-details-landscape",1280,800);capture("video-details-portrait",800,1280);}
            if(selected=="assistant"){
                auto core=object("assistantCore");core->setProperty("active",true);core->setProperty("animate",false);core->setProperty("mode","speaking");core->setProperty("level",0.8);core->setProperty("phase",4.2);
                capture("assistant-field-detail",1280,800);
                media->setTheme("tide");capture("assistant-tide",1280,800);media->setTheme("dawn");capture("assistant-dawn",1280,800);media->setTheme("ember");
                object("assistantPage")->setProperty("tab","settings");capture("assistant-settings-landscape",1280,800);capture("assistant-settings-portrait",800,1280);}
            return;
        }
        capture("dashboard-ember-landscape",1280,800);
        capture("dashboard-ember-portrait",800,1280);
        object("dashboardPages")->setProperty("currentIndex",media->dashboardPages().indexOf("news"));
        capture("more-apps-landscape",1280,800);
        capture("more-apps-portrait",800,1280);
        object("dashboardPages")->setProperty("currentIndex",media->dashboardPages().indexOf("home"));
        media->setTheme("tide");
        capture("dashboard-tide-landscape",1280,800);
        media->setTheme("dawn");
        capture("dashboard-dawn-landscape",1280,800);
        for(const auto &theme:{"autumn","spring","dusk"}){
            media->setTheme(theme);
            capture(QString("dashboard-%1-landscape").arg(theme),1280,800);
        }
        capture("dashboard-dusk-portrait",800,1280);
        media->setTheme("ember");
        window->setProperty("selectedApp","assistant");
        capture("assistant-motion-preview",1280,800);
    }
    void cleanup() {
        const auto savedWarnings=warnings;
        engine.reset();chessGame.reset();headlines.reset();hackerNews.reset();weather.reset();radio.reset();music.reset();video.reset();energy.reset();media.reset();screen.reset();window=nullptr;
        QVERIFY2(savedWarnings.isEmpty(),qPrintable(savedWarnings.join('\n')));
    }
    void cleanupTestCase() {helper.terminate();QVERIFY(helper.waitForFinished(3000));}
};
int main(int argc,char **argv){
    QTemporaryDir keyboardConfig;qputenv("XDG_CONFIG_HOME",keyboardConfig.path().toUtf8());
    bool keyboard=false;for(int i=1;i<argc;i++)if(QByteArray(argv[i])=="wifiDialogKeyboardAndSavedPreferences"||QByteArray(argv[i])=="marketsKeyboard")keyboard=true;
    qputenv("QT_IM_MODULE",keyboard?"qtvirtualkeyboard":"none");qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE","1");
    QGuiApplication app(argc,argv);HomeUiTests tests;return QTest::qExec(&tests,argc,argv);
}
#include "home-ui-tests.moc"
