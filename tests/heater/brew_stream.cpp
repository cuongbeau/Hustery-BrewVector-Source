#include "model/brew_stream.h"
constexpr bool checkStream(){
  BrewStream stream;BrewStream::Sample storage[BrewStream::Capacity]={};
  stream.attach(storage);
  HmiProtocol::TelemetryPayload p={};
  p.machineState=static_cast<uint8_t>(HmiProtocol::MachineState::Brew);
  for(unsigned i=0;i<600;++i){p.timestampMs=i*50;p.elapsedS=i*0.05f;stream.push(p);}
  if(stream.size()!=512 || stream.at(0).seq!=89 || stream.at(511).seq!=600)return false;
  stream.push(p);
  if(stream.size()!=512 || stream.at(511).seq!=600)return false;
  auto session=stream.session();
  p.machineState=static_cast<uint8_t>(HmiProtocol::MachineState::Ready);stream.push(p);
  if(stream.size()!=512)return false;
  p.machineState=static_cast<uint8_t>(HmiProtocol::MachineState::Brew);p.elapsedS=0;stream.push(p);
  if(stream.size()!=1 || stream.session()!=session+1 || stream.at(0).seq!=601)return false;
  p.timestampMs=5;p.elapsedS=0;stream.push(p);
  return stream.size()==1 && stream.session()==session+2 && stream.at(0).seq==602;
}
static_assert(checkStream(),"Brew ring must preserve order, bound memory, deduplicate and reset sessions");
