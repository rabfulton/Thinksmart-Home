#pragma once
#include <QObject>
#include <QVariantList>
#include <QProcess>
#include <QTimer>
#include <QStringList>
#include <QSet>
#include <memory>

class ChessGame : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList squares READ squares NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString lastMove READ lastMove NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY changed)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY changed)
    Q_PROPERTY(bool promotionPending READ promotionPending NOTIFY changed)
    Q_PROPERTY(int moveCount READ moveCount NOTIFY changed)
    Q_PROPERTY(int rating READ rating WRITE setRating NOTIFY changed)
    Q_PROPERTY(bool playerWhite READ playerWhite NOTIFY changed)
public:
    explicit ChessGame(QObject *parent=nullptr);
    ChessGame(const QString &engine,const QString &savePath,QObject *parent=nullptr);
    ~ChessGame()override;
    QVariantList squares()const;
    QString status()const;
    QString error()const{return m_error;}
    QString lastMove()const{return m_lastMove;}
    bool busy()const{return m_busy;}
    bool active()const{return m_active;}
    void setActive(bool active);
    bool canUndo()const;
    bool promotionPending()const{return m_promotionTo>=0;}
    int moveCount()const{return m_moves.size();}
    int rating()const{return m_rating;}
    void setRating(int rating);
    bool playerWhite()const{return m_playerWhite;}
    QString fen()const;
    Q_INVOKABLE void selectSquare(int index);
    Q_INVOKABLE void promote(const QString &piece);
    Q_INVOKABLE void cancelPromotion();
    Q_INVOKABLE void newGame();
    Q_INVOKABLE void undo();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void switchSides();
signals:
    void changed();
private:
    struct State;
    std::unique_ptr<State> m_state;
    void restore();
    void save();
    bool rebuild(const QString &start,const QStringList &moves);
    bool apply(const QString &uci);
    void requestReply();
    void stopEngine();
    void readEngine(quint64 generation);
    void fail(const QString &message);
    bool playerTurn()const;
    QString m_enginePath,m_savePath,m_startFen,m_error,m_lastMove;
    QStringList m_moves;
    QProcess *m_engine=nullptr;
    QTimer m_timeout;
    QByteArray m_buffer;
    QSet<QString> m_options;
    quint64 m_generation=0;
    int m_selected=-1,m_promotionTo=-1,m_eloMin=0,m_eloMax=0,m_phase=0,m_rating=1200;
    bool m_active=false,m_busy=false,m_playerWhite=true;
};
