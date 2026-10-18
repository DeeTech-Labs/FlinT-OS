# FlinT companion protocol: USB + BLE MVP

## Implemented wire format

USB and BLE use the same bounded command parser and application dispatcher.
The existing `HELLO 1` / `INFO.protocol = 1` wire version is deliberately retained;
legacy USB clients and firmware remain compatible. This is not a claim that
Actions, file transfer or OTA are already implemented.

Client sends one LF-terminated ASCII command at a time:

```text
@HELLO\n
@INFO\n
@STATUS\n
@TIME 1792249200\n
```

Responses are LF-terminated `FLINT HELLO 1 <board>`, `FLINT INFO <json>`,
`FLINT STATUS <json>`, `FLINT TIME OK <epoch>` or `FLINT ERROR <message>`.
TIME accepts UTC seconds in [1700000000, 4102444799]. INFO capabilities determine
available operations. STATUS keeps `usb_connected` as the physical USB state;
it does not mean that a BLE session is connected.

Only one request is outstanding. Desktop verifies the TIME acknowledgement and
refreshes STATUS after synchronization. Disconnect clears partial input and
pending operations; a new INFO starts a new capture. Polling is every two seconds
in this MVP, only while Desktop is connected. Optional tray mode keeps polling while the window is hidden; no operating-system autostart is installed.

## BLE GATT binding

- Primary service: `e7f10001-7a1e-4b5c-9d2f-46544c494e54`
- RX command: `e7f10002-7a1e-4b5c-9d2f-46544c494e54`, Write With Response,
  encrypted writes required.
- TX response: `e7f10003-7a1e-4b5c-9d2f-46544c494e54`, Indicate.
  Encrypted CCCD writes are required. Firmware initiates security on connection
  so subscribing completes before HELLO can be sent.
- Advertised name: `FlinT Spark`; discovery prefers the service UUID. If BlueZ
  has not supplied UUIDs yet, the exact name is a provisional candidate. The
  service and both characteristics are always validated before protocol exchange.
- The current firmware uses a public ESP32 identity with privacy disabled. Linux
  Desktop explicitly selects PublicAddress, so CAP_NET_ADMIN is not required
  to infer the address type for these devices.
- Subscribe to TX with CCCD `02 00` before sending HELLO.
- Requests are split into writes of at most 20 bytes; the receiver waits for LF.
- Responses are fragmented into at most 20 bytes per indication. Reassemble until
  LF; an indication boundary is not a message boundary. Indication confirmation
  supplies backpressure; only one indication is in flight.
- Desktop bounds a received line at 8192 bytes; firmware response buffer is 896
  bytes. Neither endpoint treats an incomplete line as a complete response.
- Firmware queues one command from the NimBLE task to the application task.
  Settings and clock access stay on the application task alongside USB handling.
- One BLE central at a time. USB may remain connected. Concurrent clients execute
  sequentially on the app task; the last accepted TIME wins.
- Desktop scan duration: 12 seconds. Connection/discovery timeout: 30 seconds; initial HELLO allows 30 seconds
  for system pairing; later commands allow 8 seconds. Automatic reconnect retries
  the selected device every 15 seconds for the current app session. Disconnect
  stops retries. Rescan if the device address changes. Restart persistence and
  multi-central connections are future work.

## Pairing boundary

This MVP uses encrypted Secure Connections Just Works (no input/output), without
persistent bonding or authenticated peer identity. Nearby clients can pair, change the time, settings and face design. BLE allows
SETTINGS/SET/FACES/FACE/TIMEZONES/STUDIO only; other request operations return
`authenticated_transport_required`. The `companion` capability is advertised
only on USB. Notes and Companion commands are limited to USB until authenticated
pairing is available. No remote PC command execution, notifications or DFU are exposed.
Before adding those features, implement physical pairing confirmation, bonding,
authenticated authorization and per-command policy. Do not advertise Just Works
as protection against an active intermediary.

## Firmware build and hardware validation

ESP-IDF 6.0.2 / ESP32-C3 is the locally compiled target. Enable `CONFIG_BT_ENABLED`
and `CONFIG_BT_NIMBLE_ENABLED`; defaults select one peripheral connection. Existing
sdkconfig files keep previous values: use menuconfig to enable NimBLE, or configure
a fresh build with the repository defaults. Disabling NimBLE keeps USB operational.
NVS initialization errors in the BLE module disable BLE without erasing stored data.
`CONFIG_BT_NIMBLE_MAX_BONDS` must be at least 1: NimBLE checks store capacity
before negotiating non-bonding pairing. This reserves RAM only; runtime bonding
and NVS key persistence remain disabled. Zero capacity prevents pairing.

Validate on hardware: scan, pair, HELLO/INFO/STATUS, TIME acknowledgement, remove USB
while powered independently, drop/reconnect BLE, cancel pairing, restart either
side, test the smallest MTU, and run USB and BLE clients together. Also measure
free heap, frame rate and power. ESP32-S3 and Windows/macOS require their own
build/device checks; the current board catalog targets C3.

## Protocol v1 request extension

All protocol versions remain 1 during active development. Devices advertising
`control` support correlated requests alongside existing HELLO/INFO/STATUS/TIME:

```text
@REQ 1 SETTINGS\n
@REQ 2 SET show_seconds=1 language=1 hour_format=0 timezone=Europe/Moscow\n
@REQ 3 FACES\n
@REQ 4 FACE digital\n
@REQ 5 TIMEZONES\n
```

Responses use `FLINT RESULT {"id":1,"ok":true,"result":...}` or
`FLINT RESULT {"id":1,"ok":false,"error":"invalid_setting"}` followed by LF.
Request IDs are positive integers up to 999999999. Only uppercase operation names
are allowed. Arguments are bounded ASCII tokens; the full line is at most 352
bytes. Settings are validated as a complete patch before any write and persistence
must succeed before acknowledgement. Unsupported settings/operations return errors.
BLE fragments request lines into at most 20 bytes per acknowledged ATT write.
Firmware reassembles until LF; incomplete frames expire after two seconds.

Future operations (files, Actions, DFU) will extend the same version 1 protocol
with capability negotiation, explicit limits, authorization and recovery rules.

## Face Studio v1

A device advertising `face_studio` accepts `@REQ <id> STUDIO <bg> <fg>
<timeY> <dateY> <showDate> <pixels>\n` as one logical line. Colors are six hex
digits, timeY is 80–150, dateY is 170–220, showDate is 0/1 and pixels is exactly
64 hex digits encoding a 16x16 one-bit icon (row-major, MSB first). The firmware
validates all fields, atomically stores `/cfg/studio.cfg`, then acknowledges.
`FACE studio` selects the installed design. The icon starts at y=16, scaled 3x;
text positions are baselines in the 280x240 reference canvas. This MVP has one
static icon layer, time and optional date. It does not implement arbitrary code,
animation, full-color bitmaps, a framebuffer mirror or general file transfer.

Desktop `.flintface` files are JSON with schema `flint.face/1`, a name, background,
foreground, timeY, dateY, showDate and pixels. Import is bounded to 8192 bytes and
validated before replacing the editor. Device transfer excludes the local name.

## USB Companion v1

The `companion` capability enables these correlated operations on trusted USB:

- `APPS`: returns note, focus_remaining (seconds), cpu and memory (percent; -1 unknown).
- `NOTE <hex>`: up to 96 UTF-8 bytes encoded as hex; empty clears the note.
  Stored atomically in `/cfg/note.txt`; NUL and control bytes are rejected.
- `FOCUS <seconds>`: 0 stops, 1–86400 starts a monotonic timer. It continues
  after disconnect but resets on firmware reboot.
- `PC <cpu> <memory>`: each integer 0–100. Linux Desktop sends metrics only
  after the user enables this feature.
- `SHOW note|focus|computer`: selects Companion display mode.
- `FIND`: flashes the existing status indicator for five seconds; returns
  `unsupported_hardware` if no indicator is available.
- `FACE companion`: activates the built-in Companion face.

The local simulator implements the same operations using protocol/schema v1.
Desktop stores up to 200 timeline events locally, without note/clipboard contents.
Clipboard transmission requires an explicit user action. This is not an Actions
or arbitrary-command execution API.

USB diagnostic operation `@REQ <id> BLE` returns the firmware BLE startup state,
including `advertising`, `connected`, `gatt_failed`, and initialization/address failures.
It also includes `disconnect_reason` (NimBLE/HCI code) and `encryption_status`
(-1 not attempted, -2 pending, 0 success, other values are NimBLE errors).
Firmware logs the NimBLE error codes for failed GATT or advertising setup.

Linux Desktop registers a scoped BlueZ NoInputNoOutput agent for its explicit
connection request. It authorizes Just Works only for the selected address and
GATT/FlinT services, rejects PIN/passkey/numeric-comparison requests, and unregisters
on close. It does not replace the system default agent or authenticate ownership.
