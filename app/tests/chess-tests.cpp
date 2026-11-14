#include "../chessgame.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <memory>

class ChessTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString engine,path;
    int index(const QString &square){return (7-(square[1].unicode()-'1'))*8+square[0].unicode()-'a';}
    QVariantMap cell(const ChessGame &game,const QString &square){return game.squares()[index(square)].toMap();}
    void move(ChessGame &game,const QString &from,const QString &to){game.selectSquare(index(from));game.selectSquare(index(to));}
    void seed(const QString &fen,const QStringList &moves={}){QFile f(path);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(QJsonObject{{"version",1},{"startFen",fen},{"moves",QJsonArray::fromStringList(moves)}}).toJson());}
private slots:
    void initTestCase(){QVERIFY(directory.isValid());engine=QCoreApplication::applicationDirPath()+"/fake-chess-engine";path=directory.filePath("game.json");}
    void init(){QFile::remove(path);for(auto key:{"THINKSMART_CHESS_TEST_DELAY_MS","THINKSMART_CHESS_TEST_REPLY","THINKSMART_CHESS_TEST_BAD_ELO","THINKSMART_CHESS_TEST_LOG"})qunsetenv(key);}
    void whiteInputLegalMovesAndRatedBlackReply(){
        const auto log=directory.filePath("uci.log");QFile::remove(log);qputenv("THINKSMART_CHESS_TEST_LOG",log.toUtf8());
        ChessGame game(engine,path);QCOMPARE(game.squares().size(),64);QCOMPARE(cell(game,"a8").value("piece").toString(),QString("bR"));QCOMPARE(cell(game,"e1").value("piece").toString(),QString("wK"));
        game.setActive(true);game.selectSquare(index("e7"));QVERIFY(!cell(game,"e7").value("selected").toBool());
        game.selectSquare(index("e2"));QVERIFY(cell(game,"e3").value("legal").toBool());QVERIFY(cell(game,"e4").value("legal").toBool());QVERIFY(!cell(game,"e5").value("legal").toBool());
        game.selectSquare(index("e5"));QCOMPARE(game.moveCount(),0);move(game,"e2","e4");QVERIFY(game.busy());QTRY_COMPARE(game.moveCount(),2);QVERIFY(game.error().isEmpty());QCOMPARE(game.rating(),1200);
        QCOMPARE(cell(game,"e4").value("piece").toString(),QString("wP"));QCOMPARE(cell(game,"e5").value("piece").toString(),QString("bP"));QCOMPARE(game.status(),QString("Your move"));
        QFile f(log);QVERIFY(f.open(QIODevice::ReadOnly));auto commands=f.readAll();QVERIFY(commands.contains("setoption name UCI_Elo value 1200"));QVERIFY(commands.contains("setoption name UCI_LimitStrength value true"));QVERIFY(commands.contains("setoption name Threads value 1"));QVERIFY(commands.contains("setoption name Hash value 16"));
    }
    void undoCancelsStaleRepliesAndTakesBackWholeTurn(){
        qputenv("THINKSMART_CHESS_TEST_DELAY_MS","300");ChessGame game(engine,path);game.setActive(true);const auto start=game.fen();move(game,"e2","e4");game.undo();QCOMPARE(game.moveCount(),0);QCOMPARE(game.fen(),start);QTest::qWait(400);QCOMPARE(game.moveCount(),0);
        move(game,"e2","e4");QTRY_COMPARE(game.moveCount(),2);const auto afterFirstTurn=game.fen();move(game,"g1","f3");QTRY_COMPARE(game.moveCount(),4);game.undo();QCOMPARE(game.moveCount(),2);QCOMPARE(game.fen(),afterFirstTurn);
    }
    void leavingPageStopsEngineAndResumeContinuesBlackTurn(){
        qputenv("THINKSMART_CHESS_TEST_DELAY_MS","250");ChessGame game(engine,path);game.setActive(true);move(game,"e2","e4");game.setActive(false);QVERIFY(!game.busy());QTest::qWait(350);QCOMPARE(game.moveCount(),1);
        game.setActive(true);QTRY_COMPARE(game.moveCount(),2);QVERIFY(game.error().isEmpty());
    }
    void gamePersistsWithPrivatePermissions(){
        QString fen;{
            ChessGame game(engine,path);game.setActive(true);move(game,"e2","e4");QTRY_COMPARE(game.moveCount(),2);fen=game.fen();
        }
        ChessGame restored(engine,path);QCOMPARE(restored.moveCount(),2);QCOMPARE(restored.fen(),fen);QVERIFY(!restored.active());QVERIFY(!(QFile::permissions(path)&(QFile::ReadGroup|QFile::ReadOther)));
        seed("not a chess position");ChessGame invalid(engine,path);QCOMPARE(invalid.moveCount(),0);QVERIFY(!invalid.error().isEmpty());
    }
    void castlingAndEnPassantMapToVisibleSquares(){
        seed("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");{
            ChessGame game(engine,path);game.setActive(true);game.selectSquare(index("e1"));QVERIFY(cell(game,"g1").value("legal").toBool());game.selectSquare(index("g1"));game.setActive(false);
            QCOMPARE(cell(game,"g1").value("piece").toString(),QString("wK"));QCOMPARE(cell(game,"f1").value("piece").toString(),QString("wR"));QVERIFY(cell(game,"h1").value("piece").toString().isEmpty());
        }
        seed("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");ChessGame game(engine,path);game.setActive(true);move(game,"e5","d6");game.setActive(false);QCOMPARE(cell(game,"d6").value("piece").toString(),QString("wP"));QVERIFY(cell(game,"d5").value("piece").toString().isEmpty());
    }
    void promotionRequiresChoiceAndSupportsUnderpromotion(){
        seed("7k/P7/8/8/8/8/8/7K w - - 0 1");ChessGame game(engine,path);game.setActive(true);move(game,"a7","a8");QVERIFY(game.promotionPending());QCOMPARE(game.moveCount(),0);
        game.promote("invalid");QVERIFY(game.promotionPending());game.promote("n");QVERIFY(!game.promotionPending());QCOMPARE(cell(game,"a8").value("piece").toString(),QString("wN"));QVERIFY(game.status().startsWith("Draw"));
    }
    void checkmateStalemateAndRepetition(){
        seed("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");{ChessGame game(engine,path);game.setActive(true);QCOMPARE(game.status(),QString("Checkmate · Black wins"));QVERIFY(cell(game,"e1").value("check").toBool());move(game,"e2","e3");QCOMPARE(game.moveCount(),0);}
        seed("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");{ChessGame game(engine,path);game.setActive(true);QCOMPARE(game.status(),QString("Draw · stalemate"));QVERIFY(!game.busy());}
        seed("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",{"g1f3","g8f6","f3g1","f6g8","g1f3","g8f6","f3g1","f6g8"});ChessGame game(engine,path);QCOMPARE(game.status(),QString("Draw · repetition"));
    }
    void illegalEngineMoveAndUnsupportedRatingAreRejected(){
        qputenv("THINKSMART_CHESS_TEST_REPLY","a1a2");ChessGame game(engine,path);game.setActive(true);move(game,"e2","e4");QTRY_VERIFY(!game.busy());QCOMPARE(game.moveCount(),1);QVERIFY(!game.error().isEmpty());
        qunsetenv("THINKSMART_CHESS_TEST_REPLY");game.retry();QTRY_COMPARE(game.moveCount(),2);game.newGame();qputenv("THINKSMART_CHESS_TEST_BAD_ELO","1");move(game,"e2","e4");QTRY_VERIFY(!game.busy());QCOMPARE(game.moveCount(),1);QVERIFY(game.error().contains("1200"));
    }
    void realEnginePlaysBlackAt1200(){
        const auto binary=qEnvironmentVariable("THINKSMART_REAL_CHESS_ENGINE");if(binary.isEmpty())QSKIP("Set THINKSMART_REAL_CHESS_ENGINE for an offline real-engine check");
        ChessGame game(binary,path);game.setActive(true);move(game,"e2","e4");QTRY_VERIFY_WITH_TIMEOUT(!game.busy(),20000);QVERIFY2(game.error().isEmpty(),qPrintable(game.error()));QCOMPARE(game.moveCount(),2);QCOMPARE(game.status(),QString("Your move"));
    }
    void switchingSidesKeepsPositionAndEnginePlaysWhite(){
        ChessGame game(engine,path);const auto start=game.fen();game.switchSides();QVERIFY(!game.playerWhite());QCOMPARE(game.fen(),start);game.setActive(true);
        QTRY_COMPARE(game.moveCount(),1);QVERIFY(game.error().isEmpty());QCOMPARE(game.status(),QString("Your move"));QVERIFY(!game.canUndo());
        move(game,"e7","e5");QTRY_COMPARE(game.moveCount(),3);QVERIFY(game.canUndo());game.undo();QCOMPARE(game.moveCount(),1);QVERIFY(cell(game,"e7").value("piece").toString()=="bP");
        const auto position=game.fen();game.switchSides();game.setActive(false);QVERIFY(game.playerWhite());QCOMPARE(game.fen(),position);QCOMPARE(game.moveCount(),1);
    }
    void ratingChangesRestartSearchAndPreferencesPersist(){
        qputenv("THINKSMART_CHESS_TEST_DELAY_MS","250");const auto log=directory.filePath("rating.log");QFile::remove(log);qputenv("THINKSMART_CHESS_TEST_LOG",log.toUtf8());
        {
            ChessGame game(engine,path);game.setActive(true);move(game,"e2","e4");QVERIFY(game.busy());QTest::qWait(80);game.setRating(1800);QTRY_COMPARE(game.moveCount(),2);QVERIFY(game.error().isEmpty());
            QFile f(log);QVERIFY(f.open(QIODevice::ReadOnly));QVERIFY(f.readAll().contains("setoption name UCI_Elo value 1800"));
            game.setRating(500);QCOMPARE(game.rating(),1800);game.setRating(2500);QCOMPARE(game.rating(),1800);game.setRating(2400);QCOMPARE(game.rating(),2400);game.setRating(1800);
            game.setActive(false);game.switchSides();game.newGame();QVERIFY(!game.playerWhite());QCOMPARE(game.rating(),1800);QCOMPARE(game.moveCount(),0);
        }
        ChessGame restored(engine,path);QVERIFY(!restored.playerWhite());QCOMPARE(restored.rating(),1800);restored.setActive(true);QTRY_COMPARE(restored.moveCount(),1);QVERIFY(restored.error().isEmpty());
    }
    void realEnginePlaysWhiteAtAdjustedRating(){
        const auto binary=qEnvironmentVariable("THINKSMART_REAL_CHESS_ENGINE");if(binary.isEmpty())QSKIP("Set THINKSMART_REAL_CHESS_ENGINE for an offline real-engine check");
        ChessGame game(binary,path);game.setRating(1500);game.switchSides();game.setActive(true);QTRY_VERIFY_WITH_TIMEOUT(!game.busy(),20000);QVERIFY2(game.error().isEmpty(),qPrintable(game.error()));QCOMPARE(game.moveCount(),1);QCOMPARE(game.status(),QString("Your move"));QVERIFY(!game.playerWhite());
    }
    void switchingDuringSearchHandsTheTurnToThePlayer(){
        qputenv("THINKSMART_CHESS_TEST_DELAY_MS","250");ChessGame game(engine,path);game.setActive(true);move(game,"e2","e4");QTest::qWait(80);
        game.switchSides();QVERIFY(!game.playerWhite());QVERIFY(!game.busy());QTest::qWait(350);QCOMPARE(game.moveCount(),1);QCOMPARE(game.status(),QString("Your move"));
        move(game,"e7","e5");QTRY_COMPARE(game.moveCount(),3);QVERIFY(game.error().isEmpty());
    }
    void blackCanPromoteAndOldRatingsAreCapped(){
        seed("7k/8/8/8/8/8/p7/7K b - - 0 1");
        {ChessGame game(engine,path);game.switchSides();game.setActive(true);move(game,"a2","a1");QVERIFY(game.promotionPending());game.promote("n");QCOMPARE(cell(game,"a1").value("piece").toString(),QString("bN"));}
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));auto data=QJsonDocument::fromJson(f.readAll()).object();f.close();data.insert("rating",3000);QVERIFY(f.open(QIODevice::WriteOnly));f.write(QJsonDocument(data).toJson());f.close();
        ChessGame restored(engine,path);QCOMPARE(restored.rating(),2400);QVERIFY(!restored.playerWhite());
    }
};
QTEST_GUILESS_MAIN(ChessTests)
#include "chess-tests.moc"
