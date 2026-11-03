#pragma once
#include "Policy.h"

namespace Infra {
constexpr unsigned OutputFrameLimit = 32;
constexpr unsigned OutputWaitLimitMs = 60000;
struct OutputFrame {
 uint8_t on = 0, off = 0, rgb[3]{};
 bool hasRgb = false;
 int tone = -1;
 unsigned waitMs = 0;
};

inline bool channelMask(JsonVariantConst channels, uint8_t& mask) {
 if (!channels.is<JsonArrayConst>() || channels.size() == 0 || channels.size() > 6) return false;
 for (auto channel : channels.as<JsonArrayConst>()) {
  if (!integer(channel, 1, 6)) return false;
  const uint8_t bit = 1u << (channel.as<unsigned>() - 1);
  if (mask & bit) return false;
  mask |= bit;
 }
 return true;
}

// Validate the entire program before permitting its first physical effect.
inline bool decodeOutputBatch(JsonObjectConst input, OutputFrame* frames, unsigned& totalMs) {
 totalMs = 0;
 if (!keys(input, {"steps"}) || !input["steps"].is<JsonArrayConst>()) return false;
 auto steps = input["steps"].as<JsonArrayConst>();
 if (!steps.size() || steps.size() > OutputFrameLimit || measureJson(input) > 3072) return false;
 unsigned index = 0;
 for (JsonObjectConst step : steps) {
  if (!keys(step, {}, {"on","off","rgb","tone_hz","wait_ms"})) return false;
  OutputFrame frame;
  if (step.containsKey("on") && !channelMask(step["on"], frame.on)) return false;
  if (step.containsKey("off") && !channelMask(step["off"], frame.off)) return false;
  if (frame.on & frame.off) return false;
  if (step.containsKey("rgb")) {
   auto rgb = step["rgb"].as<JsonArrayConst>();
   if (rgb.size()!=3) return false;
   for (unsigned i=0;i<3;++i) { if (!integer(rgb[i],0,255)) return false; frame.rgb[i]=rgb[i]; }
   frame.hasRgb = true;
  }
  if (step.containsKey("tone_hz")) {
   if (!integer(step["tone_hz"],0,10000)) return false;
   frame.tone = step["tone_hz"];
   if (frame.tone && frame.tone < 100) return false;
  }
  if (step.containsKey("wait_ms")) {
   if (!integer(step["wait_ms"],0,OutputWaitLimitMs-totalMs)) return false;
   frame.waitMs=step["wait_ms"]; totalMs+=frame.waitMs;
  }
  if (!frame.on && !frame.off && !frame.hasRgb && frame.tone<0 && !frame.waitMs) return false;
  if (frames) frames[index]=frame;
  ++index;
 }
 return true;
}
}
