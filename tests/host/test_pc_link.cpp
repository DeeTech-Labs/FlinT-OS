#include "usb/pc_protocol.h"
#include "usb/status_pattern.h"
#include "host_check.h"

#include <string>

static PcCommand command(PcProtocol& protocol, const std::string& line, uint32_t now = 0) {
  PcCommand out;
  for (char c : line) hostRequire(protocol.feed(c, now, out), "frame byte consumed");
  return out;
}

int main() {
  PcProtocol parser;
  PcCommand out;
  hostRequire(!parser.feed('s', 0, out), "shell keys pass through");
  hostRequire(command(parser, "@HELLO\r\n").kind == PcCommandKind::Hello, "CRLF handshake");
  out = command(parser, "@TIME 1792249200\n");
  hostRequire(out.kind == PcCommandKind::Time && out.epoch == 1792249200, "UTC epoch");
  for (const char* invalid : {"@TIME -792249200\n", "@TIME 1792249200x\n",
       "@TIME 9999999999\n", "@TIME 1699999999\n", "@TIME 4102444800\n",
       "@TIME 179224920\n", "@TIME 1792249s00\n", "@UNKNOWN\n"}) {
    hostRequire(command(parser, invalid).kind == PcCommandKind::Error, "reject invalid command");
  }
  hostRequire(command(parser, "@TIME 1700000000\n").kind == PcCommandKind::Time, "lower bound");
  hostRequire(command(parser, "@TIME 4102444799\n").kind == PcCommandKind::Time, "upper bound");
  hostRequire(command(parser, "@" + std::string(100, 's') + "\n").kind == PcCommandKind::Error,
              "oversized frame cannot enter settings");
  command(parser, "@TIME 179", 10);
  hostRequire(command(parser, "2249200\n", 2011).kind == PcCommandKind::Error, "stalled frame rejected");
  command(parser, "@broken", 0);
  hostRequire(command(parser, "@HELLO\n", 3000).kind == PcCommandKind::Hello, "new frame recovers");
  command(parser, "@partial", 0);
  parser.reset();
  hostRequire(!parser.feed('h', 1, out), "disconnect clears partial input");
  command(parser, "@HEL", UINT32_MAX - 5);
  hostRequire(command(parser, "LO\n", 5).kind == PcCommandKind::Hello, "millis wrap");

  PcStatusPattern led;
  hostRequire(!led.on(false, 0), "LED off without host");
  hostRequire(led.on(true, 0) && !led.on(true, 500) && led.on(true, 1000), "slow blink");
  for (uint32_t start : {1000u, UINT32_MAX - 100u}) {
    led.synced(start);
    int flashes = 0;
    bool previous = false;
    for (uint32_t elapsed = 0; elapsed < 800; ++elapsed) {
      const bool current = led.on(true, start + elapsed);
      if (current && !previous) ++flashes;
      previous = current;
    }
    hostRequire(flashes == 3, "exactly three flashes, including millis wrap");
    hostRequire(led.on(true, start + 800), "resume connected blink");
  }
  led.synced(0);
  hostRequire(led.on(true, 80), "first sync flash");
  hostRequire(!led.on(false, 81), "disconnect cancels sync pattern");
  hostRequire(!led.on(false, 10000), "stays off on USB power only");
  std::puts("ok test_pc_link");
}
