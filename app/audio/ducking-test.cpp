#include "ducking.h"
#include <cstdio>
int main(){
    auto require=[](bool ok,const char *message){if(!ok)std::fprintf(stderr,"%s\n",message);return ok;};
    DuckEnvelope gain;gain.setDucked(true);float previous=1;
    for(int tick=0;tick<20;tick++){gain.advance();if(!require(gain.gain()<=previous&&gain.gain()>=.12f,"Attenuation overshot"))return 1;previous=gain.gain();}
    if(!require(gain.gain()==.12f,"Attenuation did not settle promptly"))return 1;
    gain.setDucked(false);gain.advance();if(!require(gain.gain()>.12f&&gain.gain()<.25f,"Restore jumped instead of fading"))return 1;
    previous=gain.gain();for(int tick=0;tick<90;tick++){gain.advance();if(!require(gain.gain()>=previous&&gain.gain()<=1.f,"Restore overshot"))return 1;previous=gain.gain();}
    if(!require(gain.gain()==1.f,"Restore did not finish within one second"))return 1;
    gain.setDucked(true);gain.advance();gain.setDucked(false);gain.advance();gain.setDucked(true);for(int tick=0;tick<20;tick++)gain.advance();
    return require(gain.gain()==.12f,"Rapid turn changes did not settle")?0:1;
}
