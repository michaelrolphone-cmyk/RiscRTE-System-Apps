#pragma once
namespace BiomeRelay {
// Definitive rejection ends this attachment/fence attempt. Network failures and
// server overload retain the pending request for an idempotent retry.
inline bool bindingInvalidated(int status,bool attaching,bool fencing){
 return (attaching&&(status==403||status==404||status==409||status==410))||
        (fencing&&(status==404||status==409||status==410));
}
}
