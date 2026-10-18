#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Bounded, allocation-free JSON output. Never expose a truncated document.
class PcJsonWriter {
public:
  PcJsonWriter(char* buffer, size_t size) : buffer_(buffer), size_(size) {}
  void raw(const char* text) {
    while (*text) put(*text++);
  }
  void text(const char* text) {
    static constexpr char hex[] = "0123456789abcdef";
    put('"');
    while (*text) {
      const auto c = static_cast<unsigned char>(*text++);
      if (c == '"' || c == '\\') { put('\\'); put(c); }
      else if (c < 32) { raw("\\u00"); put(hex[c >> 4]); put(hex[c & 15]); }
      else put(c);
    }
    put('"');
  }
  void number(int64_t value) {
    char text[24];
    snprintf(text, sizeof(text), "%lld", static_cast<long long>(value));
    raw(text);
  }
  void boolean(bool value) { raw(value ? "true" : "false"); }
  bool finish() {
    if (!buffer_ || size_ == 0) return false;
    buffer_[okay_ ? count_ : 0] = '\0';
    return okay_;
  }

private:
  void put(char c) {
    if (!buffer_ || count_ + 1 >= size_) { okay_ = false; return; }
    buffer_[count_++] = c;
  }
  char* buffer_;
  size_t size_;
  size_t count_ = 0;
  bool okay_ = true;
};

struct PcDeviceInfo {
  const char* id;
  const char* model;
  const char* board;
  const char* chip;
  const char* firmware;
  int api;
  int screenWidth;
  int screenHeight;
  bool companionAllowed = true;
};

inline bool pcWriteInfo(char* buffer, size_t size, const PcDeviceInfo& info) {
  PcJsonWriter w(buffer, size);
  w.raw("{\"protocol\":1,\"id\":"); w.text(info.id);
  w.raw(",\"model\":"); w.text(info.model);
  w.raw(",\"board\":"); w.text(info.board);
  w.raw(",\"chip\":"); w.text(info.chip);
  w.raw(",\"firmware\":"); w.text(info.firmware);
  w.raw(",\"api\":"); w.number(info.api);
  w.raw(",\"screen\":{\"width\":"); w.number(info.screenWidth);
  w.raw(",\"height\":"); w.number(info.screenHeight);
  w.raw("},\"capabilities\":[\"info\",\"status\",\"time_sync\",\"control\",\"settings\",\"faces\",\"face_studio\"");
  if (info.companionAllowed) w.raw(",\"companion\"");
  w.raw("]}");
  return w.finish();
}

struct PcDeviceStatus {
  int64_t uptimeMs;
  uint32_t freeHeap;
  bool usbConnected;
  bool timeValid;
  bool pcSynced;
  int64_t epoch;
  const char* timezone;
  const char* activeFace;
  uint8_t showSeconds;
  uint8_t language;
  uint8_t hourFormat;
  uint8_t dateFormat;
  uint8_t dateSeparator;
};

inline bool pcWriteStatus(char* buffer, size_t size, const PcDeviceStatus& status) {
  PcJsonWriter w(buffer, size);
  w.raw("{\"uptime_ms\":"); w.number(status.uptimeMs);
  w.raw(",\"free_heap_bytes\":"); w.number(status.freeHeap);
  w.raw(",\"usb_connected\":"); w.boolean(status.usbConnected);
  w.raw(",\"time_valid\":"); w.boolean(status.timeValid);
  w.raw(",\"pc_synced\":"); w.boolean(status.pcSynced);
  w.raw(",\"epoch\":");
  if (status.timeValid) w.number(status.epoch); else w.raw("null");
  w.raw(",\"timezone\":"); w.text(status.timezone);
  w.raw(",\"active_face\":"); w.text(status.activeFace);
  w.raw(",\"settings\":{\"show_seconds\":"); w.number(status.showSeconds);
  w.raw(",\"language\":"); w.number(status.language);
  w.raw(",\"hour_format\":"); w.number(status.hourFormat);
  w.raw(",\"date_format\":"); w.number(status.dateFormat);
  w.raw(",\"date_separator\":"); w.number(status.dateSeparator);
  w.raw("}}");
  return w.finish();
}
