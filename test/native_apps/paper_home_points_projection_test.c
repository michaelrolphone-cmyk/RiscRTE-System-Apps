#include <assert.h>
#include <stdio.h>
#include "../../Apps/PaperHomePoints.h"
static portable_timezone_rule zone(const char *name) {
 portable_timezone_rule rule;assert(portable_timezone_resolve(name,strlen(name)+1,&rule)==PORTABLE_TIMEZONE_OK);return rule;
}
int main(void) {
 portable_timezone_rule rule=zone("UTC");
 points_config config={.revision=4,.created=1,.points={
  {POINTS_WORK_START,1,0,127,8,30,0,0,0},
  {POINTS_LUNCH,1,0,127,12,0,60,0,1},
  {POINTS_WORK_END,1,0,127,17,0,0,0,0}}};
 points_meta meta={.revision=3};nova_points_state view;
 /* 2026-10-07 10:42 UTC. The end edge stays visible without notify_end;
  * warning cues do not become Home rows. */
 assert(paper_points_project(&config,&meta,&rule,1791369720,&view));
 assert(view.status==NOVA_POINTS_READY&&view.next_count==4&&view.previous_valid);
 assert(view.next[0].hour==12&&view.next[0].kind==POINTS_LUNCH&&!view.next[0].is_end);
 assert(view.next[0].at_rtc-view.now_rtc==78*60&&view.previous.hour==8&&view.previous.minute==30);
 assert(view.next[1].hour==13&&view.next[1].is_end&&!strcmp(paper_points_label(&view.next[1]),"BACK"));
 assert(view.next[2].hour==17&&paper_points_progress(&view,416)==261);
 config.points[1].kind=POINTS_CUSTOM_1;strcpy(meta.custom[0].name,"Drive to Work");
 assert(paper_points_project(&config,&meta,&rule,1791369720,&view));
 assert(!strcmp(view.next[0].label,"Drive to Work")&&view.next[0].source_slot==1);
 assert(!strcmp(paper_points_label(&view.next[0]),"Drive to Work"));
 config=(points_config){.revision=5,.created=1};
 assert(paper_points_project(&config,&meta,&rule,1791369720,&view)&&view.status==NOVA_POINTS_EMPTY&&!view.next_count);
 assert(!paper_points_project(&config,&meta,&rule,2147483648LL,&view)&&view.status==NOVA_POINTS_ERROR);
 rule=zone("America/Denver");
 config=(points_config){.revision=6,.created=1,.points={{POINTS_WORK_START,1,0,127,2,30,0,0,0}}};
 /* Spring gap 2026-03-08: the invalid Sunday start is skipped, not normalized. */
 assert(paper_points_project(&config,&meta,&rule,1772956800,&view));
 assert(view.next[0].day_offset==1&&view.next[0].hour==2&&view.next[0].minute==30);
 config.points[0].hour=1;
 /* Autumn fold 2026-11-01: no arbitrary choice between the two 01:30 starts. */
 assert(paper_points_project(&config,&meta,&rule,1793512800,&view));
 assert(view.next[0].day_offset==1&&view.next[0].hour==1&&view.next[0].minute==30);
 puts("Home Points projection: shared native UTC, local zone, next/end, metadata, empty, range, gap and fold PASS");return 0;
}
