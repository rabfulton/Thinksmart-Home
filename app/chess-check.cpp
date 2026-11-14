// One isolated, offline engine turn. Never reads or modifies the user's saved game.
#include "chessgame.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTemporaryDir directory;if(!directory.isValid())return 2;
    const QString engine=argc>1?QString::fromLocal8Bit(argv[1]):QString("/usr/local/libexec/thinksmart-arasan/arasanx");
    ChessGame game(engine,directory.filePath("game.json"));const auto original=game.fen();QElapsedTimer elapsed;elapsed.start();bool finished=false;
    auto report=[&](const QJsonObject &o,int code){if(finished)return;finished=true;const auto bytes=QJsonDocument(o).toJson(QJsonDocument::Compact);std::printf("%s\n",bytes.constData());std::fflush(stdout);QTimer::singleShot(200,&app,[&,code](){app.exit(code);});};
    QObject::connect(&game,&ChessGame::changed,&app,[&](){
        if(finished)return;if(!game.error().isEmpty()){report({{"error",game.error()}},1);return;}
        if(game.moveCount()==2&&!game.busy()){
            const auto last=game.lastMove();finished=true;game.setActive(false);game.undo();const bool restored=game.moveCount()==0&&game.fen()==original;finished=false;
            report({{"engine","Arasan"},{"rating",game.rating()},{"reply",last},{"elapsedMs",elapsed.elapsed()},{"undoRestored",restored}},restored?0:3);
        }
    });
    QTimer::singleShot(0,&app,[&](){game.setActive(true);game.selectSquare(52);game.selectSquare(36);});
    QTimer::singleShot(20000,&app,[&](){report({{"error","Engine check timed out"}},4);});return app.exec();
}
