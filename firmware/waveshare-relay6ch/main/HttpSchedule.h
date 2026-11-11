#pragma once
namespace BiomeRelay {
// Both state machines use the same native HTTP enforcement point. Alternate
// when both have work, but never lend an outstanding token to the other owner.
struct HttpSchedule {
 bool nativeTurn=true;
 bool grantNative(bool nodeOwnsHttp,bool nodeSendReady,bool setup)const{
  return !setup&&!nodeOwnsHttp&&(nativeTurn||!nodeSendReady);
 }
 void openedNative(){nativeTurn=false;}
 void openedNode(){nativeTurn=true;}
};
}
