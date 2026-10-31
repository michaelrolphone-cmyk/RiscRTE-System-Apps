/* Private target-ELF compiler support, not a firmware capability or export.
 * Reuse the canonical restoring unsigned division without signed negation of
 * INT64_MIN. C-undefined zero/overflow inputs have deterministic 0/wrap results. */
#include <stdint.h>
extern uint64_t __udivdi3(uint64_t numerator, uint64_t denominator);
__attribute__((visibility("hidden")))
int64_t __divdi3(int64_t numerator, int64_t denominator) {
    uint64_t n=(uint64_t)numerator, d=(uint64_t)denominator;
    if(numerator<0)n=UINT64_C(0)-n;
    if(denominator<0)d=UINT64_C(0)-d;
    uint64_t q=__udivdi3(n,d);
    if(q==UINT64_C(0x8000000000000000))return INT64_MIN;
    return (numerator<0)!=(denominator<0)?-(int64_t)q:(int64_t)q;
}
