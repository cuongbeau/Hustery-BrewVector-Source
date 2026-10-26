#include <ArduinoJson.h>
#include <string>
struct Wire {char name[24];};
extern "C" int borrowed_array_policy(const Wire& p) {
  return ArduinoJson::detail::adaptString(p.name).isStatic();
}
extern "C" int owned_string_policy(const std::string& s) {
  return ArduinoJson::detail::adaptString(s).isStatic();
}
