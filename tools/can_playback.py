#!/usr/bin/env python3
"""
Replay an AutoCAN CSV log with original inter-frame timing.

Uses host_mono_s if present, otherwise device_us deltas.
Prints frames to stdout (same CAN line format as firmware) for piping/tools.
"""

from __future__ import annotations

import argparse
import csv
import sys
import time
from pathlib import Path


def load_rows(path: Path) -> list[dict]:
    rows: list[dict] = []
    with path.open("r", encoding="utf-8", newline="") as fh:
        # Skip comment lines starting with #
        filtered = (ln for ln in fh if ln.strip() and not ln.lstrip().startswith("#"))
        reader = csv.DictReader(filtered)
        for row in reader:
            rows.append(row)
    return rows


def timing_key(row: dict) -> float:
    if row.get("host_mono_s") not in (None, ""):
        return float(row["host_mono_s"])
    return float(row["device_us"]) / 1_000_000.0


def to_can_line(row: dict) -> str:
    return (
        f"CAN {row['device_us']} {row['ch']} {row['id']} "
        f"{row['ext']} {row['rtr']} {row['dlc']} {row['data']}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Replay AutoCAN log")
    parser.add_argument("log", type=Path, help="CSV log from can_logger.py")
    parser.add_argument(
        "-s",
        "--speed",
        type=float,
        default=1.0,
        help="Playback speed multiplier (2.0 = 2x faster)",
    )
    parser.add_argument(
        "--no-sleep",
        action="store_true",
        help="Dump as fast as possible (ignore timestamps)",
    )
    args = parser.parse_args()

    if not args.log.exists():
        print(f"File not found: {args.log}", file=sys.stderr)
        return 1

    rows = load_rows(args.log)
    if not rows:
        print("Empty log.", file=sys.stderr)
        return 1

    t_prev = timing_key(rows[0])
    for row in rows:
        t = timing_key(row)
        if not args.no_sleep:
            dt = max(0.0, (t - t_prev) / max(args.speed, 1e-9))
            if dt > 0:
                time.sleep(dt)
        t_prev = t
        print(to_can_line(row), flush=True)

    return 0


if __name__ == "__main__":
    sys.exit(main())
