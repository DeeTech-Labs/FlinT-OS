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

Development firmware now requires encrypted, authenticated, bonded Secure Connections.
New pairing requires a 60-second window opened through trusted USB serial input.
This board has no buttons/touch: serial confirmation explicitly emulates local input.
Compare the six-digit code in Desktop with serial, then confirm on both sides.
A scoped DisplayYesNo BlueZ agent rejects unexpected peers and expires unanswered
confirmation after 25 seconds. No automatic acceptance of numeric comparison occurs.
Bonded peers can reconnect without pairing. NVS stores keys across restart.
The `companion` and `actions` capabilities are available over authenticated BLE.
Pairing management and synthetic input remain USB-only, even for bonded BLE peers.

Serial examples (each line ends with LF):

```text
@REQ 1 PAIR open
@REQ 2 PAIR status
@REQ 3 PAIR confirm=123456
@REQ 4 PAIR reject
@REQ 5 PAIR revoke=0:001122334455
```

Use the actual matching code, not the example. Status includes an array of peer
identifiers; copy a returned identifier for revocation. Revocation invalidates
queued BLE work, disconnects the active session and deletes peer keys. USB is
required to manage trust. The firmware does not silently evict existing peers.

## Firmware build and hardware validation

ESP-IDF 6.0.2 / ESP32-C3 is the locally compiled target. Enable `CONFIG_BT_ENABLED`
and `CONFIG_BT_NIMBLE_ENABLED`; defaults select one peripheral connection. Existing
sdkconfig files keep previous values: use menuconfig to enable NimBLE, or configure
a fresh build with the repository defaults. Disabling NimBLE keeps USB operational.
NVS initialization errors in the BLE module disable BLE without erasing stored data.
Defaults reserve three bonds and enable `CONFIG_BT_NIMBLE_NVS_PERSIST`.
Existing sdkconfig files must enable persistence and set the bond capacity explicitly.

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

Future operations (files, DFU) will extend the same version 1 protocol
with capability negotiation, explicit limits, authorization and recovery rules.

## Face Studio v1

A device advertising `face_studio` accepts `@REQ <id> STUDIO <bg> <fg>
<timeY> <dateY> <showDate> <pixels>\n` as one logical line. Colors are six hex
digits, timeY is 80–150, dateY is 170–220, showDate is 0/1 and pixels is exactly
64 hex digits encoding a 16x16 one-bit icon (row-major, MSB first). The firmware
validates all fields, atomically stores `/cfg/studio.cfg`, then acknowledges.
`FACE studio` selects the installed design. The icon starts at y=16, scaled 3x;
text positions are baselines in the 280x240 reference canvas. The basic definition has an icon, time and optional date. Devices additionally
advertising `face_studio_layers` accept seven additional fields after pixels:
`iconX iconY iconScale showIcon order animateIcon alternatePixels`.
Coordinates must keep the icon inside 280x240; scale is 1–6, flags are 0/1,
order is one of six permutations (icon/time/date, icon/date/time, time/icon/date,
time/date/icon, date/icon/time, date/time/icon), and alternatePixels is 64 hex digits.
Two icon frames alternate every 500 ms while the Studio face is active. Designs
without these fields retain the original behavior. This is not arbitrary code,
full-color bitmaps, a framebuffer mirror or general file transfer.

Desktop `.flintface` files are JSON with schema `flint.face/1`, a name, background,
foreground, timeY, dateY, showDate and pixels. Import is bounded to 8192 bytes and
validated before replacing the editor. An optional `layers` object carries the extended layer fields. Device transfer excludes the local name.

## Companion v1

The `companion` capability enables these correlated operations on trusted USB or authenticated/bonded BLE:

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

## Input and Actions

`actions` enables `EVENTS [cursor]`. Without a cursor it returns the current sequence
and no historical events. With a cursor it returns newer events from a bounded
8-event ring. The response includes `sequence`, `lost`, and `events` (`id`, `button`).
Buttons are `primary`, `secondary`, `long`. Clients must skip lost/pre-arming events,
initialize a new cursor after reconnect, and never replay an event twice.

USB-only `BUTTON primary|secondary|long` and serial keys `1|2|3` emulate button input.
USB-only `KEY enter|settings|previous|next|plus|minus|back` routes to the existing
watch shell, with no arbitrary command execution.
Desktop Actions require explicit session arming and configured mappings. Linux
media control uses MPRIS; focus uses the existing watch timer. Presentation keys
require an X11 session and xdotool. Windows/macOS input dispatch is not validated.

## Confirmed screen scenes (`screen_scene`)

Authenticated BLE and USB can read the committed display scene with:

- `SCREEN [known_revision]`: returns `revision`, `available`, `renderer` (SHA-256
  identity of the shared rasterizer and font resources), `width`, `height`,
  `bytes` and FNV-1a `hash`. An unchanged known revision returns only revision
  and `unchanged:true`. Full metadata snapshots the encoded scene for download.
- `SCENE <revision> <byte_offset>`: returns revision, offset and up to 224 binary
  bytes encoded as lowercase hexadecimal in `data`. A snapshot remains stable
  while the live watch screen changes. A newer full SCREEN replaces that snapshot.

The binary scene is bounded to 1,098 bytes: `FS`, version byte 1, operation count,
BE16 width/height/data size, then 18-byte operations and their bounded text/icon
payload. Operations describe rectangles, rounded rectangles, text and 16×16
monochrome bitmaps. The first operation must cover the entire background.
Font glyph masks and geometry come from the identical C++ renderer packaged in
both applications. Text, time, menu selection, settings, animation frame and
other resolved values originate on the watch. Desktop does not advance the
clock independently or substitute an illustrative watch screen.

Scenes become visible to the protocol only after the display transfer completes.
Firmware rasterizes and transfers the union of changed regions, reusing its
existing framebuffer. Native SDK screens without a shared scene report
`available:false`; a renderer/resource mismatch disables reconstructed preview.
FNV checks detect incomplete/corrupt transfers; BLE transport authentication is
still required for private content.

Desktop polls only while Overview/My watch is visible and the window is not
minimized: at most once per second on USB or once per 2.5 seconds on BLE, subject
to other commands. Unchanged scenes require no scene download. A changed scene
uses a small bounded description plus JSON/hex overhead, rather than the 134,400
raw RGB565 bytes of a 280×240 frame. Real battery impact is not yet measured.

### Explicit pixel verification (USB only)

`SHOT start` temporarily pins the actual display buffer and returns revision,
pixel count and FNV-1a hash of canonical big-endian RGB565 bytes.
`PIXELS <revision> <byte_offset>` reads the same 224-byte blocks from that buffer.
Desktop compares every byte with its independently reconstructed scene and saves
the actual pixels as PNG only on this explicit diagnostic path. The screenshot
is a framebuffer verification, not an optical readback of the physical LCD.

`SHOT stop` releases the buffer and presents the latest pending scene. Cancellation
and disconnect handling release it where possible; a firmware watchdog releases
it after 30 seconds even if the client disappears. BLE rejects SHOT and PIXELS
with `usb_only`, even for authenticated peers. No continuous PNG/video streaming
is implemented or used. Concurrent snapshot readers may replace each other's
scene snapshot and must restart on a revision error.

The portable sources live in firmware `components/flint/render`. Run
`tools/sync_desktop_renderer.py` after editing shared sources/fonts to package
Desktop's standalone copy and refresh both renderer identities. Desktop's package
test rejects divergent sources or a stale identity.
