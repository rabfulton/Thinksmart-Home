// A small root service with a fixed command set. No shell or client-supplied paths.
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <poll.h>
#include <pwd.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

namespace {
std::string brightnessPath="/sys/class/backlight/backlight/brightness";
std::string maximumPath="/sys/class/backlight/backlight/max_brightness";
std::string testDirectory;
uid_t appUid;
int maximum=4095;

bool readInteger(const std::string &path,int &value) {
    std::ifstream file(path);
    return bool(file>>value);
}
bool writeValue(const std::string &path,const std::string &value) {
    int fd=open(path.c_str(),O_WRONLY|O_TRUNC|O_CLOEXEC|O_NOFOLLOW);
    if(fd<0)return false;
    const auto count=write(fd,value.data(),value.size());
    const int saved=errno;
    close(fd);errno=saved;
    return count==static_cast<ssize_t>(value.size());
}
std::string failure(const char *message) {
    return std::string("{\"ok\":false,\"error\":\"")+message+"\"}\n";
}
bool systemAction(const std::string &action) {
    if(!testDirectory.empty())return writeValue(testDirectory+"/action",action+"\n");
    pid_t pid=fork();
    if(pid<0)return false;
    if(pid==0) {
        execl("/usr/bin/systemctl","systemctl","--no-block",action.c_str(),static_cast<char *>(nullptr));
        _exit(127);
    }
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    do {
        int status=0;
        const pid_t result=waitpid(pid,&status,WNOHANG);
        if(result==pid)return WIFEXITED(status)&&WEXITSTATUS(status)==0;
        if(result<0&&errno!=EINTR)return false;
        usleep(10000);
    }while(std::chrono::steady_clock::now()<deadline);
    kill(pid,SIGKILL);waitpid(pid,nullptr,0);
    return false;
}
std::string process(const std::string &command) {
    if(command=="status") {
        int actual=-1;
        if(!readInteger(brightnessPath,actual))return failure("Cannot read backlight");
        return "{\"ok\":true,\"brightness\":"+std::to_string(actual)+",\"maximum\":"+std::to_string(maximum)+",\"test\":"+(testDirectory.empty()?"false":"true")+"}\n";
    }
    if(command.rfind("brightness ",0)==0) {
        const std::string number=command.substr(11);
        int percent=-1;
        const auto parsed=std::from_chars(number.data(),number.data()+number.size(),percent);
        if(parsed.ec!=std::errc()||parsed.ptr!=number.data()+number.size()||percent<0||percent>100)
            return failure("Brightness must be an integer from 0 to 100");
        const int raw=(maximum*percent+50)/100;
        if(!writeValue(brightnessPath,std::to_string(raw)+"\n"))return failure("Cannot set backlight");
        return "{\"ok\":true,\"brightness\":"+std::to_string(raw)+"}\n";
    }
    if(command=="halt"||command=="reboot") {
        if(!systemAction(command))return failure("System action failed");
        return "{\"ok\":true,\"action\":\""+command+"\"}\n";
    }
    return failure("Unknown command");
}
void handle(int client) {
    ucred credentials{};socklen_t size=sizeof(credentials);
    if(getsockopt(client,SOL_SOCKET,SO_PEERCRED,&credentials,&size)<0||
       (credentials.uid!=0&&credentials.uid!=appUid)) {
        auto response=failure("Access denied");send(client,response.data(),response.size(),MSG_NOSIGNAL);return;
    }
    std::string request;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
    while(std::chrono::steady_clock::now()<deadline) {
        pollfd descriptor{client,POLLIN,0};
        if(poll(&descriptor,1,100)<=0)continue;
        char buffer[129];const ssize_t count=recv(client,buffer,sizeof(buffer),MSG_DONTWAIT);
        if(count<=0)break;
        request.append(buffer,count);
        if(request.size()>128)break;
        if(request.find('\n')!=std::string::npos) {
            const bool oneLine=request.find('\n')==request.size()-1&&request.find('\0')==std::string::npos;
            auto response=oneLine?process(request.substr(0,request.size()-1)):failure("One command per connection required");
            send(client,response.data(),response.size(),MSG_NOSIGNAL);return;
        }
    }
    auto response=failure("Incomplete or oversized command");
    send(client,response.data(),response.size(),MSG_NOSIGNAL);
}
}

int main(int argc,char **argv) {
    std::string socketPath="/run/thinksmart-power/control.sock";
    if(argc==3&&std::string(argv[1])=="--test-directory") {
        testDirectory=argv[2];
        struct stat info{};
        if(stat(testDirectory.c_str(),&info)!=0||!S_ISDIR(info.st_mode)||info.st_uid!=getuid()||(info.st_mode&077)!=0)return 2;
        socketPath=testDirectory+"/control.sock";
        brightnessPath=testDirectory+"/brightness";
        maximumPath=testDirectory+"/maximum";
        appUid=getuid();
    }else {
        if(argc!=1||geteuid()!=0)return 2;
        passwd *account=getpwnam("thinksmart");
        if(!account)return 2;
        appUid=account->pw_uid;
    }
    if(!readInteger(maximumPath,maximum)||maximum<=0||maximum>1000000)return 2;
    int server=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
    if(server<0)return 1;
    sockaddr_un address{};address.sun_family=AF_UNIX;
    if(socketPath.size()>=sizeof(address.sun_path))return 2;
    std::memcpy(address.sun_path,socketPath.c_str(),socketPath.size()+1);
    unlink(socketPath.c_str());
    if(bind(server,reinterpret_cast<sockaddr *>(&address),sizeof(address))<0||chmod(socketPath.c_str(),0660)<0||listen(server,8)<0) {
        std::cerr<<"Power socket: "<<strerror(errno)<<'\n';return 1;
    }
    // The GUI owns the sleep state. Preserve brightness if this helper restarts.
    for(;;) {
        int client=accept4(server,nullptr,nullptr,SOCK_CLOEXEC);
        if(client<0) {if(errno==EINTR)continue;return 1;}
        handle(client);close(client);
    }
}
