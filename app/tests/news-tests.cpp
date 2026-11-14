#include "../news.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QJsonDocument>
#include <QUrlQuery>
#include <QAbstractItemModelTester>
#include <QPersistentModelIndex>

class NewsTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QTcpServer server;
    QUrl base;
    QList<QUrl> requests;
    bool offline=false;
    QByteArray rss(const QString &topic)const{
        return ("<rss><channel><item><guid>"+topic+"-1</guid><title>"+topic+" headline &amp; context - Example Press</title><link>https://example.test/"+topic+"</link><source>Example Press</source><pubDate>Fri, 13 Nov 2026 14:00:00 GMT</pubDate><description>&lt;p&gt;A &lt;b&gt;useful&lt;/b&gt; summary.&lt;/p&gt;</description></item></channel></rss>").toUtf8();
    }
    void accept(QTcpSocket *socket){
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        connect(socket,&QTcpSocket::readyRead,this,[this,socket](){
            auto buffer=socket->property("buffer").toByteArray()+socket->readAll();socket->setProperty("buffer",buffer);if(!buffer.contains("\r\n\r\n")||socket->property("served").toBool())return;socket->setProperty("served",true);
            const auto path=QUrl::fromEncoded(buffer.split(' ')[1]);requests.append(path);QByteArray body;int status=offline?503:200;
            if(!offline){
                if(path.path().contains("/rss/"))body=rss(path.path()=="/rss/search"?"FINANCE":path.path().section('/',-1));
                else if(path.path()=="/topstories.json")body="[101,102]";
                else if(path.path()=="/item/101.json")body=R"({"id":101,"type":"story","title":"A &amp; B","url":"https://example.test/story","by":"author","score":42,"descendants":20,"time":1794578400,"kids":[200,201,202,203,204,205,206,207,208,209,210,211]})";
                else if(path.path()=="/item/102.json")body=R"({"id":102,"type":"story","title":"Second story","kids":[300]})";
                else if(path.path()=="/item/300.json")body=R"({"id":300,"type":"comment","by":"old","text":"An old response"})";
                else{
                    const auto id=path.path().section('/',-1).section('.',0,0).toInt();
                    QJsonObject o{{"id",id},{"type","comment"},{"by","reader"},{"text","<p>First paragraph.</p><p>Second &amp; third.</p>"},{"time",1794578500}};
                    if(id==200)o.insert("kids",QJsonArray{400});
                    if(id==201){o.insert("deleted",true);o.insert("text","Do not display this removed content");}
                    if(id==400)o.insert("kids",QJsonArray{200}); // Defensive cycle handling.
                    body=QJsonDocument(o).toJson(QJsonDocument::Compact);
                }
            }
            socket->write("HTTP/1.1 "+QByteArray::number(status)+" Result\r\nContent-Type: application/json\r\nContent-Length: "+QByteArray::number(body.size())+"\r\nConnection: close\r\n\r\n"+body);socket->disconnectFromHost();
        });
    }
    QVariantMap row(QAbstractItemModel *model,int index=0){return model->data(model->index(index,0),Qt::UserRole).toMap();}
private slots:
    void initTestCase(){
        QVERIFY(directory.isValid());qputenv("XDG_CACHE_HOME",directory.filePath("cache").toUtf8());
        QCoreApplication::setOrganizationName("ThinkSmartTests");QCoreApplication::setApplicationName("News");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.filePath("settings"));
        QVERIFY(server.listen(QHostAddress::LocalHost));base=QUrl(QString("http://127.0.0.1:%1/").arg(server.serverPort()));
        connect(&server,&QTcpServer::newConnection,this,[this](){while(server.hasPendingConnections())accept(server.nextPendingConnection());});
    }
    void init(){offline=false;requests.clear();QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).removeRecursively();QSettings().clear();}
    void parsesEntitiesAndRejectsMalformedOrUnsafeItems(){
        const auto rows=HeadlinesClient::parseRss(rss("WORLD"));QCOMPARE(rows.size(),1);const auto first=rows.first().toMap();
        QCOMPARE(first.value("title").toString(),QString("WORLD headline & context"));QCOMPARE(first.value("source").toString(),QString("Example Press"));
        QCOMPARE(first.value("summary").toString(),QString("A useful summary."));QCOMPARE(first.value("when").toString(),QString("13 Nov · 14:00"));
        QVERIFY(HeadlinesClient::parseRss("<rss><item>").isEmpty());
        QVERIFY(HeadlinesClient::parseRss("<rss><channel><item><title>Unsafe</title><link>javascript:bad()</link></item></channel></rss>").isEmpty());
    }
    void internationalFinanceMixAndOfflineRetention(){
        {
            HeadlinesClient feed(base);QVERIFY(requests.isEmpty());QCOMPARE(feed.section(),QString("WORLD_FINANCE"));
            feed.setActive(true);QTRY_VERIFY_WITH_TIMEOUT(!feed.busy(),5000);QCOMPARE(feed.items()->rowCount(),2);
            QCOMPARE(row(feed.items(),0).value("section").toString(),QString("WORLD"));QCOMPARE(row(feed.items(),1).value("section").toString(),QString("FINANCE"));
            QCOMPARE(requests.size(),2);QCOMPARE(QUrlQuery(requests[0]).queryItemValue("ceid"),QString("GB:en"));QVERIFY(QUrlQuery(requests[1]).queryItemValue("q").contains("global markets"));
            feed.refresh();QCOMPARE(requests.size(),2); // Manual refresh is bounded.
        }
        offline=true;HeadlinesClient cached(base);QCOMPARE(cached.items()->rowCount(),2);const auto before=row(cached.items());
        cached.refresh();QTRY_VERIFY_WITH_TIMEOUT(!cached.busy(),5000);QVERIFY(!cached.problem().isEmpty());QCOMPARE(row(cached.items()),before);
    }
    void hnThreadRepliesPaginationCancellationAndCache(){
        {
            HackerNewsClient hn(base);QAbstractItemModelTester modelTest(hn.comments(),QAbstractItemModelTester::FailureReportingMode::QtTest);
            hn.refresh();QTRY_VERIFY(!hn.busy());QCOMPARE(hn.items()->rowCount(),2);QCOMPARE(row(hn.items()).value("title").toString(),QString("A & B"));
            hn.openStory(0);QTRY_VERIFY(!hn.commentsBusy());QCOMPARE(hn.comments()->rowCount(),10);QVERIFY(hn.moreComments());
            QPersistentModelIndex first(hn.comments()->index(0,0));QCOMPARE(row(hn.comments(),1).value("text").toString(),QString("[removed]"));
            QVERIFY(row(hn.comments()).value("text").toString().contains("\n"));
            hn.toggleReplies("200");QTRY_VERIFY(!hn.commentsBusy());QCOMPARE(hn.comments()->rowCount(),11);QCOMPARE(row(hn.comments(),1).value("id").toString(),QString("400"));QVERIFY(first.isValid());QCOMPARE(first.row(),0);
            hn.toggleReplies("400");QCOMPARE(hn.comments()->rowCount(),11); // A cycle cannot duplicate a parent.
            hn.toggleReplies("200");QCOMPARE(hn.comments()->rowCount(),10);QVERIFY(first.isValid());
            hn.loadMoreComments();QTRY_VERIFY(!hn.commentsBusy());QCOMPARE(hn.comments()->rowCount(),12);QVERIFY(!hn.moreComments());
            hn.openStory(1);hn.closeStory();QTest::qWait(100);QVERIFY(hn.selected().isEmpty());QCOMPARE(hn.comments()->rowCount(),0);
        }
        offline=true;HackerNewsClient saved(base);QCOMPARE(saved.items()->rowCount(),2);saved.openStory(0);QVERIFY(!saved.commentsBusy());QCOMPARE(saved.comments()->rowCount(),12);
        saved.reloadComments();QTRY_VERIFY(!saved.commentsBusy());QCOMPARE(saved.comments()->rowCount(),12);QVERIFY(!saved.commentsProblem().isEmpty());
    }
    void modelInsertionsPreserveExistingIndices(){
        NewsEntries model;QAbstractItemModelTester tester(&model,QAbstractItemModelTester::FailureReportingMode::QtTest);
        const QVariantMap a{{"id","a"}},b{{"id","b"}},reply{{"id","reply"}};model.update({a,b});QPersistentModelIndex index(model.index(1,0));
        model.update({a,reply,b});QVERIFY(index.isValid());QCOMPARE(index.row(),2);model.update({a,b});QVERIFY(index.isValid());QCOMPARE(index.row(),1);
    }
};
QTEST_MAIN(NewsTests)
#include "news-tests.moc"
