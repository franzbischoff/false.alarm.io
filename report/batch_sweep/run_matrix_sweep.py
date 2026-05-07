"""
run_matrix_sweep.py

Executes a complete batch-size/history sweep on hardware and captures raw logs/CSVs.

Workflow per point:
1) Generate a temporary project config for the target batch/history point
2) Build + upload firmware using that temporary config
3) Capture serial monitor output for a fixed duration
4) Parse mon: lines into structured CSV

Default matrix:
- batch sizes:   [1, 8, 16, 32, 64, 128]
- history sizes: [10, 20, 40] seconds (n = history * 250)
- runs per point: 1

Usage:
  python report/batch_sweep/run_matrix_sweep.py
  python report/batch_sweep/run_matrix_sweep.py --runs 3
  python report/batch_sweep/run_matrix_sweep.py --capture-seconds 95
"""

from __future__ import annotations

import argparse
import csv
import os
import re
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


PIO_EXE = r"C:\.platformio\penv\Scripts\platformio.exe"
ENV_NAME = "esp32_prod_o2"
SERIAL_PORT = "COM10"
SERIAL_BAUD = "115200"
SAMPLING_RATE_HZ = 250

BATCH_SIZES = [1, 8, 16, 32, 64, 128]
HISTORY_SIZES = [10, 20, 40]

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
    r"I \((\d+)\) main: mon: "
    r"q_used=(\d+) q_free=(\d+) q_peak=(\d+) "
    r"produced=(\d+)\(([0-9.]+)Hz\) processed=(\d+)\(([0-9.]+)Hz\) "
    r"dropped=(\d+) batches=(\d+).*?proc_est=([0-9.]+)% "
    r"batch_us\(avg/min/max\)=([0-9.]+)/(\d+)/(\d+) "
    r"e2e_us\(avg/min/max\)=([0-9.]+)/(\d+)/(\d+) "
    r"stack\(acq/proc/mon\)=(\d+)/(\d+)/(\d+) "
    r"heap8_free=(\d+) heap8_largest=(\d+)"
)


@dataclass
class SweepPoint:
    batch: int
    history_s: int
    run_id: int

    @property
    def n_samples(self) -> int:
        return self.history_s * SAMPLING_RATE_HZ

    @property
    def tag(self) -> str:
        return f"B{self.batch:03d}_n{self.n_samples}_run{self.run_id}"


def run_cmd(cmd: list[str], cwd: Path, log_file: Path | None = None) -> int:
    if log_file is None:
        print(f"$ {' '.join(cmd)}")
        proc = subprocess.run(cmd, cwd=str(cwd))
        return proc.returncode

    print(f"$ {' '.join(cmd)}  > {log_file.name}")
    with log_file.open("w", encoding="utf-8", errors="replace") as f:
        proc = subprocess.run(cmd, cwd=str(cwd), stdout=f, stderr=subprocess.STDOUT)
    return proc.returncode


def write_temp_project_conf(base_ini: Path, out_ini: Path, env_name: str, batch: int, history_s: int,
                            build_dir: Path, build_cache_dir: Path) -> None:
    text = base_ini.read_text(encoding="utf-8")

    env_start = text.find(f"[env:{env_name}]")
    if env_start < 0:
        raise RuntimeError(f"Could not find [env:{env_name}] in {base_ini}")

    next_env = text.find("\n[env:", env_start + 1)
    if next_env < 0:
        next_env = len(text)

    env_block = text[env_start:next_env]
    env_block = re.sub(
        r"(?m)^\s*-DHISTORY_SIZE_S=\d+\s*$",
        f"\t-DHISTORY_SIZE_S={history_s}",
        env_block,
        count=1,
    )
    env_block = re.sub(
        r"(?m)^\s*-DMPX_BATCH_SIZE=\d+\s*$",
        f"\t-DMPX_BATCH_SIZE={batch}",
        env_block,
        count=1,
    )

    if f"-DHISTORY_SIZE_S={history_s}" not in env_block or f"-DMPX_BATCH_SIZE={batch}" not in env_block:
        raise RuntimeError("Failed to patch HISTORY_SIZE_S / MPX_BATCH_SIZE in temp config")

    text = text[:env_start] + env_block + text[next_env:]
    text = re.sub(
        r"(?m)^build_cache_dir\s*=\s*.*$",
        f"build_cache_dir = {build_cache_dir.as_posix()}",
        text,
        count=1,
    )
    text = re.sub(
        r"(?m)^build_dir\s*=\s*.*$",
        f"build_dir = {build_dir.as_posix()}",
        text,
        count=1,
    )

    out_ini.parent.mkdir(parents=True, exist_ok=True)
    out_ini.write_text(text, encoding="utf-8")


def capture_serial_monitor(project_root: Path, out_log: Path, seconds: int) -> int:
    cmd = [
        PIO_EXE,
        "device",
        "monitor",
        "-p",
        SERIAL_PORT,
        "-b",
        SERIAL_BAUD,
        "--filter",
        "time",
    ]

    print(f"$ {' '.join(cmd)}  (capture {seconds}s -> {out_log.name})")
    with out_log.open("w", encoding="utf-8", errors="replace") as f:
        proc = subprocess.Popen(
            cmd,
            cwd=str(project_root),
            stdout=f,
            stderr=subprocess.STDOUT,
        )
        try:
            time.sleep(seconds)
            proc.terminate()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait(timeout=10)
        except Exception:
            proc.kill()
            proc.wait(timeout=10)
            raise
    return 0


def parse_mon_log_to_csv(log_path: Path, csv_path: Path) -> int:
    rows: list[tuple[str, ...]] = []
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = MON_PATTERN.search(line)
        if m:
            rows.append(m.groups())

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(RAW_FIELDS)
        w.writerows(rows)

    return len(rows)


def ensure_dirs(root: Path) -> tuple[Path, Path, Path, Path]:
    sweep_dir = root / "report" / "batch_sweep"
    raw_dir = sweep_dir / "raw"
    logs_dir = sweep_dir / "logs"
    temp_dir = sweep_dir / "temp_configs"
    build_root = sweep_dir / "build_workdirs"
    raw_dir.mkdir(parents=True, exist_ok=True)
    logs_dir.mkdir(parents=True, exist_ok=True)
    temp_dir.mkdir(parents=True, exist_ok=True)
    build_root.mkdir(parents=True, exist_ok=True)
    return raw_dir, logs_dir, temp_dir, build_root


def build_points(runs: int, batch_sizes: list[int], history_sizes: list[int]) -> list[SweepPoint]:
    points: list[SweepPoint] = []
    for history in history_sizes:
        for batch in batch_sizes:
            for run_id in range(1, runs + 1):
                points.append(SweepPoint(batch=batch, history_s=history, run_id=run_id))
    return points


def parse_csv_int_list(text: str | None, defaults: list[int]) -> list[int]:
    if not text:
        return defaults
    return [int(part.strip()) for part in text.split(",") if part.strip()]


def already_done(csv_path: Path) -> bool:
    if not csv_path.exists():
        return False
    try:
        with csv_path.open(encoding="utf-8") as f:
            lines = f.readlines()
        return len(lines) > 1
    except Exception:
        return False


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runs", type=int, default=1, help="Runs per sweep point (default: 1)")
    parser.add_argument("--capture-seconds", type=int, default=95, help="Serial capture duration per run")
    parser.add_argument("--no-resume", action="store_true", help="Do not skip completed CSV files")
    parser.add_argument("--batches", type=str, help="Comma-separated batch sizes override")
    parser.add_argument("--histories", type=str, help="Comma-separated history sizes override (seconds)")
    parser.add_argument("--build-only", action="store_true", help="Build/upload only; skip serial capture and CSV parsing")
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[2]
    platformio_ini = project_root / "platformio.ini"
    raw_dir, logs_dir, temp_dir, build_root = ensure_dirs(project_root)

    if not Path(PIO_EXE).exists():
        print(f"ERROR: PlatformIO executable not found: {PIO_EXE}")
        return 2

    batch_sizes = parse_csv_int_list(args.batches, BATCH_SIZES)
    history_sizes = parse_csv_int_list(args.histories, HISTORY_SIZES)
    points = build_points(args.runs, batch_sizes, history_sizes)

    print(f"Project root: {project_root}")
    print(f"Sweep points: {len(points)}")
    print(f"Capture: {args.capture_seconds}s each")

    completed = 0
    failed = 0

    for idx, p in enumerate(points, start=1):
        csv_path = raw_dir / f"{p.tag}.csv"
        mon_log_path = logs_dir / f"{p.tag}.log"
        build_log_path = logs_dir / f"{p.tag}.build.log"
        temp_ini_path = temp_dir / f"{p.tag}.ini"
        point_build_dir = build_root / p.tag
        point_cache_dir = build_root / ".cache" / p.tag

        if not args.build_only and not args.no_resume and already_done(csv_path):
            print(f"[{idx}/{len(points)}] SKIP {p.tag} (already captured)")
            completed += 1
            continue

        print("-" * 78)
        print(f"[{idx}/{len(points)}] RUN {p.tag}  (B={p.batch}, history={p.history_s}s)")

        write_temp_project_conf(
            platformio_ini,
            temp_ini_path,
            ENV_NAME,
            p.batch,
            p.history_s,
            point_build_dir,
            point_cache_dir,
        )

        rc = run_cmd([
            PIO_EXE,
            "run",
            "-d",
            str(project_root),
            "-c",
            str(temp_ini_path),
            "-e",
            ENV_NAME,
            "-t",
            "upload",
        ], cwd=project_root, log_file=build_log_path)

        if rc != 0:
            print(f"ERROR: upload failed for {p.tag}, see {build_log_path}")
            failed += 1
            continue

        if args.build_only:
            print(f"OK: build/upload succeeded for {p.tag}")
            completed += 1
            continue

        capture_serial_monitor(project_root, mon_log_path, args.capture_seconds)
        row_count = parse_mon_log_to_csv(mon_log_path, csv_path)

        if row_count == 0:
            print(f"WARN: zero mon rows parsed for {p.tag}")
            failed += 1
        else:
            print(f"OK: {p.tag} -> {row_count} mon rows")
            completed += 1

    print("=" * 78)
    print(f"Completed: {completed}")
    print(f"Failed:    {failed}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
