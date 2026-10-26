#pragma once
#include "web/hmi_web_internal.h"
#include <type_traits>
#include "app/owner_context.h"

// Return a copy: references into MainLink never escape to the HTTP worker.
template<class F> auto ownerValue(F fn) -> typename std::decay<decltype(fn())>::type {
  typename std::decay<decltype(fn())>::type result{};
  HmiWebInternal::server.callOwner([&](){
    const bool previous=HmiOwner::webCall; HmiOwner::webCall=true;
    result=fn(); HmiOwner::webCall=previous;
  });
  return result;
}
