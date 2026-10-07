#!/usr/bin/env python3
"""
AutoCAN serial logger — timestamps + replayable log.

Reads lines from ESP32-C6 firmware:
  CAN <device_us> <ch> <id_hex> <ext> <rtr> <dlc> <data_hex|->

Writes CSV:
  host_iso,host_mono_s,device_us,ch,id,ext,rtr,dlc,data

Playback uses host_mono_s (or device_us) deltas so timing can be reproduced.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("Install deps: pip install -r tools/requirements.txt", file=sys.stderr)
    raise

CAN_RE = re.compile(
    r"^CAN\s+"
    r"(?P<device_us>\d+)\s+"
    r"(?P<ch>\d+)\s+"
    r"(?P<id>[0-9A-Fa-f]+)\s+"
    r"(?P<ext>[01])\s+"
    r"(?P<rtr>[01])\s+"
    r"(?P<dlc>\d+)\s+"
    r"(?P<data>[0-9A-Fa-f\-]+)\s*$"
)

FIELDNAMES = [
    "host_iso",
    "host_mono_s",
    "device_us",
    "ch",
    "id",
    "ext",
    "rtr",
    "dlc",
    "data",
]


def list_serial_ports() -> None:
    ports = list(list_ports.comports())
    if not ports:
        print("No serial ports found.")
        return
    for p in ports:
        print(f"  {p.device:12} {p.description} [{p.hwid}]")


def parse_can_line(line: str) -> dict | None:
    m = CAN_RE.match(line.strip())
    if not m:
        return None
    return {
        "device_us": m.group("device_us"),
        "ch": m.group("ch"),
        "id": m.group("id").upper().zfill(8),
        "ext": m.group("ext"),
        "rtr": m.group("rtr"),
        "dlc": m.group("dlc"),
        "data": m.group("data").upper(),
    }


def run_logger(port: str, baud: int, out: Path, echo: bool) -> int:
    print(f"Opening {port} @ {baud}")
    print(f"Writing log → {out.resolve()}")
    print("Ctrl+C to stop.\n")

    t0 = time.perf_counter()
    frames = 0

    out.parent.mkdir(parents=True, exist_ok=True)
    new_file = not out.exists() or out.stat().st_size == 0

    with serial.Serial(port, baud, timeout=0.2) as ser, out.open(
        "a", newline="", encoding="utf-8"
    ) as fh:
        writer = csv.DictWriter(fh, fieldnames=FIELDNAMES)
        if new_file:
            fh.write("# AutoCAN log v1\n")
            fh.write("# playback: python tools/can_playback.py <this_file>\n")
            writer.writeheader()
            fh.flush()

        ser.reset_input_buffer()
        try:
            while True:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip("\r\n")
                if not line:
                    continue

                if line.startswith("#"):
                    if echo:
                        print(line)
                    continue

                parsed = parse_can_line(line)
                if not parsed:
                    if echo:
                        print(f"? {line}")
                    continue

                now = datetime.now(timezone.utc).astimezone()
                row = {
                    "host_iso": now.isoformat(timespec="microseconds"),
                    "host_mono_s": f"{time.perf_counter() - t0:.6f}",
                    **parsed,
                }
                writer.writerow(row)
                frames += 1
                if frames % 50 == 0:
                    fh.flush()
                if echo:
                    print(
                        f"{row['host_mono_s']:>12}s  ch{row['ch']}  "
                        f"ID={row['id']}  dlc={row['dlc']}  {row['data']}"
                    )
        except KeyboardInterrupt:
            fh.flush()
            print(f"\nStopped. Frames written: {frames}")
            return 0

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="AutoCAN serial logger")
    parser.add_argument(
        "-p",
        "--port",
        help="Serial port (e.g. COM5 or /dev/ttyACM0). Omit to list ports.",
    )
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=921600,
        help="Baud rate (default: 921600; для USB-CDC почти не важен)",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output CSV path (default: logs/can_YYYYMMDD_HHMMSS.csv)",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="Do not echo frames to console",
    )
    parser.add_argument(
        "-l",
        "--list",
        action="store_true",
        help="List serial ports and exit",
    )
    args = parser.parse_args()

    if args.list or not args.port:
        print("Available serial ports:")
        list_serial_ports()
        if not args.port:
            print("\nExample: python tools/can_logger.py -p COM5")
            return 0 if args.list else 1

    out = args.output
    if out is None:
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        out = Path("logs") / f"can_{stamp}.csv"

    return run_logger(args.port, args.baud, out, echo=not args.quiet)


if __name__ == "__main__":
    sys.exit(main())
