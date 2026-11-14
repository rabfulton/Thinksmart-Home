#include "controller.h"
#include "rotation.h"
#include "display.h"
#include "vrm.h"
#include "library.h"
#include "jellyfinimages.h"
#include "weather.h"
#include "news.h"
#include "chessgame.h"
#include "radio.h"
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QLockFile>
#include <QDir>
#include <cstdio>

int main(int argc,char **argv) {
    if(argc>1 && QByteArray(argv[1])=="--command") {
        QCoreApplication app(argc,argv);
        if(argc<3) return 2;
        QLocalSocket socket;
        socket.connectToServer(Controller::socketPath());
        if(!socket.waitForConnected(3000)) { fprintf(stderr,"Home app is unavailable\n"); return 1; }
        QByteArray request=argv[2];
        for(int i=3;i<argc;++i)request+=' '+QByteArray(argv[i]);
        socket.write(request+'\n');
        socket.waitForBytesWritten(1000);
        QByteArray response;
        const int replyTimeout=QJsonDocument::fromJson(request).object().value("action")=="radio.search"?20000:3000;
        QElapsedTimer timeout;timeout.start();
        while(!response.contains('\n')&&timeout.elapsed()<replyTimeout) {
            if(socket.bytesAvailable()==0&&!socket.waitForReadyRead(qMax(1,replyTimeout-int(timeout.elapsed()))))break;
            response+=socket.readAll();
            if(response.size()>256*1024)return 1;
        }
        QJsonParseError parse;
        const auto result=QJsonDocument::fromJson(response.trimmed(),&parse).object();
        if(parse.error!=QJsonParseError::NoError)return 1;
        fwrite(response.data(),1,response.size(),stdout);
        return result.contains("error")||(result.contains("ok")&&!result.value("ok").toBool())?2:0;
    }
    QGuiApplication app(argc,argv);
    registerVideoTypes();
    app.setOrganizationName("ThinkSmart");
    app.setApplicationName("Home");
    QLockFile lock(Controller::socketPath()+".lock");
    if(!lock.tryLock()) return 1;
    Rotation rotation;
    ScreenController display;
    Controller controller(&display);
    VrmClient energy;
    WeatherClient weather;
    HeadlinesClient headlines;
    HackerNewsClient hackerNews;
    ChessGame chessGame;
    RadioCatalog radio;
    controller.setDataSources(&weather,&radio,&energy);
    QObject::connect(&radio,&RadioCatalog::interruptPlayback,&controller,&Controller::stop);
    QObject::connect(&radio,&RadioCatalog::playRequested,&controller,&Controller::playRadio);
    QObject::connect(&controller,&Controller::cancelDiscoveryRequested,&radio,&RadioCatalog::cancel);
    QObject::connect(&controller,&Controller::radioRequested,&radio,&RadioCatalog::play);
    MediaLibrary music("navidrome"),video("jellyfin");
    controller.setVideoLibrary(&video);
    for(auto library:{&music,&video}) {
        QObject::connect(library,&MediaLibrary::playRequested,&controller,&Controller::playMedia);
        QObject::connect(&controller,&Controller::playbackEvent,library,&MediaLibrary::report);
    }
    QObject::connect(&controller,&Controller::advanceRequested,&music,&MediaLibrary::advance);
    QObject::connect(&controller,&Controller::replayRequested,&video,[&video](const QString &source){if(source=="jellyfin")video.replay();});
    QQmlApplicationEngine engine;
    engine.addImageProvider("jellyfin",new JellyfinImages(video.imageSettings()));
    engine.rootContext()->setContextProperty("home",&controller);
    engine.rootContext()->setContextProperty("weather",&weather);
    engine.rootContext()->setContextProperty("headlines",&headlines);
    engine.rootContext()->setContextProperty("hackerNews",&hackerNews);
    engine.rootContext()->setContextProperty("chessGame",&chessGame);
    engine.rootContext()->setContextProperty("radio",&radio);
    engine.rootContext()->setContextProperty("phoneAudio",controller.phone());
    engine.rootContext()->setContextProperty("audioLab",controller.audioLab());
    engine.rootContext()->setContextProperty("assistant",controller.assistant());
    engine.rootContext()->setContextProperty("musicLibrary",&music);
    engine.rootContext()->setContextProperty("videoLibrary",&video);
    engine.rootContext()->setContextProperty("energy",&energy);
    engine.rootContext()->setContextProperty("screenControl",&display);
    QObject::connect(&engine,&QQmlApplicationEngine::objectCreationFailed,&app,[](){QCoreApplication::exit(1);},Qt::QueuedConnection);
    engine.load(QUrl("qrc:/qml/Main.qml"));
    return app.exec();
}
