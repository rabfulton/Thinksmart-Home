#include "personal.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QLockFile>
#include <QTimer>
#include <cstdio>

// Independent of the graphical session and voice APIs. Only the dashboard's
// bounded action vocabulary can run; saved files never become shell commands.
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    app.setOrganizationName("ThinkSmart");app.setApplicationName("Home");
    PersonalStore store;
    QLockFile singleton(PersonalStore::directory()+"/scheduler.lock");
    if(!singleton.tryLock())return 1;
    QTimer timer;timer.setSingleShot(true);
    auto nextDue=[&](){
        QDateTime next;
        for(const auto &v:store.schedules()){
            const auto job=v.toMap();if(!job.value("enabled").toBool())continue;
            const auto at=QDateTime::fromString(job.value("nextAt").toString(),Qt::ISODateWithMs);
            if(at.isValid()&&(!next.isValid()||at<next))next=at;
        }
        return next;
    };
    auto reschedule=[&](){
        const auto next=nextDue();
        if(!next.isValid()){timer.stop();return;}
        // No polling at all with no schedules. With future jobs, check the
        // wall clock at least once a minute in case network time changes.
        timer.start(int(qBound(qint64(100),QDateTime::currentDateTimeUtc().msecsTo(next),qint64(60000))));
    };
    auto tick=[&](){
        const auto now=QDateTime::currentDateTimeUtc(),next=nextDue();
        if(!next.isValid()||next>now){reschedule();return;}
        const auto path=qEnvironmentVariable("THINKSMART_CONTROL_SOCKET","/run/thinksmart-ui/control.sock");
        QLocalSocket probe;probe.connectToServer(path);
        // Leave pending jobs untouched during a short dashboard restart.
        if(!probe.waitForConnected(150)){store.claimDue(now,false);reschedule();if(timer.isActive()&&timer.remainingTime()<1000)timer.start(1000);return;}
        probe.disconnectFromServer();
        for(const auto &v:store.claimDue()){
            const auto job=v.toObject();QLocalSocket socket;socket.connectToServer(path);
            QJsonObject result{{"ok",false},{"error","Dashboard unavailable; event was not retried."}};
            if(socket.waitForConnected(1000)){
                socket.write(QJsonDocument(QJsonObject{{"action","automation.execute"},{"steps",job.value("steps")}}).toJson(QJsonDocument::Compact)+'\n');socket.waitForBytesWritten(1000);
                QByteArray bytes;QElapsedTimer deadline;deadline.start();
                while(!bytes.contains('\n')&&bytes.size()<256*1024&&deadline.elapsed()<3000){
                    if(socket.bytesAvailable()==0&&!socket.waitForReadyRead(qMax(1,3000-int(deadline.elapsed()))))break;
                    bytes+=socket.readAll();
                }
                const auto reply=QJsonDocument::fromJson(bytes.trimmed());if(reply.isObject())result=reply.object();
            }
            store.request("schedule.result",{{"id",job.value("id")},{"lastRun",job.value("lastRun")},{"ok",result.value("ok").toBool()},
                {"message",result.value("ok").toBool()?QString("Actions requested successfully."):result.value("error").toString("Action request failed.")}});
        }
        reschedule();
    };
    QObject::connect(&timer,&QTimer::timeout,&app,tick);
    QObject::connect(&store,&PersonalStore::changed,&app,reschedule);
    reschedule();
    return app.exec();
}
