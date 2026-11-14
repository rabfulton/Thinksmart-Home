#include "../display.h"
#include <QtTest>
#include <QGuiApplication>
#include <QWindow>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTouchEvent>
#include <QJsonDocument>
#include <linux/input-event-codes.h>

class EventWindow : public QWindow {
public:
    int presses=0,releases=0,touches=0;
    bool event(QEvent *event) override {
        if(event->type()==QEvent::MouseButtonPress)++presses;
        if(event->type()==QEvent::MouseButtonRelease)++releases;
        if(event->type()==QEvent::TouchBegin){++touches;event->accept();return true;}
        if(event->type()==QEvent::TouchUpdate||event->type()==QEvent::TouchEnd){event->accept();return true;}
        return QWindow::event(event);
    }
};
class DisplayTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QProcess helper;
    void write(const QString &name,const QByteArray &value) {
        QFile file(directory.filePath(name));QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(value),value.size());
    }
    int actual() {
        QFile file(directory.filePath("brightness"));if(!file.open(QIODevice::ReadOnly))return -1;return file.readAll().trimmed().toInt();
    }
    QJsonObject request(const QByteArray &request) {
        QLocalSocket socket;socket.connectToServer(directory.filePath("control.sock"));
        if(!socket.waitForConnected(1000))return {};
        socket.write(request);socket.waitForBytesWritten(1000);
        if(!socket.waitForReadyRead(1500))return {};
        return QJsonDocument::fromJson(socket.readAll()).object();
    }
private slots:
    void initTestCase() {
        QVERIFY(directory.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.filePath("settings"));
        QCoreApplication::setOrganizationName("ThinkSmartTests");
        QCoreApplication::setApplicationName("Display");
        qputenv("THINKSMART_POWER_SOCKET",directory.filePath("control.sock").toUtf8());
        write("brightness","2048\n");write("maximum","4095\n");write("action","");
        helper.start(QCoreApplication::applicationDirPath()+"/thinksmart-power",{"--test-directory",directory.path()});
        QVERIFY(helper.waitForStarted());
        QTRY_VERIFY(QFile::exists(directory.filePath("control.sock")));
    }
    void init() { QSettings settings;settings.clear();settings.sync(); }
    void helperRejectsInvalidCommands() {
        QVERIFY(request("status\n").value("test").toBool());
        for(const auto &input:{"brightness -1\n","brightness 101\n","brightness 50x\n","brightness \n","halt; reboot\n","status\nhalt\n"})
            QCOMPARE(request(input).value("ok").toBool(true),false);
        QCOMPARE(request(QByteArray(129,'x')+'\n').value("ok").toBool(true),false);
        QCOMPARE(request("brightness 100\n").value("ok").toBool(),true);QCOMPARE(actual(),4095);
        QCOMPARE(request("brightness 0\n").value("ok").toBool(),true);QCOMPARE(actual(),0);
        QFile action(directory.filePath("action"));QVERIFY(action.open(QIODevice::ReadOnly));QVERIFY(action.readAll().isEmpty());
    }
    void startsAsleepAndMouseWakeIsConsumed() {
        ScreenController display;EventWindow window;window.resize(800,1280);window.show();
        QVERIFY(display.sleeping());QTRY_COMPARE(actual(),0);
        QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,QPoint(30,40));
        QVERIFY(!display.sleeping());QCOMPARE(window.presses,0);QCOMPARE(window.releases,0);QTRY_COMPARE(actual(),2048);
        QTest::mouseClick(&window,Qt::LeftButton,Qt::NoModifier,QPoint(30,40));
        QCOMPARE(window.presses,1);QCOMPARE(window.releases,1);
    }
    void firstTouchIsConsumedForWholeGesture() {
        ScreenController display;EventWindow window;window.resize(800,1280);window.show();
        const auto device=QTest::createTouchDevice();
        QTest::touchEvent(&window,device).press(0,QPoint(50,60),&window);
        QTest::touchEvent(&window,device).move(0,QPoint(70,80),&window);
        QTest::touchEvent(&window,device).release(0,QPoint(70,80),&window);
        QVERIFY(!display.sleeping());QCOMPARE(window.touches,0);QCOMPARE(window.presses,0);QCOMPARE(window.releases,0);
        QTest::touchEvent(&window,device).press(0,QPoint(50,60),&window);
        QTest::touchEvent(&window,device).release(0,QPoint(50,60),&window);
        QCOMPARE(window.touches,1);
    }
    void volumeWakeConsumesPressAndRepeats() {
        ScreenController display;
        QVERIFY(display.volumeKey(KEY_VOLUMEUP,1));QVERIFY(!display.sleeping());
        QVERIFY(display.volumeKey(KEY_VOLUMEUP,2));QVERIFY(display.volumeKey(KEY_VOLUMEUP,0));
        QVERIFY(!display.volumeKey(KEY_VOLUMEUP,1));QVERIFY(display.volumeKey(KEY_VOLUMEUP,0));
        display.sleep();QVERIFY(display.volumeKey(KEY_VOLUMEDOWN,1));QVERIFY(!display.sleeping());
        QVERIFY(display.volumeKey(KEY_VOLUMEDOWN,0));QVERIFY(!display.volumeKey(KEY_VOLUMEDOWN,1));
    }
    void idleDimSleepAndActivityReset() {
        ScreenController display;QVERIFY(display.setIdleTimers(1,3));display.wake();
        QTRY_VERIFY_WITH_TIMEOUT(display.dimmed(),2000);QTRY_COMPARE(actual(),1433);
        display.activity();QVERIFY(!display.dimmed());QTRY_COMPARE(actual(),2048);
        QTRY_VERIFY_WITH_TIMEOUT(display.dimmed(),2000);
        QTRY_VERIFY_WITH_TIMEOUT(display.sleeping(),4000);QTRY_COMPARE(actual(),0);
        display.wake();QVERIFY(!display.dimmed());QTRY_COMPARE(actual(),2048);
    }
    void settingsValidateAndPersist() {
        {
            ScreenController display;
            QVERIFY(!display.setIdleTimers(60,30));QVERIFY(!display.setIdleTimers(-1,300));
            QVERIFY(!display.setIdleTimers(0,7201));
            QVERIFY(display.setIdleTimers(120,600));display.setBrightness(80);
        }
        ScreenController display;QCOMPARE(display.dimSeconds(),120);QCOMPARE(display.sleepSeconds(),600);
        QCOMPARE(display.brightness(),80);QVERIFY(display.sleeping());QTRY_COMPARE(actual(),0);
        display.wake();QTRY_COMPARE(actual(),3276);
    }
    void helperFailureRecoversWithoutChangingAppState() {
        ScreenController display;display.wake();QTRY_COMPARE(actual(),2048);
        helper.kill();QVERIFY(helper.waitForFinished(3000));
        QCOMPARE(actual(),2048);
        display.setBrightness(80);
        QTRY_VERIFY(!display.error().isEmpty());
        helper.start(QCoreApplication::applicationDirPath()+"/thinksmart-power",{"--test-directory",directory.path()});
        QVERIFY(helper.waitForStarted());
        QTRY_COMPARE(actual(),3276);
        QTRY_VERIFY(display.error().isEmpty());QVERIFY(!display.sleeping());
    }
    void powerActionsUseFixedBackendAndStopPlaybackFirst() {
        ScreenController display;QSignalSpy stopping(&display,&ScreenController::stopping);
        display.shutdown();QCOMPARE(stopping.count(),1);QVERIFY(display.powerPending());
        QTRY_VERIFY(!display.status().value("backlightPending").toBool());
        QFile action(directory.filePath("action"));QVERIFY(action.open(QIODevice::ReadOnly));QCOMPARE(action.readAll(),QByteArray("halt\n"));
        // Test backend only: never halt or reboot the test host.
        QCOMPARE(request("reboot\n").value("ok").toBool(),true);
        action.close();QVERIFY(action.open(QIODevice::ReadOnly));QCOMPARE(action.readAll(),QByteArray("reboot\n"));
    }
    void cleanupTestCase() {helper.terminate();QVERIFY(helper.waitForFinished(3000));}
};
QTEST_MAIN(DisplayTests)
#include "display-tests.moc"
