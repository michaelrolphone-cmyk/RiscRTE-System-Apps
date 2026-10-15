/* Native-coordinate controller assertions shared by optional-service and full
 * healthy-provider fixtures. The production presenter owns every pixel. */
static springboard_contact native_input(nova_state *s,bool down,unsigned x,unsigned y) {
 springboard_contact c=read_contact(down,x,y);nova_contact_input(s,&c,20);return c;
}
static void controller_tests(void) {
 open_controller();nova_state s={.view=production,.pending=-1,.initial=true};nova_layout(&s);
 assert(api->screen_width()==(int)fw && api->screen_height()==(int)fh);
 assert(s.n==19);render(&s);capture("initial");
 int cx=(int)fw/2,cy=(int)fh/2,hit=nova_px(24)*287/256;
 assert(nova_hit(&s,cx,cy)==0);assert(nova_hit(&s,cx+hit-1,cy)==0);assert(nova_hit(&s,cx+hit,cy)!=0);
 /* Same19-point ordering, distinct adjacent colors and all catalog identities. */
 for(unsigned first=0;first<count;first+=19) {
  s.first=first;nova_layout(&s);
  for(unsigned i=0;i<s.n;i++) {
   s.motion.x=-(float)s.points[i].x/16;s.motion.y=-(float)s.points[i].y/16;s.focus=i;
   render(&s);assert(nova_hit(&s,cx,cy)==(int)i);
   if(i==0){char name[32];snprintf(name,sizeof(name),"page-%u",first/19);capture(name);}
  }
 }
 s.first=0;nova_layout(&s);render(&s);
 native_input(&s,false,(unsigned)cx,(unsigned)cy);
 springboard_contact c=native_input(&s,true,(unsigned)cx,(unsigned)cy);
 assert(c.valid&&c.down&&c.began&&c.tap_eligible&&c.x==cx&&c.y==cy);
 unsigned x=(unsigned)(cx+nova_px(30)),y=(unsigned)(cy+nova_px(24));
 native_input(&s,true,x,y);assert(s.moved>=(unsigned)nova_px(53)&&s.motion.x>0&&s.motion.y>0);
 native_input(&s,false,x,y);assert(s.pending<0);
 for(unsigned n=0;n<300;n++)nova_tick(&s,20);
 assert(s.motion.settled);render(&s);capture("after-drag");assert(s.pending<0&&!launches);
 s.first=0;nova_layout(&s);render(&s);
 y=fh-(unsigned)nova_px(10);
 native_input(&s,false,(unsigned)cx,y);native_input(&s,true,(unsigned)cx,y);native_input(&s,false,(unsigned)cx,y);
 assert(s.first==19&&s.n==count-19);render(&s);capture("page-touch");
 native_input(&s,true,(unsigned)cx,y);native_input(&s,false,(unsigned)cx,y);assert(!s.first);render(&s);
 native_input(&s,true,(unsigned)cx,(unsigned)cy);native_input(&s,false,(unsigned)cx,(unsigned)cy);assert(s.pending==0);
 c=read_contact(true,fw,fh/2);assert(!c.valid);nova_contact_input(&s,&c,20);assert(s.pending<0);
 read_contact(false,0,0);c=read_contact(true,fw/2,fh);assert(!c.valid);
 read_contact(false,0,0);contact_count=2;c=read_contact(true,(unsigned)cx,(unsigned)cy);assert(!c.valid);
 contact_count=1;read_contact(false,0,0);
 c=read_contact(true,fw-1,fh-1);assert(c.valid&&c.x==(int)fw-1&&c.y==(int)fh-1);read_contact(false,0,0);
 /* Non-grid physical coordinates survive unrounded, unlike the old inverse. */
 x=fw/2+1;y=fh/2+1;c=read_contact(true,x,y);assert(c.valid&&c.x==(int)x&&c.y==(int)y);read_contact(false,0,0);
 if(fh>fw+32) {
  s.first=0;nova_layout(&s);s.motion.y=(float)nova_px(15);render(&s);capture("lower-honeycomb");
  unsigned colored=0;for(unsigned yy=fw;yy<fh;yy++)for(unsigned xx=0;xx<fw;xx++)colored+=pixels[yy*pitch+xx]!=0;
  assert(colored>100);
 }
 close_controller();
}
