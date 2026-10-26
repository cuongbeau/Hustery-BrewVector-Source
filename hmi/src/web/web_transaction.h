#pragma once
#include "web/owner_call.h"
#include "communication/main_link.h"
class WebTransaction {
 public:
  explicit WebTransaction(MainLink& link):link_(link) {
    held_=ownerValue([&](){return link_.beginWebTransaction();});
  }
  ~WebTransaction(){if(held_)ownerValue([&](){return link_.endWebTransaction();});}
  explicit operator bool() const{return held_;}
 private: MainLink& link_;bool held_=false;
};
