#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum class PcCommandKind { None, Hello, Info, Status, Time, Request, Error };
struct PcCommand {
  PcCommandKind kind = PcCommandKind::None;
  int64_t epoch = 0;
  uint32_t requestId = 0;
  char operation[24]{};
  char payload[288]{};
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
    if (strncmp(line_, "REQ ", 4) == 0) {
      const char* cursor = line_ + 4;
      uint32_t id = 0;
      int digits = 0;
      while (*cursor >= '0' && *cursor <= '9') {
        if (++digits > 9) return true;
        id = id * 10 + *cursor++ - '0';
      }
      if (!id || *cursor++ != ' ') return true;
      const char* operation = cursor;
      while (*cursor >= 'A' && *cursor <= 'Z') ++cursor;
      const size_t length = cursor - operation;
      if (!length || length >= sizeof(out.operation) || (*cursor && *cursor != ' ')) return true;
      if (*cursor) ++cursor;
      if (strlen(cursor) >= sizeof(out.payload)) return true;
      out.kind = PcCommandKind::Request;
      out.requestId = id;
      memcpy(out.operation, operation, length);
      strcpy(out.payload, cursor);
    } else if (strcmp(line_, "HELLO") == 0) {
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
  bool active() const { return active_; }

private:
  char line_[352]{};
  size_t count_ = 0;
  uint32_t started_ = 0;
  bool active_ = false;
  bool invalid_ = false;
};

// GATT commands are complete packets, never shell input or multiple frames.
inline bool pcParsePacket(const char* bytes, size_t length, PcCommand& command) {
  command = {};
  if (!bytes || length < 2 || length > 20 || bytes[0] != '@' || bytes[length - 1] != '\n') return false;
  PcProtocol parser;
  for (size_t i = 0; i < length; ++i) {
    if (i > 0 && bytes[i] == '@') return false;
    parser.feed(bytes[i], 0, command);
    if (i + 1 != length && command.kind != PcCommandKind::None) return false;
  }
  return command.kind != PcCommandKind::None;
}
