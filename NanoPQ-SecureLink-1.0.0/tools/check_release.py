#!/usr/bin/env python3
"""Fail closed when required NanoPQ release evidence is incomplete."""

from __future__ import annotations

import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "evidence"
PROFILES = ("avr-lms-w4", "avr-lms-w8", "avr-slh")
EXPECTED_VECTOR_BYTES = {
    "cutover_manifest.bin": 48,
    "lms_w4_public.bin": 56,
    "lms_w4_signature.bin": 2348,
    "lms_w8_public.bin": 56,
    "lms_w8_signature.bin": 1292,
    "slhdsa_public.bin": 32,
    "slhdsa_signature.bin": 7856,
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"RELEASE GATE FAILED: {message}")


def main() -> None:
    for name, expected in EXPECTED_VECTOR_BYTES.items():
        vector = ROOT / "tests" / "vectors" / name
        require(vector.is_file(), f"missing {vector.relative_to(ROOT)}")
        require(vector.stat().st_size == expected, f"wrong size for {name}")

    for profile in PROFILES:
        report_path = EVIDENCE / f"{profile}-e2e.json"
        report = json.loads(report_path.read_text())
        require(report["result"] == "PASS", f"{profile} did not execute")
        require(
            report["physical_hardware_label"]
            == "PHYSICAL_NANO_RUN_STILL_REQUIRED",
            f"{profile} physical evidence label is missing",
        )
        memory = report["memory"]
        require(
            memory["executed_total_peak_sram_bytes"] <= 1792,
            f"{profile} exceeds the 1792-byte SRAM gate",
        )
        require(
            memory["executed_sram_headroom_bytes"] >= 256,
            f"{profile} has under 256 bytes SRAM headroom",
        )
        for assertion, passed in report["assertions"].items():
            require(passed is True, f"{profile} assertion failed: {assertion}")

    benchmark_path = EVIDENCE / "same-target-benchmark.csv"
    with benchmark_path.open(newline="") as handle:
        rows = list(csv.DictReader(handle))
    require(len(rows) == 4, "same-target benchmark must have four rows")
    require(rows[0]["result"] == "FAIL", "legacy ML-KEM failure was hidden")
    require(
        all(row["result"] == "PASS_SIMULATOR" for row in rows[1:]),
        "replacement profiles are not all simulator passes",
    )

    required_builds = (
        "build/avr-lms-w4/nanopq.hex",
        "build/avr-lms-w4/nanopq.elf",
        "build/avr-lms-w4/nanopq.map",
        "build/avr-lms-w4/factory-reset.eep",
        "build/avr-lms-w8/nanopq.hex",
        "build/avr-lms-w8/nanopq.elf",
        "build/avr-lms-w8/nanopq.map",
        "build/avr-slh/nanopq.hex",
        "build/avr-slh/nanopq.elf",
        "build/avr-slh/nanopq.map",
        "build/host/nanopq-peer",
    )
    for relative in required_builds:
        require((ROOT / relative).is_file(), f"missing {relative}")

    print(
        "PASS: release gates preserve the failed baseline, all three AVR "
        "execution passes, SRAM limits, vectors, and flashable artifacts"
    )


if __name__ == "__main__":
    main()
