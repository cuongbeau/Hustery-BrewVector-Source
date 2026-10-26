#include "ui/screen_manager.h"
#include "ui/components.h"
#include "ui/theme.h"
using namespace UiComponents;

void ScreenManager::handleHeaterTap(int x,int y) {
 if(y>=203 && y<235){
   if(x>=8 && x<80)heaterPage_=(heaterPage_+4)%5;
   else if(x>=88 && x<160)heaterPage_=(heaterPage_+1)%5;
   return;
 }
 if(heaterPage_==0 && x>=8 && x<312 && y>=112 && y<142 && heaterEditable_){
   heaterEditor_.reload(heaterApplied_);return;
 }
 if(!heaterPage_ || !heaterEditable_ || y<76 || y>=184)return;
 const unsigned n=(heaterPage_-1)*4+(y-76)/27;
 if(x>=226 && x<264)heaterEditor_.change(n,-1);
 else if(x>=274 && x<312)heaterEditor_.change(n,1);
}

void ScreenManager::drawHeaterSettings(const UiTelemetry& t) {
 heaterApplied_=t.heaterStatus;heaterFresh_=t.heaterFresh;
 heaterEditor_.sync(t.heaterStatus,t.heaterFresh,millis());
 heaterBusy_=t.heaterPending || heaterEditor_.ticket;
 heaterEditable_=t.heaterFresh && !heaterBusy_ && t.mainConnected && t.mainCompatible;
 heaterApplyAllowed_=heaterEditable_ && t.machineState==HmiProtocol::MachineState::Ready &&
   (t.heaterStatus.mode==HeaterWire::Mode::Off || t.heaterStatus.mode==HeaterWire::Mode::Fault);
 const char* pages[]={"HEATER OVERVIEW 1/5","PID CONTROL 2/5","SAFETY & POWER 3/5","WATER COMP 4/5","AUTO SLEEP 5/5"};
 const char* pageHelp[]={"","Kp reacts - Ki removes error - Kd damps","Ceiling cuts heat - caps limit output",
   "Feed-forward adds heat for incoming water","Idle time + delay before heater turns OFF"};
 header(canvas_,pages[heaterPage_],true);
 button(canvas_,8,40,80,30,"OFF",UiTheme::RED,false);
 char state[40];
 if(!t.mainConnected)snprintf(state,sizeof(state),"MAIN DISCONNECTED");
 else if(!t.mainCompatible)snprintf(state,sizeof(state),"PROTOCOL MISMATCH");
 else if(!t.heaterFresh)snprintf(state,sizeof(state),"WAITING FOR HEATER DATA");
 else snprintf(state,sizeof(state),"%.1fC  %.0f%%  cap %.0f%%",t.heaterStatus.filtered,t.heaterStatus.power,t.heaterStatus.activeMaxPower);
 textLeft(canvas_,state,96,50,&fonts::Font0,UiTheme::TEXT);
 const char* msg= !t.mainConnected?"Check Main connection":!t.mainCompatible?"Update Main / HMI firmware":
   !t.heaterFresh?"Waiting for current configuration":heaterEditor_.conflict?"Main config changed: reload":
   heaterBusy_?"Waiting for Main ACK":heaterEditor_.message;
 const uint16_t msgColor=heaterEditor_.conflict||heaterBusy_||!heaterEditable_?UiTheme::AMBER:
   heaterEditor_.dirty?UiTheme::AMBER:UiTheme::CYAN;
 textLeft(canvas_,msg,96,64,&fonts::Font0,msgColor);
 if(heaterPage_==0){
   char applied[50];snprintf(applied,sizeof(applied),"Target %.0fC  |  Water comp %s x%.2f",t.heaterStatus.config.target,
     t.heaterStatus.config.ffEnabled?"ON":"OFF",t.heaterStatus.config.ffScale);
   textLeft(canvas_,t.heaterFresh?applied:"Waiting for fresh heater status",8,86,&fonts::Font0,UiTheme::TEXT);
   char limits[50];snprintf(limits,sizeof(limits),"Ceiling %.0fC  |  Idle/Brew cap %.0f/%.0f%%",
     t.heaterStatus.config.ceiling,t.heaterStatus.config.maxPower,t.heaterStatus.config.waterMaxPower);
   textLeft(canvas_,t.heaterFresh?limits:"",8,101,&fonts::Font0,UiTheme::MUTED);
   button(canvas_,8,112,304,30,"RELOAD / DISCARD DRAFT",heaterEditable_?UiTheme::CYAN:UiTheme::DIM,false);
   button(canvas_,8,150,146,34,"PREHEAT",heaterApplyAllowed_?UiTheme::CYAN:UiTheme::DIM,false);
   button(canvas_,166,150,146,34,"CLEAR FAULT",heaterApplyAllowed_?UiTheme::AMBER:UiTheme::DIM,false);
   textLeft(canvas_,heaterApplyAllowed_?"Main is ready for heater commands":"Commands need machine Ready and heater OFF",8,195,&fonts::Font0,UiTheme::MUTED);
 }else{
   for(unsigned row=0;row<4;++row){
     const unsigned n=(heaterPage_-1)*4+row;if(n>=15)break;
     const auto& f=HeaterSettings::fields[n];const int y=76+row*27;
     char val[24];float v=HeaterSettings::value(heaterEditor_.draft,n);
     if(f.bytes==1)snprintf(val,sizeof(val),"%s",v?"ON":"OFF");
     else snprintf(val,sizeof(val),"%.*f",f.decimals,v);
     const bool changed=heaterEditor_.loaded && HeaterSettings::value(heaterEditor_.baseline,n)!=v;
     const bool important=n==0||n==4||n==5||n==6||n==8||n==12;
     if(important)textLeft(canvas_,f.label,8,y+13,&fonts::Font2,UiTheme::TEXT);
     else textLeft(canvas_,f.label,8,y+13,&fonts::Font0,UiTheme::MUTED);
     textRight(canvas_,heaterEditor_.loaded?val:"--",216,y+13,&fonts::Font2,changed?UiTheme::AMBER:UiTheme::CYAN);
     button(canvas_,226,y,38,24,"-",heaterEditable_?UiTheme::CYAN:UiTheme::DIM,false);
     button(canvas_,274,y,38,24,"+",heaterEditable_?UiTheme::CYAN:UiTheme::DIM,false);
   }
   const char* footer=!heaterEditable_?"Waiting for editable Main configuration":
     !heaterApplyAllowed_?"Edit OK - press OFF before SAVE":pageHelp[heaterPage_];
   textLeft(canvas_,footer,8,195,&fonts::Font0,!heaterApplyAllowed_?UiTheme::AMBER:UiTheme::MUTED);
 }
 button(canvas_,8,203,72,32,"PREV",UiTheme::CYAN,false);
 button(canvas_,88,203,72,32,"NEXT",UiTheme::CYAN,false);
 const bool save=heaterApplyAllowed_ && heaterEditor_.dirty && heaterEditor_.valid();
 button(canvas_,172,203,140,32,heaterEditor_.dirty&&!heaterEditor_.valid()?"CHECK / RELOAD":"SAVE",save?UiTheme::GREEN:UiTheme::DIM,false);
}
