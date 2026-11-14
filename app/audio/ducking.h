#pragma once
#include <cmath>
// Called once per 10 ms control tick. Fast attenuation, a gentler return.
class DuckEnvelope {
    float current=1.f,target=1.f;
public:
    void setDucked(bool ducked){target=ducked?.12f:1.f;}
    float gain()const{return current;}
    bool advance(){
        if(current==target)return false;
        current+=(target-current)*(target<current?.4f:.08f);
        if(std::abs(target-current)<.001f)current=target;
        return true;
    }
};
