/* Points current facade executes the unchanged shared production scene fixture. */
#define PORTABLE_POINTS_NATIVE_UI
#define main shared_scene_main
#include "native_scene_adapter_test.c"
#undef main
#include "PortablePointsNative.h"
_Static_assert(sizeof(portable_points_contact)==sizeof(portable_native_scene_contact),"exact copied contact");
int main(int argc,char **argv) {
 assert(portable_points_native_available==portable_native_scene_available);
 assert(portable_points_native_begin==portable_native_scene_begin);
 assert(portable_points_native_layout==portable_native_scene_layout);
 assert(portable_points_native_present==portable_native_scene_present);
 assert(portable_points_native_ready==portable_native_scene_ready);
 assert(portable_points_native_repaint==portable_native_scene_repaint);
 assert(portable_points_native_contact==portable_native_scene_contact_read);
 assert(portable_points_native_back==portable_native_scene_back);
 return shared_scene_main(argc,argv);
}
