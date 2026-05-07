"""
parse_logs.py
Parses runtime_*_mon_run*.log files from report/ into structured CSV files.
Also computes per-run aggregated statistics into batch_sweep_summary.csv.

Usage: python parse_logs.py   (from any directory; paths are relative to this script)
"""

import re
import csv
import statistics
import os
import sys

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
REPORT_DIR  = os.path.dirname(SCRIPT_DIR)
OUTPUT_DIR  = os.path.join(SCRIPT_DIR, "raw")
os.makedirs(OUTPUT_DIR, exist_ok=True)

RAW_FIELDS = [
    "esp_uptime_ms", "q_used", "q_free", "q_peak",
    "produced_count", "produced_hz", "processed_count", "processed_hz",
    "dropped", "batches", "proc_est_pct",
    "batch_us_avg", "batch_us_min", "batch_us_max",
    "e2e_us_avg", "e2e_us_min", "e2e_us_max",
    "stack_acq_b", "stack_proc_b", "stack_mon_b",
    "heap8_free_b", "heap8_largest_b",
]

MON_PATTERN = re.compile(
    r'I \((\d+)\) main: mon: '
    r'q_used=(\d+) q_free=(\d+) q_peak=(\d+) '
    r'produced=(\d+)\(([0-9.]+)Hz\) processed=(\d+)\(([0-9.]+)Hz\) '
    r'dropped=(\d+) batches=(\d+).*?proc_est=([0-9.]+)% '
    r'batch_us\(avg/min/max\)=([0-9.]+)/(\d+)/(\d+) '
    r'e2e_us\(avg/min/max\)=([0-9.]+)/(\d+)/(\d+) '
    r'stack\(acq/proc/mon\)=(\d+)/(\d+)/(\d+) '
    r'heap8_free=(\d+) heap8_largest=(\d+)',
    re.DOTALL
)


def parse_log(input_path, output_path):
    rows = []
    with open(input_path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = MON_PATTERN.search(line)
            if m:
                rows.append(m.groups())
    with open(output_path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(RAW_FIELDS)
        w.writerows(rows)
    print(f"  {len(rows):4d} rows  ->  {os.path.basename(output_path)}")
    return rows


def steady_state_rows(rows, warmup_rows=5):
    """Drop first N rows (pipeline warm-up) and return the rest."""
    return rows[warmup_rows:]


def aggregate(rows):
    """
    Compute publication statistics from raw parsed rows.
    batch_us_avg (col 11) is the running cumulative average from the firmware.
    We take the final stable value as the mean estimate, and use the global
    min/max for range. The standard deviation is estimated from per-row
    batch_us_avg deltas (individual batch measurements are not exported by
    the firmware, so the cumulative avg is the best available proxy).
    """
    ss = steady_state_rows(rows)
    if not ss:
        return {}

    batch_avgs   = [float(r[11]) for r in ss]
    batch_mins   = [float(r[12]) for r in ss]
    batch_maxs   = [float(r[13]) for r in ss]
    dropped_last = int(ss[-1][8])
    q_peak_last  = int(ss[-1][3])
    heap_min     = min(int(r[20]) for r in ss)
    proc_hz      = [float(r[7]) for r in ss]

    # Final running average = best mean estimate
    mean_us = batch_avgs[-1]
    # Global min / max across all intervals
    global_min = min(batch_mins)
    global_max = max(batch_maxs)
    # Std estimated from range (range / 4 approximation is crude;
    # use std of cumulative average series as a proxy instead)
    std_us = statistics.pstdev(batch_avgs) if len(batch_avgs) > 1 else 0.0

    return {
        "mean_us": round(mean_us, 1),
        "std_us":  round(std_us, 1),
        "min_us":  global_min,
        "max_us":  global_max,
        "dropped_total": dropped_last,
        "q_peak": q_peak_last,
        "heap_free_min": heap_min,
        "proc_hz_mean": round(statistics.mean(proc_hz), 2),
    }


# ---------------------------------------------------------------------------
# 1. Parse B=128, n=5000 (HISTORY_SIZE_S=20, SAMPLING_RATE_HZ=250) - 3 runs
# ---------------------------------------------------------------------------
CONFIGS = [
    # (batch_size, history_size_s, sampling_rate_hz, log_basename, run_id)
    (128, 20, 250, "runtime_prod_o2_mon_run1.log", 1),
    (128, 20, 250, "runtime_prod_o2_mon_run2.log", 2),
    (128, 20, 250, "runtime_prod_o2_mon_run3.log", 3),
]

print("Parsing B=128, n=5000 runs...")
run_records = []
for batch, hist, fs, logname, run_id in CONFIGS:
    n_samples = hist * fs
    in_path  = os.path.join(REPORT_DIR, logname)
    tag      = f"B{batch:03d}_n{n_samples}_run{run_id}"
    out_path = os.path.join(OUTPUT_DIR, f"{tag}.csv")
    rows = parse_log(in_path, out_path)
    stats = aggregate(rows)
    run_records.append({
        "tag": tag, "batch_size": batch, "history_size_s": hist,
        "sampling_rate_hz": fs, "n_samples": n_samples,
        "n_bytes": n_samples * 4,
        "run_id": run_id, **stats
    })

# ---------------------------------------------------------------------------
# 2. Check for B=1 raw CSV (created separately from serial capture)
# ---------------------------------------------------------------------------
b1_path = os.path.join(OUTPUT_DIR, "B001_n5000_run1.csv")
if os.path.exists(b1_path):
    print(f"\nFound existing B001 raw CSV: {b1_path}")
    with open(b1_path, newline="", encoding="utf-8") as f:
        r = csv.DictReader(f)
        b1_rows = [tuple(row.values()) for row in r]
    stats = aggregate(b1_rows)
    run_records.insert(0, {
        "tag": "B001_n5000_run1", "batch_size": 1, "history_size_s": 20,
        "sampling_rate_hz": 250, "n_samples": 5000, "n_bytes": 20000,
        "run_id": 1, **stats
    })
else:
    print(f"\nNOTE: B001 raw CSV not found at {b1_path}")
    print("      Create it manually and re-run this script to include it in the summary.")

# ---------------------------------------------------------------------------
# 3. Write batch_sweep_summary.csv
# ---------------------------------------------------------------------------
SUMMARY_FIELDS = [
    "tag", "batch_size", "history_size_s", "sampling_rate_hz",
    "n_samples", "n_bytes", "run_id",
    "batch_us_mean", "batch_us_std", "batch_us_min", "batch_us_max",
    "dropped_total", "q_peak", "heap_free_min_b", "proc_hz_mean",
]
summary_path = os.path.join(SCRIPT_DIR, "batch_sweep_summary.csv")
with open(summary_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(SUMMARY_FIELDS)
    for r in run_records:
        w.writerow([
            r["tag"], r["batch_size"], r["history_size_s"], r["sampling_rate_hz"],
            r["n_samples"], r["n_bytes"], r["run_id"],
            r.get("mean_us",""), r.get("std_us",""), r.get("min_us",""), r.get("max_us",""),
            r.get("dropped_total",""), r.get("q_peak",""), r.get("heap_free_min",""),
            r.get("proc_hz_mean",""),
        ])
print(f"\nSummary CSV -> {summary_path}")

# ---------------------------------------------------------------------------
# 4. Compute overhead decomposition model for n=5000
#    T(B) = O + C*B  (linear fit over available data points)
# ---------------------------------------------------------------------------
model_points = [(r["batch_size"], r.get("mean_us", None)) for r in run_records
                if r["n_samples"] == 5000 and r.get("mean_us") is not None]
model_points.sort()

model_path = os.path.join(SCRIPT_DIR, "overhead_model_n5000.csv")
MODEL_FIELDS = [
    "batch_size", "n_samples", "T_measured_us", "T_measured_ms",
    "T_model_us", "T_model_ms",
    "overhead_fixed_us", "overhead_fixed_ms",
    "compute_per_sample_us", "compute_per_sample_ms",
    "overhead_pct", "compute_pct",
]

# 2-point linear model using B=1 and B=128 anchor points
anchor_B1   = next((t for b, t in model_points if b == 1),   None)
anchor_B128 = next((t for b, t in model_points if b == 128), None)

if anchor_B1 is None:
    # Use known measured value from this session (2026-05-07)
    anchor_B1 = 56644.5
    print("  Using B=1 anchor from session measurement: 56644.5 µs")
if anchor_B128 is None:
    # Use validated Phase 2 value
    anchor_B128 = 241282.2
    print("  Using B=128 anchor from Phase 2 data: 241282.2 µs")

C_us = (anchor_B128 - anchor_B1) / (128 - 1)   # µs/sample
O_us = anchor_B1 - C_us * 1                     # µs (fixed overhead)

print(f"\nLinear model: T(B) = {O_us:.1f} + {C_us:.3f}*B  (µs)")
print(f"  Fixed overhead O = {O_us/1000:.3f} ms")
print(f"  Compute per sample C = {C_us/1000:.4f} ms/sample")

with open(model_path, "w", newline="", encoding="utf-8") as f:
    w = csv.writer(f)
    w.writerow(MODEL_FIELDS)
    for b_val in [1, 8, 16, 32, 64, 128, 256]:
        t_model = O_us + C_us * b_val
        t_meas  = next((t for bv, t in model_points if bv == b_val), "")
        ovh_pct = round(O_us / t_model * 100, 1)
        cmp_pct = round(C_us * b_val / t_model * 100, 1)
        w.writerow([
            b_val, 5000,
            round(t_meas, 1) if t_meas != "" else "",
            round(t_meas / 1000, 3) if t_meas != "" else "",
            round(t_model, 1), round(t_model / 1000, 3),
            round(O_us, 1), round(O_us / 1000, 3),
            round(C_us, 3), round(C_us / 1000, 4),
            ovh_pct, cmp_pct,
        ])
print(f"Model CSV -> {model_path}")
print("\nDone.")
