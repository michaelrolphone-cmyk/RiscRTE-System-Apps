#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Provider-free state machine. The adapter owns all I/O and fault classification. */
typedef struct {
 uint64_t generation,layout,submitted_generation,submitted_layout,watermark;
 bool enabled,known,visible,awaiting_observation,gesture,exhausted;
 uint8_t contact;
} portable_native_scene_state;
static inline bool pns_advance(portable_native_scene_state *s) {
 if(s->exhausted||s->generation==UINT64_MAX){s->exhausted=true;s->visible=s->gesture=s->awaiting_observation=false;return false;}
 ++s->generation;return true;
}
static inline bool pns_invalidate(portable_native_scene_state *s) {
 s->visible=s->gesture=s->awaiting_observation=false;
 return pns_advance(s);
}
static inline bool pns_begin(portable_native_scene_state *s) {
 s->enabled=true;s->known=false;return pns_invalidate(s);
}
static inline bool pns_layout(portable_native_scene_state *s,uint64_t id) {
 if(!s->enabled||s->exhausted)return false;
 if(s->known&&s->layout==id)return true;
 if(!pns_invalidate(s))return false;
 s->known=true;s->layout=id;return true;
}
static inline void pns_submit(portable_native_scene_state *s) {
 s->submitted_generation=s->generation;s->submitted_layout=s->layout;
}
static inline void pns_complete(portable_native_scene_state *s) {
 if(!s->enabled||!s->known||s->exhausted||s->submitted_generation!=s->generation||s->submitted_layout!=s->layout)return;
 if(!s->visible)s->awaiting_observation=true;
}
static inline void pns_observe(portable_native_scene_state *s,uint64_t watermark) {
 if(!s->awaiting_observation||s->exhausted)return;
 s->watermark=watermark;s->visible=true;s->awaiting_observation=false;s->gesture=false;
}
static inline bool pns_accept(portable_native_scene_state *s,bool ready,bool valid,bool cancelled,bool began,bool down,bool released,uint8_t contact,uint64_t sequence) {
 if(!s->enabled)return true;
 if(!ready||!valid||cancelled){s->gesture=false;return false;}
 if(began){s->gesture=down&&sequence>s->watermark;s->contact=contact;}
 bool owned=s->gesture&&contact==s->contact;
 if(released){s->gesture=false;return owned;}
 return down?owned:true;
}
