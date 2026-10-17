#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class PcCommandKind { None, Hello, Info, Status, Time, Error };
struct PcCommand {
  PcCommandKind kind = PcCommandKind::None;
  int64_t epoch = 0;
};

// Framed commands coexist with single-key shell input. Never dispatch a
// partial/oversized command's payload as shell keys. A new @ resynchronizes.
class PcProtocol {
public:
  bool feed(char c, uint32_t now, PcCommand& out) {
    out = {};
    if (c == '@') {
      active_ = true;
      invalid_ = false;
      count_ = 0;
      started_ = now;
      return true;
    }
    if (!active_) return false;
    if (now - started_ > 2000) invalid_ = true;
    if (c == '\r') return true;
    if (c != '\n') {
      if (count_ < sizeof(line_) - 1 && c >= ' ' && c <= '~') line_[count_++] = c;
      else invalid_ = true;
      return true;
    }
    active_ = false;
    line_[count_] = '\0';
    out.kind = PcCommandKind::Error;
    if (invalid_) return true;
    if (strcmp(line_, "HELLO") == 0) {
      out.kind = PcCommandKind::Hello;
    } else if (strcmp(line_, "INFO") == 0) {
      out.kind = PcCommandKind::Info;
    } else if (strcmp(line_, "STATUS") == 0) {
      out.kind = PcCommandKind::Status;
    } else if (strncmp(line_, "TIME ", 5) == 0 && count_ == 15) {
      int64_t epoch = 0;
      for (size_t i = 5; i < count_; ++i) {
        if (line_[i] < '0' || line_[i] > '9') return true;
        epoch = epoch * 10 + line_[i] - '0';
      }
      if (epoch >= 1700000000LL && epoch <= 4102444799LL) {
        out.kind = PcCommandKind::Time;
        out.epoch = epoch;
      }
    }
    return true;
  }

  void reset() { active_ = false; }

private:
  char line_[32]{};
  size_t count_ = 0;
  uint32_t started_ = 0;
  bool active_ = false;
  bool invalid_ = false;
};
