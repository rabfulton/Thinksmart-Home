#include "deviceconfig.h"
#include "chessgame.h"
#include "third_party/chess/chess.hpp"
#include <QStandardPaths>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <sys/prctl.h>
#include <unistd.h>
#include <csignal>

struct ChessGame::State {chess::Board board;};
namespace {
chess::Square square(int index){return chess::Square((7-index/8)*8+index%8);}
QString coordinate(int index){return QString::fromStdString(static_cast<std::string>(square(index)));}
QString uci(const chess::Move &move){return QString::fromStdString(chess::uci::moveToUci(move));}
bool validFen(const QString &fen){
    const auto fields=fen.split(' ',Qt::SkipEmptyParts);if(fields.size()!=6||!QStringList{"w","b"}.contains(fields[1]))return false;
    const auto ranks=fields[0].split('/');if(ranks.size()!=8||fields[0].count('K')!=1||fields[0].count('k')!=1)return false;
    for(const auto &rank:ranks){int count=0;for(auto c:rank){if(c>='1'&&c<='8')count+=c.unicode()-'0';else if(QString("prnbqkPRNBQK").contains(c))++count;else return false;}if(count!=8)return false;}
    if(!QRegularExpression("^(-|[KQkq]+)$").match(fields[2]).hasMatch()||!QRegularExpression("^(-|[a-h][36])$").match(fields[3]).hasMatch())return false;
    bool half=false,full=false;int h=fields[4].toInt(&half),f=fields[5].toInt(&full);return half&&full&&h>=0&&h<1000000&&f>0&&f<1000000;
}
}
ChessGame::ChessGame(QObject *parent):ChessGame(qEnvironmentVariable("THINKSMART_CHESS_ENGINE","/usr/local/libexec/thinksmart-arasan/arasanx"),QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)+"/chess/game.json",parent){}
ChessGame::ChessGame(const QString &engine,const QString &savePath,QObject *parent):QObject(parent),m_state(std::make_unique<State>()),m_enginePath(engine),m_savePath(savePath){
    m_startFen=QString::fromStdString(m_state->board.getFen());restore();m_timeout.setSingleShot(true);m_timeout.setInterval(15000);connect(&m_timeout,&QTimer::timeout,this,[this](){fail("The chess engine timed out. Try again.");});
}
ChessGame::~ChessGame(){stopEngine();}
QString ChessGame::fen()const{return QString::fromStdString(m_state->board.getFen());}
bool ChessGame::playerTurn()const{return m_state->board.sideToMove()==(m_playerWhite?chess::Color::WHITE:chess::Color::BLACK);}
bool ChessGame::canUndo()const{return !m_moves.isEmpty()&&(!playerTurn()||m_moves.size()>=2);}
void ChessGame::setRating(int rating){
    if(rating==m_rating||rating<1000||rating>2400)return;
    stopEngine();m_rating=rating;m_error.clear();save();emit changed();requestReply();
}
void ChessGame::switchSides(){
    stopEngine();m_playerWhite=!m_playerWhite;m_selected=-1;m_promotionTo=-1;m_error.clear();save();emit changed();requestReply();
}
QVariantList ChessGame::squares()const{
    QVariantList cells;QSet<QString> targets;chess::Movelist legal;
    if(m_selected>=0){chess::movegen::legalmoves(legal,m_state->board);for(auto move:legal){const auto text=uci(move);if(text.startsWith(coordinate(m_selected)))targets.insert(text.mid(2,2));}}
    const auto last=m_moves.isEmpty()?QString():m_moves.last();const bool check=m_state->board.inCheck();
    for(int i=0;i<64;i++){
        const auto sq=square(i);const auto piece=m_state->board.at(sq);QString asset;
        if(piece!=chess::Piece::NONE){const auto type=QString::fromStdString(static_cast<std::string>(piece.type())).toUpper();asset=(piece.color()==chess::Color::WHITE?"w":"b")+type;}
        const auto name=coordinate(i);
        cells.append(QVariantMap{{"piece",asset},{"square",name},{"selected",m_selected==i},{"legal",targets.contains(name)},
            {"last",!last.isEmpty()&&(last.left(2)==name||last.mid(2,2)==name)},{"check",check&&piece.type()==chess::PieceType::KING&&piece.color()==m_state->board.sideToMove()}});
    }
    return cells;
}
QString ChessGame::status()const{
    const auto result=m_state->board.isGameOver();
    if(result.first==chess::GameResultReason::CHECKMATE)return playerTurn()?(m_playerWhite?"Checkmate · Black wins":"Checkmate · White wins"):"Checkmate · You win";
    if(result.second==chess::GameResult::DRAW){
        switch(result.first){case chess::GameResultReason::STALEMATE:return "Draw · stalemate";case chess::GameResultReason::THREEFOLD_REPETITION:return "Draw · repetition";case chess::GameResultReason::FIFTY_MOVE_RULE:return "Draw · fifty-move rule";default:return "Draw · insufficient material";}
    }
    if(m_busy)return m_playerWhite?"Black is thinking…":"White is thinking…";
    if(playerTurn())return m_state->board.inCheck()?"Your move · check":"Your move";
    return m_playerWhite?"Black to move":"White to move";
}
void ChessGame::setActive(bool active){active=active&&DeviceConfig::enabled("chess");if(active==m_active)return;m_active=active;if(active)requestReply();else stopEngine();emit changed();}
bool ChessGame::rebuild(const QString &start,const QStringList &moves){
    if(!validFen(start)||moves.size()>2000)return false;chess::Board board;if(!board.setFen(start.toStdString()))return false;
    const auto wk=board.kingSq(chess::Color::WHITE),bk=board.kingSq(chess::Color::BLACK);
    if(qAbs(int(wk.file())-int(bk.file()))<=1&&qAbs(int(wk.rank())-int(bk.rank()))<=1)return false;
    QString last;
    for(const auto &text:moves){
        chess::Movelist legal;chess::movegen::legalmoves(legal,board);bool found=false;
        for(auto move:legal)if(uci(move)==text){last=(board.sideToMove()==chess::Color::WHITE?"White · ":"Black · ")+QString::fromStdString(chess::uci::moveToSan(board,move));board.makeMove(move);found=true;break;}
        if(!found)return false;
    }
    m_state->board=std::move(board);m_startFen=start;m_moves=moves;m_lastMove=last;m_selected=-1;m_promotionTo=-1;return true;
}
void ChessGame::restore(){
    QFile file(m_savePath);if(!file.open(QIODevice::ReadOnly)||file.size()>128*1024)return;const auto data=QJsonDocument::fromJson(file.readAll()).object();QStringList moves;
    m_playerWhite=data.value("playerWhite").toBool(true);const int rating=data.value("rating").toInt(1200);if(rating>=1000)m_rating=qMin(rating,2400);
    for(auto value:data.value("moves").toArray()){if(!value.isString())return;moves.append(value.toString());}
    if(data.value("version").toInt()==1&&!rebuild(data.value("startFen").toString(),moves))m_error="Saved game could not be restored. Start a new game.";
}
void ChessGame::save(){
    QDir().mkpath(QFileInfo(m_savePath).absolutePath());QSaveFile file(m_savePath);
    if(file.open(QIODevice::WriteOnly)){file.setPermissions(QFile::ReadOwner|QFile::WriteOwner);file.write(QJsonDocument(QJsonObject{{"version",1},{"startFen",m_startFen},{"moves",QJsonArray::fromStringList(m_moves)},{"playerWhite",m_playerWhite},{"rating",m_rating}}).toJson(QJsonDocument::Compact));file.commit();}
}
bool ChessGame::apply(const QString &text){
    chess::Movelist legal;chess::movegen::legalmoves(legal,m_state->board);
    for(auto move:legal)if(uci(move)==text){
        m_lastMove=(m_state->board.sideToMove()==chess::Color::WHITE?"White · ":"Black · ")+QString::fromStdString(chess::uci::moveToSan(m_state->board,move));
        m_state->board.makeMove(move);m_moves.append(text);m_selected=-1;m_promotionTo=-1;m_error.clear();save();emit changed();return true;
    }
    return false;
}
void ChessGame::selectSquare(int index){
    if(index<0||index>=64||!m_active||m_busy||promotionPending()||!playerTurn()||m_state->board.isGameOver().second!=chess::GameResult::NONE)return;
    if(m_selected==index){m_selected=-1;emit changed();return;}
    if(m_selected>=0){
        const auto prefix=coordinate(m_selected)+coordinate(index);chess::Movelist legal;chess::movegen::legalmoves(legal,m_state->board);
        for(auto move:legal)if(uci(move).startsWith(prefix)){
            if(move.typeOf()==chess::Move::PROMOTION){m_promotionTo=index;emit changed();return;}
            if(apply(uci(move)))requestReply();return;
        }
    }
    m_selected=m_state->board.at(square(index)).color()==(m_playerWhite?chess::Color::WHITE:chess::Color::BLACK)?index:-1;emit changed();
}
void ChessGame::promote(const QString &piece){
    if(!m_active||!promotionPending()||piece.size()!=1||!QString("qrbn").contains(piece))return;
    const auto text=coordinate(m_selected)+coordinate(m_promotionTo)+piece;if(apply(text))requestReply();
}
void ChessGame::cancelPromotion(){if(!promotionPending())return;m_promotionTo=-1;emit changed();}
void ChessGame::newGame(){stopEngine();m_state=std::make_unique<State>();m_startFen=fen();m_moves.clear();m_error.clear();m_lastMove.clear();m_selected=-1;m_promotionTo=-1;save();emit changed();requestReply();}
void ChessGame::undo(){
    if(!canUndo())return;stopEngine();auto moves=m_moves;int count=playerTurn()?2:1;while(count--&&!moves.isEmpty())moves.removeLast();
    if(rebuild(m_startFen,moves)){m_error.clear();save();emit changed();requestReply();}
}
void ChessGame::retry(){if(!m_busy)requestReply();}
void ChessGame::stopEngine(){
    ++m_generation;m_timeout.stop();m_busy=false;
    if(!m_engine)return;auto process=m_engine;m_engine=nullptr;process->disconnect(this);
    if(process->state()==QProcess::NotRunning)process->deleteLater();else{connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),process,&QObject::deleteLater);process->kill();}
}
void ChessGame::fail(const QString &message){stopEngine();m_error=message;emit changed();}
void ChessGame::requestReply(){
    if(!m_active||m_busy||promotionPending()||playerTurn()||m_state->board.isGameOver().second!=chess::GameResult::NONE)return;
    stopEngine();m_error.clear();m_buffer.clear();m_options.clear();m_eloMin=m_eloMax=0;m_phase=0;m_busy=true;emit changed();
    const auto generation=m_generation;m_engine=new QProcess(this);m_engine->setWorkingDirectory(QFileInfo(m_enginePath).absolutePath());
    m_engine->setChildProcessModifier([](){prctl(PR_SET_PDEATHSIG,SIGTERM);nice(5);});
    connect(m_engine,&QProcess::started,this,[this,generation](){if(generation==m_generation)m_engine->write("uci\n");});
    connect(m_engine,&QProcess::readyReadStandardOutput,this,[this,generation](){if(generation==m_generation)readEngine(generation);});
    connect(m_engine,&QProcess::readyReadStandardError,this,[this,generation](){if(generation==m_generation)m_engine->readAllStandardError();});
    connect(m_engine,&QProcess::errorOccurred,this,[this,generation](){if(generation==m_generation)fail("Cannot start the chess engine. Try again.");});
    connect(m_engine,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,generation](int,QProcess::ExitStatus){if(generation==m_generation&&m_busy)fail("The chess engine stopped. Try again.");});
    m_timeout.start();m_engine->start(m_enginePath,{});
}
void ChessGame::readEngine(quint64 generation){
    m_buffer+=m_engine->readAllStandardOutput();if(m_buffer.size()>1024*1024){fail("Invalid chess engine response.");return;}
    while(generation==m_generation&&m_buffer.contains('\n')){
        const auto end=m_buffer.indexOf('\n');const QString line=QString::fromUtf8(m_buffer.left(end)).trimmed();m_buffer.remove(0,end+1);
        if(line.startsWith("option name ")){
            const int type=line.indexOf(" type ");if(type>12){const auto name=line.mid(12,type-12);m_options.insert(name);
                if(name=="UCI_Elo"){auto match=QRegularExpression("min ([0-9]+) max ([0-9]+)").match(line);if(match.hasMatch()){m_eloMin=match.captured(1).toInt();m_eloMax=match.captured(2).toInt();}}
            }
        }else if(line=="uciok"&&m_phase==0){
            if(!m_options.contains("UCI_LimitStrength")||m_eloMin>rating()||m_eloMax<rating()){fail(QString("This engine cannot use the requested %1 rating.").arg(rating()));return;}
            auto option=[this](const QString &name,const QString &value){if(m_options.contains(name))m_engine->write(("setoption name "+name+" value "+value+"\n").toUtf8());};
            option("Threads","1");option("Hash","16");option("Ponder","false");option("OwnBook","false");option("Position learning","false");option("Use tablebases","false");option("UCI_LimitStrength","true");option("UCI_Elo",QString::number(rating()));
            m_phase=1;m_engine->write("ucinewgame\nisready\n");
        }else if(line=="readyok"&&m_phase==1){
            m_phase=2;QString position="position fen "+m_startFen;if(!m_moves.isEmpty())position+=" moves "+m_moves.join(' ');
            m_engine->write((position+"\ngo movetime 700\n").toUtf8());m_timeout.start();
        }else if(line.startsWith("bestmove ")&&m_phase==2){
            const auto move=line.section(' ',1,1);stopEngine();if(!apply(move)){fail("The engine returned an invalid move. Try again.");return;}emit changed();return;
        }
    }
}
