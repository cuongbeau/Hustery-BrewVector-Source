#include "model/shot_save_policy.h"
static_assert(ShotSavePolicy::RecentLimit==10,"Only ten recent shots");
static_assert(!ShotSavePolicy::accepts(10,9,250),"Exactly ten seconds is rejected");
static_assert(!ShotSavePolicy::accepts(9.99f,9,250),"Short pressurized run is rejected");
static_assert(!ShotSavePolicy::accepts(30,0.99f,750),"Long low-pressure run is rejected");
static_assert(ShotSavePolicy::accepts(10.01f,1,251),"Boundary above ten seconds and at one bar is accepted");
static_assert(ShotSavePolicy::accepts(30,9,750),"Normal shot is accepted");
static_assert(!ShotSavePolicy::accepts(30,9,1),"No useful samples");
static_assert(!ShotSavePolicy::accepts(__builtin_nanf(""),9,750),"Invalid duration");
static_assert(!ShotSavePolicy::accepts(30,__builtin_inff(),750),"Invalid pressure");
