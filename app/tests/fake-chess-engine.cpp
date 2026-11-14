#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <sstream>
int main(){
    std::string line,position;
    while(std::getline(std::cin,line)){
        if(auto path=std::getenv("THINKSMART_CHESS_TEST_LOG")){std::ofstream log(path,std::ios::app);log<<line<<'\n';}
        if(line=="uci"){
            std::cout<<"id name Test engine\noption name Threads type spin default 1 min 1 max 8\noption name Hash type spin default 16 min 1 max 128\noption name Ponder type check default false\noption name OwnBook type check default false\noption name UCI_LimitStrength type check default false\noption name UCI_Elo type spin default 1200 min "<<(std::getenv("THINKSMART_CHESS_TEST_BAD_ELO")?1320:1000)<<" max 3450\nuciok"<<std::endl;
        }else if(line=="isready")std::cout<<"readyok"<<std::endl;
        else if(line.rfind("position ",0)==0)position=line;
        else if(line.rfind("go ",0)==0){
            int delay=20;if(auto value=std::getenv("THINKSMART_CHESS_TEST_DELAY_MS"))delay=std::atoi(value);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            const char *forced=std::getenv("THINKSMART_CHESS_TEST_REPLY");
            bool white=position.find(" w ")!=std::string::npos;std::string moves;
            const auto at=position.find(" moves ");if(at!=std::string::npos){moves=position.substr(at+7);std::istringstream input(moves);std::string move;while(input>>move)white=!white;}
            const auto reply=white?(moves.find("e2e4")==std::string::npos?"e2e4":"g1f3"):(moves.find("e7e5")==std::string::npos?"e7e5":"b8c6");
            std::cout<<"bestmove "<<(forced?forced:reply)<<std::endl;
        }else if(line=="quit")return 0;
    }
}
