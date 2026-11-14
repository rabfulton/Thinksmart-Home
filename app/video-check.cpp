// Standalone local-clip render check; does not touch the home app or Jellyfin.
#include "videoplayer.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QJsonDocument>
#include <QFileInfo>
#include <cstdio>
int main(int argc,char **argv){
    QGuiApplication app(argc,argv);if(argc!=3||!QFileInfo(argv[1]).isFile())return 2;registerVideoTypes();
    VideoPlayer player;QQmlApplicationEngine engine;engine.rootContext()->setContextProperty("probePlayer",&player);

    engine.loadData(R"QML(import QtQuick
import QtQuick.Window
import ThinkSmart 1.0
Window { width: 1280; height: 800; visible: true; visibility: Window.FullScreen; color: "black"; VideoSurface { anchors.fill: parent; player: probePlayer } }
)QML");
    QTimer::singleShot(250,&app,[&](){player.start(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])),{},0);});
    if(engine.rootObjects().isEmpty())return 3;auto *window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());bool captured=false;
    QObject::connect(&player,&VideoPlayer::event,&app,[&](const QJsonObject &e){
        if(e.value("event")=="property-change"&&e.value("name")=="time-pos"&&e.value("data").toDouble()>.5&&!captured){
            captured=true;player.command({"set_property","pause",true});
            QTimer::singleShot(200,&app,[&](){bool saved=window->grabWindow().save(QString::fromLocal8Bit(argv[2]));auto json=QJsonDocument(QJsonObject{{"decoder",player.decoder()},{"rendered",saved}}).toJson(QJsonDocument::Compact);std::printf("%s\n",json.constData());player.stop();app.exit(saved&&player.decoder()=="v4l2m2m-copy"?0:4);});
        }
        if(e.value("event")=="end-file"&&!captured)app.exit(5);
    });QTimer::singleShot(15000,&app,[&](){app.exit(6);});return app.exec();
}
