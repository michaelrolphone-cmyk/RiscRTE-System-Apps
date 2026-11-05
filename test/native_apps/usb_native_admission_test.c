#define USB_TRANSFER_TEST_MAIN ignored_usb_main
#include "usb_transfer_test.c"
#ifdef PORTABLE_USB_TRANSFER_NATIVE_UI
#include "../../Apps/UsbTransferNative.h"
#endif
int main(void){
 assert(app_module_init()==0);
 const bool color=TEST_PIXEL_FORMAT==RISC_DISPLAY_FORMAT_RGB565;
#ifdef PORTABLE_USB_TRANSFER_NATIVE_UI
 assert(portable_usb_transfer_native_available()==color);
 const unsigned minimum=160;
#else
 const unsigned minimum=240;
#endif
 const bool expected=!color || (PANEL_WIDTH>=minimum&&PANEL_HEIGHT>=minimum);
 assert((paper_presentation_get()!=NULL)==expected);
 app_module_fini();assert(!grants&&!subs&&!frames&&!session);
 printf("USB admission: %ux%u color=%u minimum=%u admitted=%u; no MSC session\n",PANEL_WIDTH,PANEL_HEIGHT,(unsigned)color,minimum,(unsigned)expected);
}
