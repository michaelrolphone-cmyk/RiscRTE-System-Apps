#include "ultrasonic_capture_model.h"
#include <cassert>
#include <cstdio>
using ultrasonic_capture::Sample;

int main() {
    Sample history[4];
    assert(!ultrasonic_capture::selectAt(history,100).sampled);
    history[0].sampled=true; history[0].valid=true;
    history[0].observedUs=100; history[0].completedUs=110; history[0].sequence=1;
    assert(!ultrasonic_capture::selectAt(history,105).sampled);
    assert(ultrasonic_capture::selectAt(history,110).sequence==1);
    history[1]=history[0]; history[1].observedUs=120; history[1].completedUs=130;
    history[1].sequence=2;
    assert(ultrasonic_capture::selectAt(history,125).sequence==1);
    assert(ultrasonic_capture::selectAt(history,130).sequence==2);
    history[2]=history[1]; history[2].observedUs=140; history[2].completedUs=150;
    history[2].sequence=3; history[2].valid=false; history[2].status="timeout";
    assert(!ultrasonic_capture::selectAt(history,160).valid);
    assert(ultrasonic_capture::selectAt(history,160).sequence==3);
    history[3]=history[0]; history[3].observedUs=200; history[3].completedUs=170;
    assert(ultrasonic_capture::selectAt(history,180).sequence==3);
    history[0].observedUs=(uint64_t(1)<<32)*1000+5;
    history[0].completedUs=history[0].observedUs+10; history[0].sequence=4;
    assert(ultrasonic_capture::selectAt(history,history[0].completedUs).sequence==4);
    std::puts("sonar capture history: causal completion, invalidation and 64-bit uptime passed");
}
