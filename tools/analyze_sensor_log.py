#!/usr/bin/env python3
"""
analyze_sensor_log.py

Reads a sensor_log_*.csv file written by train_dashboard.py (one row
per distance_sensor reading: timestamp, sensor_id, distance_mm, valid)
and produces a report per sensor:

  - Sample rate (readings/sec, and interval statistics)
  - Shortest and longest distance measured
  - Whether readings change smoothly as the train passes (grouped into
    "passes" - runs of in-calibration-range readings - each checked
    for monotonic progression, since the train runs the same route in
    the same direction every time for now)
  - Suspicious readings: large jumps between consecutive samples,
    single-sample reversals against an otherwise smooth trend, and
    invalid/out-of-range readings

Usage:
    python analyze_sensor_log.py                  # uses the most recent
                                                    # log in ./sensor_logs/
    python analyze_sensor_log.py path/to/file.csv  # analyze a specific file
"""

import csv
import sys
from pathlib import Path

# Calibration constants - kept in sync with train_dashboard.py by hand
# (not imported from it) so this script has no dependency on tkinter
# or any of the dashboard's other imports, and can run anywhere.
SENSOR_ZONE_MIN_MM = 300
SENSOR_ZONE_MAX_MM = 1150

# How much a reading is allowed to move backward within a pass before
# it's flagged as a reversal - small values are just sensor jitter,
# not a real direction change (the train doesn't reverse mid-pass).
REVERSAL_TOLERANCE_MM = 15

# A jump between two consecutive readings larger than this, taken less
# than MAX_JUMP_INTERVAL_S apart, is flagged as suspicious - it's
# larger than a slow-moving model train should cover in that little
# time. Both numbers are starting estimates, not measured - tune them
# once you've seen a few real passes and know the train's actual speed.
SUSPICIOUS_JUMP_MM = 150
MAX_JUMP_INTERVAL_S = 0.5

# A gap with no readings at all longer than this ends a pass (train
# has left the monitored zone) even if the next reading would
# otherwise look like a continuation.
PASS_GAP_SECONDS = 2.0


def find_latest_log():
    log_dir = Path(__file__).resolve().parent / "sensor_logs"
    if not log_dir.exists():
        return None
    candidates = sorted(log_dir.glob("sensor_log_*.csv"))
    return candidates[-1] if candidates else None


def load_rows(path):
    rows = []
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            ts = float(row["timestamp"])
            sensor_id = row["sensor_id"]
            valid = row["valid"].strip().lower() == "true"
            dist_str = row["distance_mm"].strip()
            distance_mm = int(dist_str) if dist_str else None
            rows.append({"ts": ts, "sensor_id": sensor_id, "distance_mm": distance_mm, "valid": valid})
    return rows


def in_calibration_range(distance_mm):
    return distance_mm is not None and SENSOR_ZONE_MIN_MM <= distance_mm <= SENSOR_ZONE_MAX_MM


def group_into_passes(rows):
    """Split a sensor's rows into passes: contiguous runs of valid,
    in-calibration-range readings, broken by an invalid/out-of-range
    reading or a time gap larger than PASS_GAP_SECONDS."""
    passes = []
    current = []
    prev_ts = None
    for r in rows:
        usable = r["valid"] and in_calibration_range(r["distance_mm"])
        gap_broke_it = prev_ts is not None and (r["ts"] - prev_ts) > PASS_GAP_SECONDS
        if not usable or gap_broke_it:
            if current:
                passes.append(current)
                current = []
        if usable:
            current.append(r)
        prev_ts = r["ts"]
    if current:
        passes.append(current)
    return passes


def check_pass_monotonic(pass_rows):
    """Returns (is_smooth, reversal_indices) - reversal_indices are
    positions within this pass where the reading moved backward by
    more than REVERSAL_TOLERANCE_MM against the pass's overall
    direction."""
    if len(pass_rows) < 2:
        return True, []
    net_direction = pass_rows[-1]["distance_mm"] - pass_rows[0]["distance_mm"]
    increasing = net_direction >= 0
    reversals = []
    for i in range(1, len(pass_rows)):
        diff = pass_rows[i]["distance_mm"] - pass_rows[i - 1]["distance_mm"]
        backward = diff < -REVERSAL_TOLERANCE_MM if increasing else diff > REVERSAL_TOLERANCE_MM
        if backward:
            reversals.append(i)
    return len(reversals) == 0, reversals


def find_suspicious_jumps(rows):
    """Large distance jumps between consecutive VALID readings taken
    close together in time - regardless of pass grouping, since a
    jump like this is suspicious whether or not it happens to fall
    inside the calibrated zone."""
    suspicious = []
    valid_rows = [r for r in rows if r["valid"] and r["distance_mm"] is not None]
    for i in range(1, len(valid_rows)):
        a, b = valid_rows[i - 1], valid_rows[i]
        dt = b["ts"] - a["ts"]
        jump = abs(b["distance_mm"] - a["distance_mm"])
        if dt <= MAX_JUMP_INTERVAL_S and jump >= SUSPICIOUS_JUMP_MM:
            suspicious.append((a, b, jump, dt))
    return suspicious


def format_report(sensor_id, rows):
    lines = []
    lines.append(f"=== {sensor_id} ===")
    lines.append(f"Total readings: {len(rows)}")

    if len(rows) < 2:
        lines.append("Not enough readings for further analysis.")
        return "\n".join(lines)

    # --- Sample rate ---
    intervals = [rows[i]["ts"] - rows[i - 1]["ts"] for i in range(1, len(rows))]
    duration = rows[-1]["ts"] - rows[0]["ts"]
    avg_interval = sum(intervals) / len(intervals)
    if duration > 0:
        rate_hz = (len(rows) - 1) / duration
        rate_str = f"{rate_hz:.2f} readings/sec"
    else:
        rate_str = "N/A (readings too close together in time to measure)"
    lines.append(f"Sample rate: {rate_str} "
                 f"(avg interval {avg_interval*1000:.0f}ms, "
                 f"min {min(intervals)*1000:.0f}ms, max {max(intervals)*1000:.0f}ms) "
                 f"over {duration:.1f}s")

    # --- Min/max of valid readings ---
    valid_distances = [r["distance_mm"] for r in rows if r["valid"] and r["distance_mm"] is not None]
    if valid_distances:
        lines.append(f"Shortest distance measured: {min(valid_distances)/10:.1f}cm")
        lines.append(f"Longest distance measured: {max(valid_distances)/10:.1f}cm")
    else:
        lines.append("No valid readings with a distance value at all.")

    invalid_count = sum(1 for r in rows if not r["valid"])
    out_of_range_count = sum(1 for r in rows if r["valid"] and not in_calibration_range(r["distance_mm"]))
    lines.append(f"Invalid readings: {invalid_count}  |  "
                 f"Valid but outside the {SENSOR_ZONE_MIN_MM/10:.0f}-{SENSOR_ZONE_MAX_MM/10:.0f}cm "
                 f"calibrated zone: {out_of_range_count}")

    # --- Passes and monotonicity ---
    passes = group_into_passes(rows)
    lines.append(f"\nDetected {len(passes)} pass(es) through the calibrated zone.")
    smooth_count = 0
    all_reversals = []
    for i, p in enumerate(passes, 1):
        is_smooth, reversals = check_pass_monotonic(p)
        span = f"{p[0]['distance_mm']/10:.1f}cm -> {p[-1]['distance_mm']/10:.1f}cm"
        duration_p = p[-1]["ts"] - p[0]["ts"]
        status = "smooth" if is_smooth else f"{len(reversals)} reversal(s)"
        lines.append(f"  Pass {i}: {len(p)} readings, {span}, {duration_p:.2f}s - {status}")
        if is_smooth:
            smooth_count += 1
        else:
            all_reversals.append((i, p, reversals))
    if passes:
        lines.append(f"{smooth_count}/{len(passes)} passes changed smoothly and consistently "
                     f"(no readings moving backward more than {REVERSAL_TOLERANCE_MM/10:.1f}cm "
                     f"against the pass's overall direction).")

    # --- Suspicious readings ---
    lines.append("\nSuspicious readings:")
    found_any = False

    jumps = find_suspicious_jumps(rows)
    for a, b, jump, dt in jumps:
        found_any = True
        lines.append(f"  Jump of {jump/10:.1f}cm in {dt:.2f}s "
                     f"({a['distance_mm']/10:.1f}cm -> {b['distance_mm']/10:.1f}cm) "
                     f"at t={b['ts']:.2f}")

    for pass_num, p, reversals in all_reversals:
        for idx in reversals:
            found_any = True
            r_prev, r_cur = p[idx - 1], p[idx]
            lines.append(f"  Reversal in pass {pass_num}: "
                         f"{r_prev['distance_mm']/10:.1f}cm -> {r_cur['distance_mm']/10:.1f}cm "
                         f"at t={r_cur['ts']:.2f}")

    if not found_any:
        lines.append("  None found.")

    return "\n".join(lines)


def main():
    if len(sys.argv) > 1:
        path = Path(sys.argv[1])
    else:
        path = find_latest_log()
        if path is None:
            print("No log file given, and no sensor_logs/sensor_log_*.csv found "
                  "next to this script.")
            sys.exit(1)
        print(f"Using most recent log: {path}\n")

    if not path.exists():
        print(f"File not found: {path}")
        sys.exit(1)

    rows = load_rows(path)
    if not rows:
        print("Log file is empty.")
        sys.exit(0)

    by_sensor = {}
    for r in rows:
        by_sensor.setdefault(r["sensor_id"], []).append(r)

    for sensor_id in sorted(by_sensor):
        print(format_report(sensor_id, by_sensor[sensor_id]))
        print()


if __name__ == "__main__":
    main()
