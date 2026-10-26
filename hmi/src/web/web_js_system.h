#pragma once
#include <Arduino.h>

static const char WEB_JS_SYSTEM[] PROGMEM = R"JS(
  window.addEventListener('DOMContentLoaded',function(){
    console.log('BREWVECTOR Page Init...');
    switchTab('dash');
    startLive();
  });
)JS";
