#define PORTABLE_HOME_APP "default.elf"
#define FILE_BROWSER_PAPER_PROFILE
#define PORTABLE_DISPLAY_ROTATION 90
#define PORTABLE_FILE_BROWSER_CAPABILITY "storage.volume"
#define FILE_BROWSER_RETURN_APP "springboard.elf"
#define main legacy_file_browser_fixture_main
#include "portable_file_browser_test.c"
#undef main
static void paper_frame(const char *name){const char *directory=getenv("FILE_BROWSER_PAPER_FRAMES");if(!directory)return;char path[512];snprintf(path,sizeof(path),"%s/%s.pbm",directory,name);FILE*f=fopen(path,"wb");assert(f);fprintf(f,"P4\n800 480\n");assert(fwrite(fb_pixels,1,sizeof(fb_pixels),f)==sizeof(fb_pixels));fclose(f);}
int main(void){
 check_model();test_volume=(risc_storage_volume_api_v1){1,sizeof(test_volume),NULL,test_ready,test_ready,test_label,test_stat,test_diropen,test_dirnext,test_dirclose,test_open,test_read,NULL,NULL,test_close,NULL,test_error_get};
 assert(app_module_init()==0&&fb_open()&&fb_paper&&fb_page_rows==6&&fb_count==6);fb_draw();paper_frame("files");unsigned frames=test_presents;fb_draw();assert(test_presents==frames);
 for(unsigned i=0;i<6;i++){char name[24];snprintf(name,sizeof(name),"file%u.txt",i+1);assert(!strcmp(fb_rows[i].name,name));}
 fb_page_next();assert(fb_page==1&&fb_count==2&&!strcmp(fb_rows[0].name,"file7.txt"));fb_page_previous();assert(fb_page==0&&!strcmp(fb_rows[5].name,"file6.txt"));
 test_entries=513;assert(fb_load(0));for(unsigned i=0;i<85;i++)fb_page_next();assert(fb_page==85&&fb_count==3&&!strcmp(fb_rows[2].name,"file513.txt"));fb_page_previous();assert(fb_count==6&&!strcmp(fb_rows[0].name,"file505.txt")&&!strcmp(fb_rows[5].name,"file510.txt"));
 test_entries=8;assert(fb_load(0));assert(!fbp_touch(420,50)&&fb_mode==FB_OPTIONS);fb_draw();paper_frame("options");assert(!fbp_touch(100,150)&&fb_mode==FB_FILTER);fb_draw();paper_frame("filter");
 unsigned chars=0;for(unsigned page=0;page<(95+fbp_keys()-1)/fbp_keys();page++)for(unsigned key=0;key<fbp_keys()&&32+page*fbp_keys()+key<=126;key++){fbp_key_page=page;fb_editor[0]=0;int w=(fbp_w()-64)/5;assert(!fbp_touch(32+(int)(key%5)*w+10,208+(int)(key/5)*88+30));assert((unsigned char)fb_editor[0]==32+page*fbp_keys()+key);chars++;}assert(chars==95);
 strcpy(fb_editor,"file8");assert(!fbp_touch(240,730)&&fb_mode==FB_FILTER&&!fb_query[0]);assert(!fbp_touch(350,730)&&fb_mode==FB_LIST&&fb_count==1&&!strcmp(fb_rows[0].name,"file8.txt"));fb_draw();paper_frame("filtered");
 fb_query[0]=0;assert(fb_load(0));assert(!fbp_touch(100,150)&&fb_mode==FB_DETAIL);fb_draw();paper_frame("detail");assert(!fbp_touch(300,730)&&fb_mode==FB_PREVIEW&&fb_preview_count==192);fb_draw();paper_frame("preview");
 fb_preview_page(1);assert(fb_offset==192&&fb_preview[0]=='K');fb_activate();assert(fb_hex&&fb_preview_count==64);fb_draw();paper_frame("hex");
 test_fail_close=true;assert(!fb_preview_read()&&fb_retained_file);unsigned reads=test_reads;assert(!fb_preview_read()&&test_reads==reads);assert(!fb_back());test_fail_close=false;assert(portable_file_browser_close());
 fb_mode=FB_LIST;test_fail_acquire=true;assert(!fb_load(0));fb_draw();paper_frame("unavailable");test_fail_acquire=false;assert(fb_load(0));test_invalid_name=true;assert(!fb_load(0)&&!fb_count);test_invalid_name=false;assert(fb_load(0));
 assert(fb_back());app_module_fini();assert(!test_grants&&!test_frames&&!test_subs);
 for(unsigned script=4;script<=7;script++){test_script=script;test_polls=0;test_seen_modes=0;unsigned before=test_launches;assert(app_module_init()==0);app_main();app_module_fini();assert(test_launches==before+1&&!test_grants&&!test_frames&&!test_subs);if(script==4)assert(test_seen_modes&(1u<<FB_PREVIEW));if(script==5)assert(fb_page==1);if(script==6)assert(!fb_query[0]);}
 puts("Paper File Browser: native raster, 513-item bidirectional paging, 95 filter keys, preview/error/close safety and real raw-touch lifecycle pass");
}
