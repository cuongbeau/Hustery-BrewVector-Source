#include "src/app/quick_yield_session.h"
constexpr bool nextShotOnly(){
  QuickYieldSession s;
  if(!s.command(0,42,36,100))return false;
  s.rotate(4);
  if(s.command(2,41,36,101) || !s.command(2,42,36,102))return false;
  s.consume(true);
  if(s.running!=40 || __builtin_isfinite(s.pending) || s.editor)return false;
  s.consume(true);return !__builtin_isfinite(s.running);
}
constexpr bool cancelAndLoss(){
  QuickYieldSession s;s.command(0,1,36,0);s.rotate(5);s.command(3,1,36,1);
  if(__builtin_isfinite(s.pending))return false;
  s.command(0,2,36,2);s.command(2,2,36,3);s.poll(false,4);s.consume(true);
  return !__builtin_isfinite(s.running) && !s.editor;
}
constexpr bool leaseAndWrap(){
  QuickYieldSession s;s.command(0,1,36,0xfffffff0U);s.poll(true,20);
  if(!s.editor)return false;
  s.poll(true,1984);return !s.editor && !s.command(2,1,36,1985);
}
constexpr bool boundsAndDuplicate(){
  QuickYieldSession s;s.command(0,1,36,0);s.rotate(3);s.command(0,1,36,1);
  if(s.draft!=39 || s.command(0,2,36,2))return false;
  s.rotate(-100);if(s.draft!=0)return false;
  s.rotate(999);return s.draft==500;
}
static_assert(nextShotOnly(),"Applied yield is consumed once, stale token rejected");
static_assert(cancelAndLoss(),"Cancel never applies draft; loss clears pending override");
static_assert(leaseAndWrap(),"Lease deadline wraps and rejects late apply");
static_assert(boundsAndDuplicate(),"Duplicate open cannot reset draft, bounded grams");
constexpr bool physicalStartAndLoss(){
  QuickYieldSession s;s.command(0,1,36,0);s.rotate(4);
  s.consume(true);if(__builtin_isfinite(s.running))return false; // Unsaved draft is never consumed.
  s.command(0,2,36,1);s.command(2,2,36,2);s.consume(true);
  s.poll(false,3); // Active shot snapshot outlives idle-editor validity; scale eligibility guards execution.
  return s.running==36 && !__builtin_isfinite(s.pending);
}
static_assert(physicalStartAndLoss(),"Start ignores unsaved draft and keeps consumed shot snapshot");
