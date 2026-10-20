#pragma once
#include <stdint.h>
#include <string.h>
#include "usb/pc_data.h"

// Application-task only. Each client owns a cursor; reads never consume events.
class ActionInput {
public:
  bool push(const char* key) {
    int button = !strcmp(key, "primary") ? 1 : !strcmp(key, "secondary") ? 2 : !strcmp(key, "long") ? 3 : 0;
    if (!button || sequence_ == UINT32_MAX) return false;
    ++sequence_; buttons_[(sequence_ - 1) % 8] = button;
    return true;
  }
  uint32_t sequence() const { return sequence_; }
  bool json(uint32_t after, char* output, size_t capacity) const {
    if (after > sequence_) return false;
    const uint32_t oldest = sequence_ > 8 ? sequence_ - 8 : 0;
    PcJsonWriter w(output, capacity);
    w.raw("{\"sequence\":"); w.number(sequence_);
    w.raw(",\"lost\":"); w.boolean(after < oldest);
    w.raw(",\"events\":[");
    bool comma = false;
    for (uint32_t i = (after > oldest ? after : oldest); i < sequence_; ++i) {
      if (comma) w.raw(",");
      comma = true;
      w.raw("{\"id\":"); w.number(i + 1); w.raw(",\"button\":");
      w.text(buttons_[i % 8] == 1 ? "primary" : buttons_[i % 8] == 2 ? "secondary" : "long");
      w.raw("}");
    }
    w.raw("]}"); return w.finish();
  }
private:
  uint32_t sequence_ = 0;
  uint8_t buttons_[8]{};
};
inline ActionInput flintActionInput;
