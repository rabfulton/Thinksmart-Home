#pragma once
// Called once per aligned 10 ms microphone/reference block. Keep the filter's
// learned room response, but stop processing after the speaker's echo has faded.
class IdleEchoPolicy {
    int remaining=0;
public:
    bool process(double speakerPower,bool conversation){
        if(speakerPower>1e-8)remaining=300;
        const bool active=conversation||remaining>0;
        if(remaining>0)--remaining;
        return active;
    }
};

