#include "RiscProtectedSettingsPath.h"
#include "FileBrowserModel.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 const char *blocked[]={"/RTECFG","rtecfg","/rTeCfG/a","/sd/RTECFG./a","/a/rtecfg. /b","\\RTECFG\\a","/RTECFG/../public"};
 const char *allowed[]={"/","/RTECFG.txt","/RTECFG-other","/prefixRTECFG","/RTECF","/AppData/app/settings.index"};
 for(unsigned i=0;i<sizeof(blocked)/sizeof(*blocked);++i)assert(risc_protected_settings_path(blocked[i]));
 for(unsigned i=0;i<sizeof(allowed)/sizeof(*allowed);++i)assert(!risc_protected_settings_path(allowed[i]));
 char out[512];assert(!fb_join("/","RTECFG",out,sizeof out));assert(!fb_join("/rtecfg","value",out,sizeof out));assert(fb_join("/","RTECFG.txt",out,sizeof out));
 puts("Protected settings component and FAT aliases PASS");return 0;
}
