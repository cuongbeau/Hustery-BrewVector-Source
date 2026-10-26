#pragma once
#include <stdint.h>
#include <math.h>
// Main-owned, volatile next-shot setting; independent of UI and persistent profiles.
struct QuickYieldSession {
  uint32_t editor=0, deadline=0;
  float draft=0, pending=NAN, running=NAN;
  constexpr void cancel(){editor=0;}
  constexpr void reset(){editor=0;pending=NAN;}
  constexpr void poll(bool allowed,uint32_t now){
    if(!allowed)reset();
    if(editor && static_cast<int32_t>(now-deadline)>=0)cancel();
  }
  constexpr bool command(uint8_t op,uint32_t id,float baseline,uint32_t now){
    if(!id)return false;
    if(op==0){
      if(editor && editor!=id)return false;
      if(!editor){draft=__builtin_isfinite(pending)?pending:baseline;editor=id;}
      deadline=now+2000;return true;
    }
    if(editor!=id)return false;
    if(op==1){deadline=now+2000;return true;}
    if(op==2){pending=draft;cancel();return true;}
    if(op==3){cancel();return true;}
    return false;
  }
  constexpr void rotate(int steps){
    if(!editor)return;
    const float next=draft+steps;draft=next<0?0:next>500?500:next;
  }
  constexpr void consume(bool allowed){running=allowed?pending:NAN;reset();}
};
