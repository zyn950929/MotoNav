#include <cassert>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>
#include "HomeClock-pages4.h"

int main() {
    std::ifstream input("/tmp/motonav-clock-wires.txt");
    assert(input.good());
    HomeClock clock;
    std::string line;
    int number=0;
    while(std::getline(input,line)) {
        // Reproduce a lost initial notification/write: only the fresh retries arrive.
        if(number++==0)continue;
        assert(clock.receive(line.c_str(),100));
        long long epoch=0;int offset=0;
        assert(std::sscanf(line.c_str(),"#TIME,%lld,%d",&epoch,&offset)==2);
        std::time_t local=static_cast<std::time_t>(epoch+offset);
        char expectedTime[6],expectedDate[11],actualTime[6],actualDate[11];
        auto date=std::gmtime(&local);
        assert(date);
        std::strftime(expectedTime,sizeof(expectedTime),"%H:%M",date);
        std::strftime(expectedDate,sizeof(expectedDate),"%Y-%m-%d",date);
        assert(clock.format(100,actualTime,actualDate));
        assert(std::string(actualTime)==expectedTime && std::string(actualDate)==expectedDate);
    }
    assert(number==3 && clock.synced());
    std::cout<<"PASS iOS clock retries accepted by production pages.4 HomeClock with a lost initial message\n";
}
