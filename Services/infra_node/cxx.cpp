// Local C++ allocation adapters. No process-global runtime, exceptions or tasks.
#include <cstdlib>
#include <cstddef>
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
extern "C" void __cxa_pure_virtual(){__builtin_trap();}
