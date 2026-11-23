#pragma once
#include <cassert>
#include <mutex>
extern thread_local bool diagnosticInCallback;
struct portMUX_TYPE { std::mutex mutex; };
#define portMUX_INITIALIZER_UNLOCKED {}
inline void portENTER_CRITICAL(portMUX_TYPE *lock) {
    assert(!diagnosticInCallback); lock->mutex.lock();
}
inline void portEXIT_CRITICAL(portMUX_TYPE *lock) { lock->mutex.unlock(); }
