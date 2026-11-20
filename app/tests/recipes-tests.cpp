#include "../recipes.h"
#include "../deviceconfig.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>

class RecipesTests: public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QTcpServer server;
    QJsonObject received;
    int requests=0;
    bool fail=false,incomplete=false;
    QString root;
    void save(const QString &name,const QByteArray &bytes){QFile f(root+'/'+name);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(bytes),bytes.size());f.setPermissions(QFile::ReadOwner|QFile::WriteOwner);}
    QJsonObject example(QString title="Lentil soup")const{return {{"action","recipes.show"},{"title",title},{"left","## Ingredients\n- [ ] 200 g lentils\n- [ ] 1 onion"},{"right","## Method\n1. Rinse the lentils.\n2. Simmer until tender."}};}
private slots:
    void initTestCase(){
        QVERIFY(server.listen(QHostAddress::LocalHost));
        connect(&server,&QTcpServer::newConnection,this,[this](){while(server.hasPendingConnections()){
            auto *socket=server.nextPendingConnection();auto bytes=std::make_shared<QByteArray>();
            connect(socket,&QTcpSocket::readyRead,this,[this,socket,bytes](){
                *bytes+=socket->readAll();const auto boundary=bytes->indexOf("\r\n\r\n");if(boundary<0)return;
                const auto headers=bytes->left(boundary);QRegularExpression re("content-length: (\\d+)",QRegularExpression::CaseInsensitiveOption);
                const auto match=re.match(QString::fromLatin1(headers));if(!match.hasMatch()||bytes->size()-boundary-4<match.captured(1).toInt())return;
                requests++;received=QJsonDocument::fromJson(bytes->mid(boundary+4)).object();
                const auto recipe=example();const auto document=QJsonObject{{"title",recipe.value("title")},{"left",recipe.value("left")},{"right",recipe.value("right")}};
                const QJsonObject response{{"status",incomplete?"incomplete":"completed"},{"output",QJsonArray{QJsonObject{{"type","message"},{"content",QJsonArray{QJsonObject{{"type","output_text"},{"text",QString::fromUtf8(QJsonDocument(document).toJson())}}}}}}}};
                const auto body=fail?QByteArray("private upstream failure body"):QJsonDocument(response).toJson();
                socket->write(QByteArray(fail?"HTTP/1.1 503 Unavailable\r\n":"HTTP/1.1 200 OK\r\n")+"Content-Type: application/json\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket->disconnectFromHost();
            });connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        }});
    }
    void init(){
        root=temporary.path()+'/'+QTest::currentTestFunction();QDir().mkpath(root);requests=0;received={};fail=incomplete=false;
        qputenv("THINKSMART_DEVICE_CONFIG",(root+"/device.json").toUtf8());qputenv("THINKSMART_DASHBOARD_CONFIG",(root+"/dashboard.json").toUtf8());
        qputenv("THINKSMART_RECIPES_FILE",(root+"/history.json").toUtf8());qputenv("THINKSMART_RECIPES_CONFIG",(root+"/settings.json").toUtf8());qputenv("THINKSMART_RECIPES_PROMPT",(root+"/system.md").toUtf8());
        qputenv("THINKSMART_VOICE_CONFIG",(root+"/voice.json").toUtf8());qputenv("THINKSMART_RECIPES_TEST_ENDPOINT",QString("http://127.0.0.1:%1/v1/responses").arg(server.serverPort()).toUtf8());
        save("device.json","{\"modules\":{\"recipes\":true}}");save("voice.json","{\"provider\":\"openai\",\"apiKey\":\"fixture-key\"}");DeviceConfig::reload();
    }
    void historyNavigationDeletionAndSettingsArePersistent(){
        RecipeBook book;QVERIFY(book.current().isEmpty());QCOMPARE(requests,0);
        QVERIFY(book.request(example()).value("ok").toBool());const auto first=book.current().value("id");
        QVERIFY(book.request(example("Stew")).value("ok").toBool());QCOMPARE(book.count(),2);book.navigate(1);QCOMPARE(book.current().value("id"),first);book.navigate(-1);QCOMPARE(book.current().value("title").toString(),QString("Stew"));
        QVERIFY(book.configure(30,"custom-text-model","text"));RecipeBook restored;QCOMPARE(restored.fontSize(),30);QCOMPARE(restored.model(),QString("custom-text-model"));QCOMPARE(restored.purpose(),QString("text"));QCOMPARE(restored.count(),2);QCOMPARE(requests,0);
        QVERIFY(restored.removeCurrent());QCOMPARE(restored.current().value("id"),first);RecipeBook again;QCOMPARE(again.count(),1);QVERIFY(!(QFileInfo(RecipeBook::storagePath()).permissions()&QFile::ReadOther));
    }
    void generationUsesExplicitRequestsAndPreservesRevisionSource(){
        RecipeBook book;QVERIFY(book.request(example()).value("ok").toBool());const auto id=book.current().value("id").toString();
        const auto result=book.request({{"action","recipes.compose"},{"request","Make this serve four"},{"id",id}});QVERIFY(result.value("accepted").toBool());QVERIFY(book.busy());QCOMPARE(book.count(),1);
        QVERIFY(!book.request({{"action","recipes.compose"},{"request","Another"}}).value("ok").toBool());
        QTRY_VERIFY(!book.busy());QCOMPARE(book.count(),2);QCOMPARE(requests,1);QCOMPARE(received.value("store").toBool(true),false);QCOMPARE(received.value("model").toString(),book.model());
        const auto input=QJsonDocument::fromJson(received.value("input").toString().toUtf8()).object();QCOMPARE(input.value("previousOutput").toObject().value("id").toString(),id);
        QCOMPARE(received.value("text").toObject().value("format").toObject().value("type").toString(),QString("json_schema"));RecipeBook restored;QCOMPARE(restored.count(),2);
    }
    void failedIncompleteOrInvalidRequestsKeepSavedOutputs(){
        RecipeBook book;QVERIFY(book.request(example()).value("ok").toBool());const auto before=book.current();
        fail=true;QVERIFY(book.request({{"action","recipes.compose"},{"request","A soup"}}).value("ok").toBool());QTRY_VERIFY(!book.busy());QCOMPARE(book.current(),before);QVERIFY(!book.problem().contains("private upstream"));QVERIFY(!book.problem().isEmpty());
        fail=false;incomplete=true;QVERIFY(book.request({{"action","recipes.compose"},{"request","A soup"}}).value("ok").toBool());QTRY_VERIFY(!book.busy());QCOMPARE(book.current(),before);
        QVERIFY(!book.request({{"action","recipes.compose"},{"request","A soup"},{"id","missing"}}).value("ok").toBool());QCOMPARE(requests,2);
        auto large=example();large.insert("left",QString(16001,'x'));QVERIFY(!book.request(large).value("ok").toBool());QCOMPARE(book.count(),1);
    }
    void disabledAndCorruptHistoryDoNoWork(){
        save("device.json","{\"modules\":{\"recipes\":false}}");DeviceConfig::reload();RecipeBook disabled;
        QVERIFY(!disabled.request(example()).value("ok").toBool());QVERIFY(!disabled.request({{"action","recipes.compose"},{"request","A soup"}}).value("ok").toBool());QCOMPARE(requests,0);QVERIFY(!QFile::exists(RecipeBook::storagePath()));
        save("device.json","{\"modules\":{\"recipes\":true}}");DeviceConfig::reload();save("history.json","broken");RecipeBook bad;QVERIFY(!bad.request(example()).value("ok").toBool());QFile file(RecipeBook::storagePath());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),QByteArray("broken"));
    }
    void imageSyntaxIsNeutralisedAcrossRepeatedSaves(){
        RecipeBook book;auto content=example();content.insert("left","Text ![inline](https://example.invalid/image.png)\n![shortcut]\n[shortcut]: https://example.invalid/other.png");
        QVERIFY(book.request(content).value("ok").toBool());const auto first=book.current().value("left").toString();QVERIFY(!first.contains("!["));content.insert("left",first);QVERIFY(book.request(content).value("ok").toBool());QCOMPARE(book.current().value("left").toString(),first);QCOMPARE(requests,0);
    }
};
QTEST_MAIN(RecipesTests)
#include "recipes-tests.moc"
