#!/usr/bin/env python3
"""Synchronize FlinT's UTC clock over USB; --watch handles reconnects.

Requires pyserial (already included in the ESP-IDF Python environment).
No reset, flashing, network access, or shell-key input is performed.
"""

from __future__ import annotations

import argparse
import math
import os
import sys
import time

import serial
from serial.tools import list_ports


def wait_line(port, prefix: bytes, timeout: float) -> bytes:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        line = port.readline(512).strip()
        if line.startswith(prefix):
            return line
    raise TimeoutError(f"No response: {prefix.decode()}")


def synchronize(device: str) -> int:
    # Match IDF monitor's safe-open sequence. The OS initially asserts modem
    # lines on open; release RTS before DTR to avoid a native-USB chip reset.
    port = serial.Serial(port=None, baudrate=115200, timeout=0.2, write_timeout=2)
    port.dtr = True
    port.rts = True
    port.port = device
    if os.name == "posix":
        port.exclusive = True
    try:
        port.open()
        port.rts = False
        port.dtr = False
        for attempt in range(3):
            port.write(b"@HELLO\n")
            port.flush()
            try:
                wait_line(port, b"FLINT HELLO 1 ", 2)
                break
            except TimeoutError:
                if attempt == 2:
                    raise
        epoch = int(time.time())
        if not 1700000000 <= epoch <= 4102444799:
            raise ValueError("PC time must be between November 2023 and December 2099")
        port.write(f"@TIME {epoch}\n".encode("ascii"))
        port.flush()
        reply = wait_line(port, b"FLINT TIME OK ", 3)
        if reply != f"FLINT TIME OK {epoch}".encode("ascii"):
            raise ValueError("Unexpected synchronization acknowledgement")
        return epoch
    finally:
        port.close()


def candidates(requested: str | None):
    if requested:
        return [requested] if os.path.exists(requested) or os.name == "nt" else []
    return [p.device for p in list_ports.comports() if (p.vid, p.pid) == (0x303A, 0x1001)]


def device_identity(device: str):
    # A re-enumerated USB device can reuse its tty name; inode creation time
    # distinguishes it even when an unplug/reboot occurs between poll passes.
    if os.name == "posix":
        st = os.stat(device)
        return device, st.st_ino, st.st_ctime_ns
    return device


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="serial port; default: discover Espressif USB devices")
    parser.add_argument("--watch", action="store_true", help="sync automatically on connection")
    parser.add_argument("--interval", type=float, default=3600,
                        help="periodic resync interval in seconds with --watch (default: 3600)")
    args = parser.parse_args(argv)
    if not math.isfinite(args.interval) or args.interval < 1:
        parser.error("--interval must be at least one second")
    next_attempt = {}
    while True:
        present = set()
        success = False
        for device in candidates(args.port):
            identity = None
            try:
                identity = device_identity(device)
                present.add(identity)
                if time.monotonic() < next_attempt.get(identity, 0):
                    continue
                epoch = synchronize(device)
                print(f"{device}: synced UTC {epoch}", flush=True)
                next_attempt[identity] = time.monotonic() + args.interval
                success = True
            except (serial.SerialException, OSError, TimeoutError, ValueError) as exc:
                print(f"{device}: {exc}", file=sys.stderr, flush=True)
                if identity is not None:
                    next_attempt[identity] = time.monotonic() + 10
        if not args.watch:
            return 0 if success else 1
        next_attempt = {k: v for k, v in next_attempt.items() if k in present}
        time.sleep(1)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(0)
