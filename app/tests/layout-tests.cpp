#include "../layouteditor.h"
#include "../deviceconfig.h"
#include "../assistantprompts.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

class LayoutTests:public QObject {
    Q_OBJECT
    QTemporaryDir dir;
    void write(const QString &path,const QByteArray &data){QDir().mkpath(QFileInfo(path).absolutePath());QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write(data);}
private slots:
    void initTestCase(){
        QVERIFY(dir.isValid());qputenv("THINKSMART_DEVICE_CONFIG",dir.filePath("device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",dir.filePath("dashboard.json").toUtf8());qputenv("THINKSMART_ASSISTANT_DIR",dir.filePath("assistant").toUtf8());
    }
    void init(){
        QFile::remove(DashboardConfig::path());QFile::remove(DashboardConfig::path()+".bak");write(DeviceConfig::path(),"{}");DeviceConfig::reload();
    }
    void validationAndAtomicRecovery(){
        auto config=DashboardConfig::defaults(DeviceConfig::modules());QVERIFY(DashboardConfig::validate(config).isEmpty());
        auto pages=config.value("pages").toArray();auto home=pages.last().toObject();
        auto tooMany=home;auto widgets=tooMany.value("widgets").toArray();widgets.append("weather");tooMany.insert("widgets",widgets);auto invalid=config;auto badPages=pages;badPages[badPages.size()-1]=tooMany;invalid.insert("pages",badPages);QVERIFY(!DashboardConfig::validate(invalid).isEmpty());
        invalid=config;badPages=pages;badPages.removeLast();invalid.insert("pages",badPages);QVERIFY(!DashboardConfig::validate(invalid).isEmpty());
        invalid=config;auto flags=invalid.value("modules").toObject();flags.insert("shell",true);invalid.insert("modules",flags);QVERIFY(!DashboardConfig::validate(invalid).isEmpty());
        QString error;QVERIFY(DashboardConfig::write(config,error));QVERIFY(QFileInfo(DashboardConfig::path()).permissions().testFlag(QFile::ReadOwner));QVERIFY(!QFileInfo(DashboardConfig::path()).permissions().testFlag(QFile::ReadOther));
        flags=config.value("modules").toObject();flags.insert("radio",false);config.insert("modules",flags);QVERIFY(DashboardConfig::write(config,error));DeviceConfig::reload();QVERIFY(!DeviceConfig::enabled("radio"));QVERIFY(DeviceConfig::enabled("weather"));
        write(DashboardConfig::path(),"corrupted");DeviceConfig::reload();QVERIFY(!DeviceConfig::problem().isEmpty());QVERIFY(!DeviceConfig::enabled("radio"));QVERIFY(!DeviceConfig::enabled("assistant")); // Recover placement without unexpectedly enabling integrations.
        write(DashboardConfig::path()+".bak","also corrupted");DeviceConfig::reload();for(const auto &id:DeviceConfig::names())QVERIFY(!DeviceConfig::enabled(id));QCOMPARE(DeviceConfig::pages(),QStringList{"home"});
    }
    void draftPlacementAndModuleState(){
        LayoutEditor editor;QCOMPARE(editor.pages().size(),4);QVERIFY(!editor.dirty());
        editor.addPage();QCOMPARE(editor.pages().size(),5);const int added=4;editor.renamePage(added,"Quiet room");editor.addWidget(added,"weather");editor.addWidget(added,"radio");editor.moveWidget(added,1,-1);QCOMPARE(editor.pages()[added].toMap().value("widgets").toList()[0].toString(),QString("radio"));
        editor.removeWidget(added,0);QVERIFY(editor.modules().value("radio").toBool());editor.setModule("radio",false);QVERIFY(editor.dirty());QVERIFY(DeviceConfig::enabled("radio"));QVERIFY(!QFile::exists(DashboardConfig::path()));
        editor.begin();QCOMPARE(editor.pages().size(),4);QVERIFY(!editor.dirty());editor.removePage(3);QCOMPARE(editor.pages().size(),4);QVERIFY(!editor.problem().isEmpty());
        editor.addPage("news");editor.setModule("news",false);QSignalSpy restart(&editor,&LayoutEditor::restartRequested);QVERIFY(editor.apply());QCOMPARE(restart.size(),1);DeviceConfig::reload();QVERIFY(!DeviceConfig::enabled("news"));QCOMPARE(DeviceConfig::pages().size(),3);QCOMPARE(DeviceConfig::dashboard().value("pages").toArray().size(),5);
    }
    void guidedApplicationsPreserveDraftAndHandleCapacity(){
        LayoutEditor editor;QVERIFY(editor.removeApplications({"chess","news","notes"}));QCOMPARE(editor.pages().size(),1);
        QVERIFY(DeviceConfig::enabled("chess"));QVERIFY(!editor.modules().value("chess").toBool());
        QVERIFY(!editor.placeApplication("chess","widget","home"));QVERIFY(!editor.modules().value("chess").toBool());
        QVERIFY(!editor.placeApplication("chess","page","home"));QCOMPARE(editor.pages().size(),1);
        QVERIFY(editor.placeApplication("chess","page",{},"Chess night"));QCOMPARE(editor.pages().size(),2);
        const auto chessPage=editor.pages().last().toMap().value("id");
        QVERIFY(editor.placeApplication("chess","page",{},"Chess night",true));QCOMPARE(editor.pages().last().toMap().value("id"),chessPage);
        QCOMPARE(editor.pages().last().toMap().value("module").toString(),QString("chess"));
        QVERIFY(!editor.placeApplication("chess","widget",{},"Duplicate"));
        QVERIFY(!editor.placeApplication("chess","widget","home",{},true));QCOMPARE(editor.pages().size(),2);
        QVERIFY(editor.placeApplication("chess","widget",{},"Games",true));QCOMPARE(editor.pages().size(),2);
        const auto games=editor.pages().last().toMap().value("id").toString();
        QVERIFY(editor.placeApplication("news","widget",games));QCOMPARE(editor.pages().last().toMap().value("widgets").toList().size(),2);
        QVERIFY(editor.placeApplication("notes","ondemand"));QCOMPARE(editor.pages().size(),2);
        QVERIFY(editor.removeApplications({"chess","news"}));QCOMPARE(editor.pages().size(),1);QCOMPARE(editor.pages().first().toMap().value("id").toString(),QString("home"));
        QVERIFY(editor.modules().value("notes").toBool());QVERIFY(!QFile::exists(DashboardConfig::path()));
        QVERIFY(editor.apply());DeviceConfig::reload();QVERIFY(!DeviceConfig::enabled("chess"));QVERIFY(DeviceConfig::enabled("notes"));
    }
    void guidedApplicationsShareCardsAndRespectPageLimit(){
        LayoutEditor editor;QVERIFY(editor.removeApplications({"bluetooth","navidrome"}));
        QVERIFY(editor.placeApplication("bluetooth","widget","home"));
        QCOMPARE(editor.locations("navidrome").size(),1); // Reuse a shared card on a full page.
        QVERIFY(editor.placeApplication("navidrome","widget","home"));
        QVERIFY(editor.removeApplications({"bluetooth"}));QVERIFY(editor.pages().last().toMap().value("widgets").toList().contains("music"));
        editor.setModule("weather",false);QVERIFY(editor.removeApplications({"navidrome"}));
        QVERIFY(editor.pages().last().toMap().value("widgets").toList().contains("weather"));
        for(int i=editor.pages().size();i<24;++i)editor.addPage();QCOMPARE(editor.pages().size(),24);
        QVERIFY(!editor.placeApplication("markets","page"));QVERIFY(!editor.modules().value("markets").toBool());
        QVERIFY(editor.placeApplication("markets","ondemand"));QCOMPARE(editor.pages().size(),24);
    }
    void enabledModulePromptsAndNativePermissions(){
        const auto root=dir.filePath("assistant/");write(root+"household.md","Household fixture");write(root+"personalities/warm.md","Warm fixture");write(root+"system.md","Core realtime policy");write(root+"live.md","Core Live policy");write(root+"backend.md","Core backend policy");
        AssistantPrompts prompts;prompts.reload("warm");QVERIFY(prompts.ready());QVERIFY(prompts.instructions().contains("BBC Radio 4"));QVERIFY(prompts.instructions().contains("notes.add"));
        LayoutEditor editor;editor.setModule("radio",false);editor.setModule("weather",false);editor.setModule("vrm",false);editor.setModule("notes",false);QVERIFY(editor.apply());DeviceConfig::reload();
        write(root+"modules/radio/realtime.md",QByteArray(40000,'x')); // A disabled module must not even load its broken file.
        prompts.reload("warm");QVERIFY(prompts.ready());QVERIFY(!prompts.instructions().contains("BBC Radio 4"));QVERIFY(!prompts.instructions().contains("notes.add"));QVERIFY(!prompts.instructions().contains("battery.get"));
        prompts.reload("warm",true);QVERIFY(prompts.ready());QVERIFY(!prompts.liveInstructions().contains("Radio module"));QVERIFY(!prompts.backendInstructions().contains("radio.play"));QVERIFY(prompts.liveInstructions().contains("Warm fixture"));QVERIFY(!prompts.backendInstructions().contains("Warm fixture"));
        for(const auto &action:{"radio.play","weather.get","battery.get","notes.add"})QVERIFY(!DeviceConfig::actionEnabled(action));QVERIFY(DeviceConfig::actionEnabled("media.volume"));
    }
};
QTEST_GUILESS_MAIN(LayoutTests)
#include "layout-tests.moc"
