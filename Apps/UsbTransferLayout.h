#pragma once
#include <stdbool.h>
typedef struct { int x,y,w,h; } usb_native_rect;
typedef struct {
 int width,height,margin,title_y,status_y,body_y,line_height,body_lines;
 unsigned heading_face,body_face;
 usb_native_rect primary,secondary;
} usb_native_layout;
/* Capability pixels, anchored controls and fixed glyph metrics. There is no
 * virtual viewport or completed-frame scaling. Both paint and hit use this map. */
static inline bool usb_native_layout_open(usb_native_layout *out,int width,int height) {
 if(!out || width<160 || height<160 || width>65535 || height>65535)return false;
 const bool compact=height<224;
 const bool small_text=width<200 || height<240;
 const int margin=(width<200||compact)?6:10,gap=compact?4:8,button=compact?32:44;
 *out=(usb_native_layout){.width=width,.height=height,.margin=margin,
  .title_y=margin,.status_y=margin+(compact?15:24),
  .body_y=margin+(compact?30:48),.line_height=small_text?14:18,
  .body_face=small_text?2u:1u,
  .heading_face=width<200?6u:0u,
  .secondary={margin,height-margin-button,width-2*margin,button},
  .primary={margin,height-margin-2*button-gap,width-2*margin,button}};
 out->body_lines=(out->primary.y-gap-out->body_y)/out->line_height;
 return out->body_lines>=3;
}
static inline bool usb_native_rect_hit(usb_native_rect r,int x,int y) {
 return x>=r.x && y>=r.y && x-r.x<r.w && y-r.y<r.h;
}
