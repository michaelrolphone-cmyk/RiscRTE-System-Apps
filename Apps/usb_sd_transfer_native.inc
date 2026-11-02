/* Dedicated native-color presentation. Lifecycle decisions stay in the app. */
#include "UsbTransferNative.h"
#include "UsbTransferLayout.h"
#include "PortableNovaUi.h"
static bool usb_native;
static usb_native_layout usb_layout;
static void usb_native_label(int y,const char *s,unsigned face,uint32_t color) {
 portable_nova_native_text(face,usb_layout.margin,y,
   usb_layout.width-2*usb_layout.margin,s,color);
}
/* Wrap bounded provider text using actual glyph advances. An ellipsis makes
 * truncation explicit; no narrow-screen fit/squash or storage is involved. */
static int usb_native_wrap(int y,int rows,const char *s,uint32_t color) {
 const int width=usb_layout.width-2*usb_layout.margin;
 for(int row=0;row<rows && *s;row++) {
  char line[112];size_t n=0,space=0;
  while(s[n] && n<92 && n+1<sizeof(line)) {
   line[n]=s[n];line[n+1]=0;
   if(portable_nova_measure(usb_layout.body_face,line)>width){line[n]=0;break;}
   if(s[n]==' ')space=n;
   ++n;
  }
  if(!n)break;
  if(s[n] && space){n=space;line[n]=0;}
  const char *next=s+n;while(*next==' ')++next;
  if(row+1==rows && *next) {
   size_t end=strlen(line);
   while(end && (end+4>sizeof(line) || (memcpy(line+end,"...",4),portable_nova_measure(usb_layout.body_face,line)>width)))--end;
   memcpy(line+end,"...",4);
  }
  usb_native_label(y,line,usb_layout.body_face,color);y+=usb_layout.line_height;s=next;
 }
 return y;
}
static void usb_native_button(usb_native_rect r,const char *s,bool primary) {
 portable_nova_native_round(r.x,r.y,r.w,r.h,6,primary?NOVA_CYAN:NOVA_DIM);
 portable_nova_native_round(r.x+1,r.y+1,r.w-2,r.h-2,5,0x031115u);
 const unsigned face=usb_layout.body_face;
 const int text_width=portable_nova_measure(face,s);
 portable_nova_native_text(face,r.x+(r.w-text_width)/2,r.y+(r.h-usb_layout.line_height)/2,
   r.w-12,s,primary?NOVA_WHITE:NOVA_TEXT);
}
static const char *usb_native_status(void) {
 if(state==RISC_USB_MSC_CONNECTED && (host_reads || host_writes))return "SD ACCESS CONFIRMED";
 return status_name();
}
static void usb_native_render(void) {
 const uint32_t painted_state=state;
 dirty=false;
 portable_nova_begin();
 usb_native_label(usb_layout.title_y,"USB SD TRANSFER",usb_layout.heading_face,NOVA_CYAN);
 /* The compact status uses native Rajdhani glyphs; its full label fits160px. */
 usb_native_label(usb_layout.status_y,usb_native_status(),usb_layout.body_face,NOVA_WHITE);
 const bool compact=usb_layout.body_lines<6;
 const char *message,*primary,*secondary;
 if(confirm_removed) {
  message=compact?"Eject or stop copies and unplug. Unplugging during writes can lose files.":
   "Eject the SD drive on your computer. If eject is unavailable, stop all copies and physically unplug the USB cable. Unplugging during a write can lose files.";
  primary="CABLE REMOVED";secondary="KEEP TRANSFERRING";
 } else if(state==RISC_USB_MSC_PREPARING) {
  message=compact?"Finishing log writes. Keep this screen open or cancel.":
   "Finishing pending device log writes. The SD drive appears when preparation ends. Keep this screen open, or cancel below.";
  primary="CANCEL PREPARATION";secondary="HOME LOCKED";
 } else if(portable_usb_transfer_owned()) {
  if(unknown_custody)message="Ownership is uncertain. Restart required.";
  else if(state==RISC_USB_MSC_FAULT_RETAINED)message="SD is reserved. Retry Stop. If it fails, restart the device.";
  else if(state==RISC_USB_MSC_SUSPENDED)message="SD is still shared. Wake your computer. Eject after copies finish.";
  else if(state==RISC_USB_MSC_WAITING)message="Connect a USB data cable. Keep this screen open. Eject after copying.";
  else message="Keep this screen open while copying. Safely eject on your computer when finished.";
  primary="STOP TRANSFER";secondary="HOME LOCKED";
 } else {
  message=state==RISC_USB_MSC_MEDIA_UNAVAILABLE?"Check the SD card and retry Start.":
   terminal(state)?(local_ready?"Transfer ended. SD is available locally.":"Transfer ended. USB ownership released."):
   "Connect a USB data cable. Start shares the SD card. Eject before disconnecting.";
  primary="START TRANSFER";secondary="BACK TO APPS";
 }
 int rows=usb_layout.body_lines;
 /* A small display keeps the complete safety instruction ahead of diagnostics.
  * Larger displays reserve three lines for the provider's bounded reason. */
 int detail_rows=detail[0] && rows>=8?3:0;
 int bottom=usb_native_wrap(usb_layout.body_y,rows-detail_rows,message,NOVA_TEXT);
 if(detail_rows)usb_native_wrap(bottom+usb_layout.line_height,detail_rows-1,detail,NOVA_CYAN);
 usb_native_button(usb_layout.primary,primary,true);
 usb_native_button(usb_layout.secondary,secondary,false);
 app->present(false);
 preparing_presented=painted_state==RISC_USB_MSC_PREPARING && state==painted_state && !dirty;
}
