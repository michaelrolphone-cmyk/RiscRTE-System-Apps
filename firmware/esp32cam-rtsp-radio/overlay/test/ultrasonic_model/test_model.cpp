#include "ultrasonic_model.h"
#include <cassert>
#include <cmath>
int main() {
    using namespace ultrasonic_model;
    assert(!inRange(0) && !inRange(117) && inRange(118));
    assert(inRange(23529) && !inRange(23530) && !inRange(30000));
    assert(std::fabs(metres(5882)-1.0f) < .0001f);
    assert(fresh(true,true,false,1000,0));
    assert(!fresh(true,true,false,1001,0));
    assert(!fresh(false,true,false,100,0));
    assert(!fresh(true,false,false,100,0));
    assert(!fresh(true,true,true,100,0));
    assert(fresh(true,true,false,20,UINT32_MAX-20));
    assert(TRIGGER_INTERVAL_MS * 1000 > TIMEOUT_US);
}
