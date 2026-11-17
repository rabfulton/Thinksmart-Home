#pragma once
#include <QObject>
#include <QJsonObject>
#include <QVariantList>
class LayoutEditor:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList pages READ pages NOTIFY changed)
    Q_PROPERTY(QVariantMap modules READ modules NOTIFY changed)
    Q_PROPERTY(QVariantList catalogue READ catalogue CONSTANT)
    Q_PROPERTY(QVariantList widgets READ widgets CONSTANT)
    Q_PROPERTY(QVariantList fullPages READ fullPages CONSTANT)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(bool dirty READ dirty NOTIFY changed)
public:
    explicit LayoutEditor(QObject *parent=nullptr);
    QVariantList pages()const;
    QVariantMap modules()const;
    QVariantList catalogue()const;
    QVariantList widgets()const;
    QVariantList fullPages()const;
    QString problem()const{return m_problem;}
    bool dirty()const{return m_draft!=m_initial;}
    Q_INVOKABLE void begin();
    Q_INVOKABLE void setModule(const QString &id,bool enabled);
    Q_INVOKABLE void addPage(const QString &module=QString());
    Q_INVOKABLE void renamePage(int index,const QString &title);
    Q_INVOKABLE void removePage(int index);
    Q_INVOKABLE void movePage(int index,int direction);
    Q_INVOKABLE void addWidget(int page,const QString &widget);
    Q_INVOKABLE void removeWidget(int page,int index);
    Q_INVOKABLE void moveWidget(int page,int index,int direction);
    Q_INVOKABLE bool apply();
signals:
    void changed();
    void restartRequested();
private:
    bool update(const QJsonObject &draft);
    QJsonObject m_initial,m_draft;
    QString m_problem;
};
