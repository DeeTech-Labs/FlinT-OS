# USB time sync and Spark status LED

Spark uses an ESP32-C3 Super Mini with native USB Serial/JTAG. The firmware
contains no Wi-Fi driver, IP stack, or SNTP client. The PC supplies its UTC
clock over the same serial port used by the shell. The configured device
timezone is applied only for display; changing it does not change UTC.

## PC helper

Use Python 3 with `pyserial` (`python3 -m pip install pyserial`), or the existing
ESP-IDF Python environment, which already includes it. From the project root:

```bash
python3 tools/sync_time.py
python3 tools/sync_time.py --port /dev/ttyACM0
python3 tools/sync_time.py --watch
```

Without `--port`, the helper discovers Espressif USB Serial/JTAG devices and
checks the FlinT handshake before sending time. It never resets the device.
`--watch` syncs on connection/re-enumeration, retries failures, and resyncs hourly.
It closes the port after each attempt so other tools can use the console.
Avoid running a serial monitor or flasher during a sync attempt; close/stop
them and the helper will retry. The one-shot command exits nonzero on failure.

USB itself supplies no clock/date information. Sync requires this helper on
any PC that should supply time. Run once for a single sync, or leave `--watch`
running in a terminal and stop it with Ctrl+C. No service or startup task is
installed. Windows ports can be supplied as `--port COM5`.

## LED and wiring

The active-low blue LED is GPIO8 in the existing `flint_spark` board profile
([Super Mini hardware reference](https://nuttx.apache.org/docs/latest/platforms/risc-v/esp32c3/boards/esp32c3-supermini/index.html)).
It is off without a USB host, including power-bank/charger power. While a host
sends USB SOF packets, it blinks slowly (500 ms on/off), even with the serial
port closed. After a valid time command is applied, it flashes three times
(80 ms per on/off phase), pauses, then resumes slow blinking. USB detach
cancels the confirmation pattern. Host suspend can also stop USB SOF packets.

TFT pins: SCK=6, MOSI=7, CS=10, DC=4, RST=3. **GPIO8 is reserved for the LED**;
move an existing display's RST wire from GPIO8 to GPIO3 and DC from GPIO9 to
GPIO4. GPIO9 is a boot strap and is no longer driven by the display.

## Serial protocol v1

ASCII frames start with `@` and end with LF; CRLF is accepted:

```text
PC:     @HELLO
Device: FLINT HELLO 1 flint_spark
PC:     @TIME 1792249200
Device: FLINT TIME OK 1792249200
```

Epochs must be ten decimal digits in the range 1700000000..4102444799.
Invalid, oversized, or stalled (>2 s) commands return `FLINT ERROR invalid command`
and do not alter the clock or trigger the LED confirmation. Framed bytes are
never interpreted as shell navigation. A new `@` recovers from an incomplete
frame. Ordinary shell keys work outside frames.

The device keeps elapsed time using a 64-bit monotonic timer, persists the UTC
epoch on sync and at most once a minute while running, and restores it after
reset. Without a battery-backed RTC it cannot know time spent powered off;
the next PC sync corrects this. Manual editing clears the PC-synced status.

## Device data for FlinT Desktop

Two read-only commands extend protocol v1 without changing HELLO or TIME:

- `@INFO\n` returns `FLINT INFO {JSON}\n`: protocol, stable eFuse-based id, model,
  board, chip, firmware version, API, configured screen size and capabilities
  (`info`, `status`, `time_sync`). Reading the factory MAC does not enable radio.
- `@STATUS\n` returns `FLINT STATUS {JSON}\n`: 64-bit `uptime_ms`, `free_heap_bytes`,
  `usb_connected`, `time_valid`, `pc_synced`, UTC `epoch` (null when unset),
  `timezone`, `active_face`, and `settings` (`show_seconds`, `language`,
  `hour_format`, `date_format`, `date_separator`).

Replies are single-line JSON, bounded and escaped; serialization failure returns
`FLINT ERROR` instead of a truncated document. Queries do not write settings or
flash. FlinT Desktop polls STATUS every two seconds and supports manual JSON
capture export. Only one command should be in flight per client. Diagnostic
logs can appear between replies; clients must match the `FLINT` response prefix.

FlinT Desktop's `docs/USB-DATA.md` documents the fields and capture file format.
Keep the serial monitor and Python time helper closed while the app owns the port.
