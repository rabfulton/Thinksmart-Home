#include <QGuiApplication>
#include <QInputMethod>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTimer>
#include <cstdio>
#include <functional>

// Hardware smoke check: a separate synthetic window, no NetworkManager,
// household data, credentials or assistant session. Exits after one test key.
int main(int argc,char **argv){
    qputenv("QT_IM_MODULE","qtvirtualkeyboard");qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE","1");
    QGuiApplication app(argc,argv);QQmlApplicationEngine engine;
    engine.loadData(R"QML(
import QtQuick
import QtQuick.Controls
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Settings
import "."
ApplicationWindow {
    visible: true; width: 1280; height: 800; visibility: Window.FullScreen; color: Theme.background
    function probeKey() { InputContext.inputEngine.virtualKeyClick(Qt.Key_A, "a", 0) }
    TextField {
        objectName: "probeField"; x: 32; y: 32; width: parent.width - 64; height: 64
        placeholderText: "Local keyboard check · no network changes"; font.pixelSize: 24
        inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
    }
    InputPanel {
        objectName: "probeKeyboard"; width: parent.width; y: parent.height - height; visible: active
        Component.onCompleted: { VirtualKeyboardSettings.styleName="thinksmart"; VirtualKeyboardSettings.activeLocales=["en_GB"]; VirtualKeyboardSettings.locale="en_GB" }
    }
}
)QML",QUrl("qrc:/qml/KeyboardCheck.qml"));
    if(engine.rootObjects().isEmpty())return 1;
    auto window=qobject_cast<QQuickWindow *>(engine.rootObjects().first());if(!window)return 1;
    auto field=window->findChild<QQuickItem *>("probeField");auto keyboard=window->findChild<QQuickItem *>("probeKeyboard");
    std::function<QQuickItem *(QQuickItem *)> findKey=[&](QQuickItem *item)->QQuickItem *{
        if(item->objectName()=="virtualKey_a"&&item->isVisible())return item;
        for(auto child:item->childItems())if(auto key=findKey(child))return key;
        return nullptr;
    };
    QTimer::singleShot(350,&app,[&](){window->requestActivate();field->forceActiveFocus();QGuiApplication::inputMethod()->show();});
    QTimer poll;poll.setInterval(100);bool pressed=false;
    QObject::connect(&poll,&QTimer::timeout,&app,[&](){
        auto key=findKey(window->contentItem());if(pressed||!key||!keyboard->isVisible())return;pressed=true;
        QMetaObject::invokeMethod(window,"probeKey");
        QTimer::singleShot(150,&app,[&](){
            const bool typed=field->property("text").toString()=="a";
            const auto result=QJsonDocument(QJsonObject{{"keyboardVisible",keyboard->isVisible()},{"inputMethodVisible",QGuiApplication::inputMethod()->isVisible()},{"windowActive",window->isActive()},{"fieldFocused",field->hasActiveFocus()},{"syntheticKeyTyped",typed}}).toJson(QJsonDocument::Compact);
            std::fwrite(result.constData(),1,result.size(),stdout);std::fputc('\n',stdout);app.exit(typed?0:2);
        });
    });poll.start();
    QTimer::singleShot(6000,&app,[&](){if(!pressed){std::fputs("{\"syntheticKeyTyped\":false,\"error\":\"Keyboard did not become ready\"}\n",stdout);app.exit(2);}});
    return app.exec();
}
