#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "diagnostics.h"
int main(void){
 diagnostic_display d={0};
 assert(!diagnostic_due(&d,0,true,false));
 assert(!diagnostic_due(&d,59999,true,false));
 assert(diagnostic_due(&d,60000,true,false));diagnostic_shown(&d,60000);
 assert(!diagnostic_due(&d,60001,true,true));
 assert(!diagnostic_due(&d,659999,true,false));
 assert(diagnostic_due(&d,660000,true,false));diagnostic_shown(&d,660000);
 assert(!diagnostic_due(&d,660001,false,false));
 assert(!diagnostic_due(&d,700000,true,false));
 assert(diagnostic_due(&d,700001,true,true));diagnostic_shown(&d,700001);
 assert(!diagnostic_due(&d,700002,true,true));
 uint8_t guarded[DIAGNOSTIC_BYTES+2];memset(guarded,0xaa,sizeof guarded);
 const char *lines[17];for(unsigned i=0;i<17;i++)lines[i]="BIOME DIAGNOSTICS 0123456789 !?#: abcdefghijklmnopqrstuvwxyz\xff";
 diagnostic_render(guarded+1,lines,17);
 assert(guarded[0]==0xaa&&guarded[DIAGNOSTIC_BYTES+1]==0xaa);
 unsigned black=0;for(unsigned i=1;i<=DIAGNOSTIC_BYTES;i++)black+=guarded[i]!=0;assert(black>500);
 diagnostic_render(guarded+1,NULL,0);for(unsigned i=1;i<=DIAGNOSTIC_BYTES;i++)assert(!guarded[i]);
 puts("Diagnostic grace, ten-minute cap, recovery, fatal and pixel bounds passed");
}
