#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
static void send(QJsonObject object){const auto b=QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n';fwrite(b.data(),1,size_t(b.size()),stdout);fflush(stdout);}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QTimer poll;poll.setInterval(30);
    QObject::connect(&poll,&QTimer::timeout,&app,[&](){
        if(qEnvironmentVariableIntValue("THINKSMART_SHARED_AUDIO"))send({{"event","health"},{"cleanDbfs",qEnvironmentVariableIntValue("THINKSMART_VOICE_TEST_SILENT")?-120:-25}});
        QFile trigger(qEnvironmentVariable("THINKSMART_WAKE_TEST_TRIGGER"));
        if(trigger.open(QIODevice::ReadOnly)&&trigger.readAll().trimmed()=="Sheila"){
            trigger.close();QFile::remove(trigger.fileName());
            send({{"event","wake"},{"word","Sheila"}});if(!qEnvironmentVariableIntValue("THINKSMART_SHARED_AUDIO"))app.quit();
        }
    });
    send({{"event","started"},{"local",true}});poll.start();return app.exec();
}
