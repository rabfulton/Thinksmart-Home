#pragma once
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QQuickTextDocument>
#include <QVariantMap>
#include <QVariantList>
#include <QColor>

class RecipeBook : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int index READ index NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(int fontSize READ fontSize NOTIFY settingsChanged)
    Q_PROPERTY(QString model READ model NOTIFY settingsChanged)
    Q_PROPERTY(QString purpose READ purpose NOTIFY settingsChanged)
    Q_PROPERTY(QVariantList models READ models NOTIFY settingsChanged)
public:
    explicit RecipeBook(QObject *parent=nullptr);
    ~RecipeBook() override;
    QVariantMap current()const;
    int count()const{return m_outputs.size();}
    int index()const{return m_index;}
    bool busy()const{return !m_reply.isNull();}
    QString problem()const{return m_problem;}
    int fontSize()const{return m_fontSize;}
    QString model()const{return m_model;}
    QString purpose()const{return m_purpose;}
    QVariantList models()const;
    Q_INVOKABLE void navigate(int direction);
    Q_INVOKABLE bool removeCurrent();
    Q_INVOKABLE bool configure(int fontSize,const QString &model,const QString &purpose);
    Q_INVOKABLE void render(QQuickTextDocument *document,const QString &markdown,int baseSize,const QColor &headingColor,const QColor &boldColor);
    Q_INVOKABLE bool toggleTask(QQuickTextDocument *document,int position,const QString &pane,const QString &id);
    QJsonObject request(const QJsonObject &request);
    static QString storagePath();
    static QString settingsPath();
    static QString promptPath();
signals:
    void changed();
    void settingsChanged();
    void openRequested();
private:
    bool store(const QJsonArray &outputs);
    QJsonObject add(const QJsonObject &content,const QString &id={});
    QJsonObject compose(const QString &request,const QString &sourceId);
    static bool validContent(const QJsonObject &content);
    QJsonArray m_outputs;
    int m_index=-1,m_fontSize=24;
    QString m_model="gpt-6-luna",m_purpose="recipe",m_problem,m_pendingId;
    bool m_healthy=true;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
};
