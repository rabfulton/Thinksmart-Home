#pragma once
#include <QObject>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QVariantList>

// Plain-text personal data shared by the dashboard and the local scheduler.
// All mutations are locked and committed atomically; no stored content is code.
class PersonalStore:public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList notes READ notes NOTIFY changed)
    Q_PROPERTY(QVariantList routines READ routines NOTIFY changed)
    Q_PROPERTY(QVariantList schedules READ schedules NOTIFY changed)
    Q_PROPERTY(QStringList themeNames READ themeNames NOTIFY changed)
    Q_PROPERTY(QString directory READ directory CONSTANT)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
public:
    explicit PersonalStore(QObject *parent=nullptr);
    static QString directory();
    static QStringList actions();
    static QString instructions();
    QVariantList notes()const;
    QVariantList routines()const;
    QVariantList schedules()const;
    QStringList themeNames()const;
    QString problem()const{return m_problem;}
    QJsonObject palette(const QString &id)const;
    QJsonObject request(const QString &action,const QJsonObject &data={},const QDateTime &now=QDateTime::currentDateTimeUtc());
    QJsonArray claimDue(const QDateTime &now=QDateTime::currentDateTimeUtc(),bool dashboardAvailable=true);
    static QDateTime nextOccurrence(const QJsonObject &schedule,const QDateTime &after);
    static QString validateSteps(const QJsonArray &steps);
    QJsonArray routineSteps(const QString &id)const;
    Q_INVOKABLE void reload();
signals:
    void changed();
private:
    QJsonArray readItems(const QString &kind,QString *error=nullptr)const;
    bool writeItems(const QString &kind,const QJsonArray &items);
    bool writeJson(const QString &path,const QJsonObject &object);
    QJsonObject readTheme(const QString &id,bool *valid=nullptr)const;
    QVariantList rows(const QString &kind)const;
    QFileSystemWatcher m_watch;
    QString m_problem;
    QString m_signature;
};
