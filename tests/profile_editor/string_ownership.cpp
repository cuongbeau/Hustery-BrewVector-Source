// Regression: compile/run with ArduinoJson 7.4.x on a host C++ toolchain.
// g++ -std=c++17 -I <ArduinoJson>/src string_ownership.cpp -o ownership
#include <ArduinoJson.h>
#include <cassert>
#include <cstring>
#include <string>
struct Wire {char id[40];char name[24];char note[192];char phase[16];};
void append(JsonArray out, const Wire& b) {
  auto p=out.add<JsonObject>();
  // std::string has the same owned-string policy as Arduino String in codec.
  p["id"]=std::string(b.id);p["label"]=std::string(b.name);
  p["description"]=std::string(b.note);p["phase"]=std::string(b.phase);
}
int main(){
  Wire buffer{};JsonDocument doc;auto list=doc.to<JsonArray>();
  for(int i=0;i<3;++i){buffer={};buffer.id[0]='a'+i;buffer.name[0]='A'+i;
    buffer.note[0]='n'+i;buffer.phase[0]='x'+i;append(list,buffer);}
  memset(&buffer,'!',sizeof(buffer));
  assert(list[0]["id"]=="a");assert(list[1]["label"]=="B");
  assert(list[2]["phase"]=="z");assert(list[0]["description"]=="n");
  std::string json;serializeJson(doc,json);assert(json.find("\\u0000")==std::string::npos);
  // Reproduce former const-array-reference path using the exact overload.
  JsonDocument old;Wire reused{};strcpy(reused.name,"first");const Wire& ref=reused;
  old["name"]=ref.name;strcpy(reused.name,"last");
  // With ArduinoJson 7.4.x this demonstrates aliasing to mutable storage.
  assert(strncmp(old["name"].as<const char*>(),"last",4)==0);
}
