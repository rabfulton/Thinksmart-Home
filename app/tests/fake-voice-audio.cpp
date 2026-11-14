#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
static void send(QJsonObject object){const auto b=QJsonDocument(object).toJson(QJsonDocument::Compact)+'\n';fwrite(b.data(),1,size_t(b.size()),stdout);fflush(stdout);}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);QByteArray buffer;QString item;int index=0,ticks=0;bool playing=false;
    fcntl(0,F_SETFL,fcntl(0,F_GETFL)|O_NONBLOCK);
    QSocketNotifier input(0,QSocketNotifier::Read);
    QObject::connect(&input,&QSocketNotifier::activated,&app,[&](){
        char b[8192];auto n=read(0,b,sizeof(b));if(n<=0){app.quit();return;}buffer.append(b,n);
        while(buffer.contains('\n')){
            auto end=buffer.indexOf('\n');auto o=QJsonDocument::fromJson(buffer.left(end)).object();buffer.remove(0,end+1);
            if(o.value("type")=="audio"){item=o.value("item").toString();index=o.value("index").toInt();playing=true;QTimer::singleShot(350,&app,[&](){playing=false;});}
            if(o.value("type")=="flush"){playing=false;send({{"event","flushed"},{"request",o.value("request")},{"played",QJsonArray{QJsonObject{{"item",item},{"index",index},{"ms",137}}}}});}
        }
    });
    QTimer timer;timer.setInterval(20);
    QObject::connect(&timer,&QTimer::timeout,&app,[&](){const bool quiet=qEnvironmentVariableIntValue("THINKSMART_VOICE_TEST_SILENT");send({{"event","mic"},{"audio",QString::fromLatin1(QByteArray(960,'\0').toBase64())}});send({{"event","meter"},{"cleanDbfs",quiet?-120:-30},{"speakerDbfs",playing&&!quiet?-25:-120}});if(++ticks%5==0)send({{"event","levels"},{"cleanDbfs",quiet?-120:-30},{"speakerDbfs",playing&&!quiet?-25:-120},{"playing",playing}});});
    send({{"event","started"}});timer.start();return app.exec();
}
