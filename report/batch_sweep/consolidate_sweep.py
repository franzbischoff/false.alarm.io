import csv
import math
import re
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent
RAW_DIR = ROOT / "raw"

RAW_NAME_RE = re.compile(r"^B(?P<batch>\d+)_n(?P<n>\d+)_run(?P<run>\d+)\.csv$")


def to_int(value: str) -> int:
    return int(float(value))


def to_float(value: str) -> float:
    return float(value)


def mean(values):
    return sum(values) / len(values) if values else 0.0


def pstdev(values):
    if len(values) < 2:
        return 0.0
    m = mean(values)
    return math.sqrt(sum((v - m) ** 2 for v in values) / len(values))


def linear_fit_lsq(xs, ys):
    if len(xs) != len(ys) or len(xs) < 2:
        raise ValueError("Need at least two paired points for linear fit")
    x_bar = mean(xs)
    y_bar = mean(ys)
    denom = sum((x - x_bar) ** 2 for x in xs)
    if denom == 0:
        raise ValueError("Degenerate x values for linear fit")
    slope = sum((x - x_bar) * (y - y_bar) for x, y in zip(xs, ys)) / denom
    intercept = y_bar - slope * x_bar
    return intercept, slope


def parse_raw_file(path: Path):
    with path.open(newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))

    if not rows:
        return {
            "row_count": 0,
            "batch_us_mean": 0.0,
            "batch_us_std": 0.0,
            "batch_us_min": 0.0,
            "batch_us_max": 0.0,
            "dropped_total": 0,
            "drop_rate_per_s": 0.0,
            "q_peak_p95": 0,
            "heap8_free_min_b": 0,
            "proc_hz_mean": 0.0,
            "status": "empty",
        }

    warmup = 5
    steady = rows[warmup:] if len(rows) > warmup else rows

    batch_avgs = [to_float(r["batch_us_avg"]) for r in steady]
    batch_mins = [to_float(r["batch_us_min"]) for r in steady]
    batch_maxs = [to_float(r["batch_us_max"]) for r in steady]
    q_peaks = sorted(to_int(r["q_peak"]) for r in steady)
    heap_free = [to_int(r["heap8_free_b"]) for r in steady]
    proc_hz = [to_float(r["processed_hz"]) for r in steady]

    first_uptime = to_int(rows[0]["esp_uptime_ms"])
    last_uptime = to_int(rows[-1]["esp_uptime_ms"])
    elapsed_s = max((last_uptime - first_uptime) / 1000.0, 1e-9)
    dropped_total = to_int(rows[-1]["dropped"])

    # P95 index by nearest-rank method.
    p95_index = max(0, math.ceil(0.95 * len(q_peaks)) - 1)

    return {
        "row_count": len(rows),
        "batch_us_mean": round(batch_avgs[-1], 1),
        "batch_us_std": round(pstdev(batch_avgs), 1),
        "batch_us_min": round(min(batch_mins), 1),
        "batch_us_max": round(max(batch_maxs), 1),
        "dropped_total": dropped_total,
        "drop_rate_per_s": round(dropped_total / elapsed_s, 2),
        "q_peak_p95": q_peaks[p95_index],
        "heap8_free_min_b": min(heap_free),
        "proc_hz_mean": round(mean(proc_hz), 2),
        "status": "ok",
    }


def main():
    run_records = []
    by_point = defaultdict(list)

    for path in sorted(RAW_DIR.glob("B*_n*_run*.csv")):
        match = RAW_NAME_RE.match(path.name)
        if not match:
            continue

        batch = int(match.group("batch"))
        n_samples = int(match.group("n"))
        run_id = int(match.group("run"))
        history_size_s = n_samples // 250

        metrics = parse_raw_file(path)
        row = {
            "tag": path.stem,
            "batch_size": batch,
            "history_size_s": history_size_s,
            "sampling_rate_hz": 250,
            "n_samples": n_samples,
            "n_bytes": n_samples * 4,
            "run_id": run_id,
            **metrics,
        }
        run_records.append(row)
        by_point[(n_samples, batch)].append(row)

    summary_fields = [
        "tag", "batch_size", "history_size_s", "sampling_rate_hz", "n_samples", "n_bytes", "run_id",
        "row_count", "batch_us_mean", "batch_us_std", "batch_us_min", "batch_us_max",
        "dropped_total", "drop_rate_per_s", "q_peak_p95", "heap8_free_min_b", "proc_hz_mean", "status",
    ]

    with (ROOT / "batch_sweep_summary.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=summary_fields)
        writer.writeheader()
        for row in sorted(run_records, key=lambda r: (r["n_samples"], r["batch_size"], r["run_id"])):
            writer.writerow(row)

    agg_fields = [
        "n_samples", "history_size_s", "batch_size", "runs",
        "batch_us_mean", "batch_us_std_across_runs",
        "dropped_total_mean", "q_peak_p95_mean", "heap8_free_min_b_mean", "proc_hz_mean",
    ]
    with (ROOT / "batch_sweep_summary_agg.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=agg_fields)
        writer.writeheader()
        for (n_samples, batch), rows in sorted(by_point.items()):
            writer.writerow(
                {
                    "n_samples": n_samples,
                    "history_size_s": n_samples // 250,
                    "batch_size": batch,
                    "runs": len(rows),
                    "batch_us_mean": round(mean([r["batch_us_mean"] for r in rows]), 1),
                    "batch_us_std_across_runs": round(pstdev([r["batch_us_mean"] for r in rows]), 1),
                    "dropped_total_mean": round(mean([r["dropped_total"] for r in rows]), 1),
                    "q_peak_p95_mean": round(mean([r["q_peak_p95"] for r in rows]), 1),
                    "heap8_free_min_b_mean": round(mean([r["heap8_free_min_b"] for r in rows]), 1),
                    "proc_hz_mean": round(mean([r["proc_hz_mean"] for r in rows]), 2),
                }
            )

    model_fields = [
        "n_samples", "history_size_s", "batch_size", "T_measured_us", "T_measured_ms",
        "T_model_us", "T_model_ms", "overhead_fixed_us", "overhead_fixed_ms",
        "compute_per_sample_us", "compute_per_sample_ms", "overhead_pct", "compute_pct",
    ]

    all_model_rows = []
    grouped_by_n = defaultdict(dict)
    for (n_samples, batch), rows in by_point.items():
        grouped_by_n[n_samples][batch] = mean([r["batch_us_mean"] for r in rows])

    for n_samples in sorted(grouped_by_n.keys()):
        point_map = grouped_by_n[n_samples]
        if 1 not in point_map or 128 not in point_map:
            continue

        t1 = point_map[1]
        t128 = point_map[128]
        c_us = (t128 - t1) / 127.0
        o_us = t1 - c_us

        per_n_rows = []
        for batch in [1, 8, 16, 32, 64, 128]:
            t_model = o_us + c_us * batch
            t_meas = point_map.get(batch)
            overhead_pct = (o_us / t_model * 100.0) if t_model > 0 else 0.0
            compute_pct = 100.0 - overhead_pct
            row = {
                "n_samples": n_samples,
                "history_size_s": n_samples // 250,
                "batch_size": batch,
                "T_measured_us": round(t_meas, 1) if t_meas is not None else "",
                "T_measured_ms": round(t_meas / 1000.0, 3) if t_meas is not None else "",
                "T_model_us": round(t_model, 1),
                "T_model_ms": round(t_model / 1000.0, 3),
                "overhead_fixed_us": round(o_us, 1),
                "overhead_fixed_ms": round(o_us / 1000.0, 3),
                "compute_per_sample_us": round(c_us, 3),
                "compute_per_sample_ms": round(c_us / 1000.0, 4),
                "overhead_pct": round(overhead_pct, 1),
                "compute_pct": round(compute_pct, 1),
            }
            per_n_rows.append(row)
            all_model_rows.append(row)

        with (ROOT / f"overhead_model_n{n_samples}.csv").open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=model_fields)
            writer.writeheader()
            writer.writerows(per_n_rows)

    with (ROOT / "overhead_models_all_n.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=model_fields)
        writer.writeheader()
        for row in sorted(all_model_rows, key=lambda r: (r["n_samples"], r["batch_size"])):
            writer.writerow(row)

    # Alternative fit: least-squares using all available B points for each n.
    lsq_rows = []
    fit_compare_rows = []
    for n_samples in sorted(grouped_by_n.keys()):
        point_map = grouped_by_n[n_samples]
        if len(point_map) < 2:
            continue

        xs = sorted(point_map.keys())
        ys = [point_map[b] for b in xs]
        o_lsq, c_lsq = linear_fit_lsq(xs, ys)

        # Endpoint fit values, if available.
        o_end = None
        c_end = None
        if 1 in point_map and 128 in point_map:
            t1 = point_map[1]
            t128 = point_map[128]
            c_end = (t128 - t1) / 127.0
            o_end = t1 - c_end

        for batch in [1, 8, 16, 32, 64, 128]:
            t_meas = point_map.get(batch)
            t_model = o_lsq + c_lsq * batch
            overhead_pct = (o_lsq / t_model * 100.0) if t_model > 0 else 0.0
            compute_pct = 100.0 - overhead_pct
            lsq_rows.append(
                {
                    "n_samples": n_samples,
                    "history_size_s": n_samples // 250,
                    "batch_size": batch,
                    "T_measured_us": round(t_meas, 1) if t_meas is not None else "",
                    "T_measured_ms": round(t_meas / 1000.0, 3) if t_meas is not None else "",
                    "T_model_us": round(t_model, 1),
                    "T_model_ms": round(t_model / 1000.0, 3),
                    "overhead_fixed_us": round(o_lsq, 1),
                    "overhead_fixed_ms": round(o_lsq / 1000.0, 3),
                    "compute_per_sample_us": round(c_lsq, 3),
                    "compute_per_sample_ms": round(c_lsq / 1000.0, 4),
                    "overhead_pct": round(overhead_pct, 1),
                    "compute_pct": round(compute_pct, 1),
                }
            )

        fit_compare_rows.append(
            {
                "n_samples": n_samples,
                "history_size_s": n_samples // 250,
                "fit_endpoints_overhead_ms": round((o_end or 0.0) / 1000.0, 3) if o_end is not None else "",
                "fit_endpoints_compute_ms_per_sample": round((c_end or 0.0) / 1000.0, 4) if c_end is not None else "",
                "fit_lsq_overhead_ms": round(o_lsq / 1000.0, 3),
                "fit_lsq_compute_ms_per_sample": round(c_lsq / 1000.0, 4),
                "delta_overhead_ms_lsq_minus_endpoints": round((o_lsq - o_end) / 1000.0, 3) if o_end is not None else "",
                "delta_compute_msps_lsq_minus_endpoints": round((c_lsq - c_end) / 1000.0, 4) if c_end is not None else "",
            }
        )

    with (ROOT / "overhead_models_all_n_lsq.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=model_fields)
        writer.writeheader()
        for row in sorted(lsq_rows, key=lambda r: (r["n_samples"], r["batch_size"])):
            writer.writerow(row)

    fit_compare_fields = [
        "n_samples", "history_size_s", "fit_endpoints_overhead_ms", "fit_endpoints_compute_ms_per_sample",
        "fit_lsq_overhead_ms", "fit_lsq_compute_ms_per_sample",
        "delta_overhead_ms_lsq_minus_endpoints", "delta_compute_msps_lsq_minus_endpoints",
    ]
    with (ROOT / "fit_method_comparison.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fit_compare_fields)
        writer.writeheader()
        writer.writerows(sorted(fit_compare_rows, key=lambda r: r["n_samples"]))

    # Compact table intended for direct publication use.
    pub_fields = [
        "n_samples", "history_size_s", "batch_size", "runs", "T_measured_ms",
        "drop_observed", "q_peak_p95", "proc_hz_mean", "overhead_pct", "compute_pct",
    ]
    publication_rows = []
    for (n_samples, batch), rows in sorted(by_point.items()):
        model_match = next(
            (
                m for m in all_model_rows
                if int(m["n_samples"]) == n_samples and int(m["batch_size"]) == batch
            ),
            None,
        )
        publication_rows.append(
            {
                "n_samples": n_samples,
                "history_size_s": n_samples // 250,
                "batch_size": batch,
                "runs": len(rows),
                "T_measured_ms": round(mean([r["batch_us_mean"] for r in rows]) / 1000.0, 3),
                "drop_observed": "yes" if mean([r["dropped_total"] for r in rows]) > 0 else "no",
                "q_peak_p95": round(mean([r["q_peak_p95"] for r in rows]), 1),
                "proc_hz_mean": round(mean([r["proc_hz_mean"] for r in rows]), 2),
                "overhead_pct": model_match["overhead_pct"] if model_match else "",
                "compute_pct": model_match["compute_pct"] if model_match else "",
            }
        )

    with (ROOT / "batch_sweep_publication_table.csv").open("w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=pub_fields)
        writer.writeheader()
        writer.writerows(publication_rows)

    print(f"Wrote {ROOT / 'batch_sweep_summary.csv'}")
    print(f"Wrote {ROOT / 'batch_sweep_summary_agg.csv'}")
    print(f"Wrote {ROOT / 'overhead_models_all_n.csv'}")
    print(f"Wrote {ROOT / 'overhead_models_all_n_lsq.csv'}")
    print(f"Wrote {ROOT / 'fit_method_comparison.csv'}")
    print(f"Wrote {ROOT / 'batch_sweep_publication_table.csv'}")


if __name__ == "__main__":
    main()
