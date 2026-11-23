#pragma once
struct TestUpdate {
    bool running=false;
    bool isRunning() const{return running;}
};
extern TestUpdate Update;
