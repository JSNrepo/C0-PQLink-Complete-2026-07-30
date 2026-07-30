#!/usr/bin/env python3
"""Build the reproducible same-target NanoPQ benchmark artifacts."""

from __future__ import annotations

import csv
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "evidence"
ROWS = (
    {
        "profile": "C0 ML-KEM-512 baseline",
        "role": "KEM each session",
        "standard": "FIPS 203",
        "public_key_bytes": 800,
        "ciphertext_or_signature_bytes": 768,
        "flash_bytes": 20118,
        "static_sram_bytes": 2016,
        "executed_peak_stack_bytes": 611,
        "total_peak_sram_bytes": 2627,
        "sram_headroom_bytes": -579,
        "authorization_ms_at_16mhz": "",
        "state_model": "stateless KEM",
        "target_evidence": "link + compiler frame lower bound",
        "result": "FAIL",
    },
    {
        "profile": "NanoPQ LMS H5/W4",
        "role": "rare authorization + PSK session",
        "standard": "RFC 8554 / NIST SP 800-208",
        "public_key_bytes": 56,
        "ciphertext_or_signature_bytes": 2348,
        "flash_bytes": 18188,
        "state_model": "stateful authority; stateless Nano verifier",
        "target_evidence": "full compiled AVR execution",
        "result": "PASS_SIMULATOR",
        "evidence": "avr-lms-w4-e2e.json",
    },
    {
        "profile": "NanoPQ LMS H5/W8",
        "role": "rare authorization + PSK session",
        "standard": "RFC 8554 / NIST SP 800-208",
        "public_key_bytes": 56,
        "ciphertext_or_signature_bytes": 1292,
        "flash_bytes": 18188,
        "state_model": "stateful authority; stateless Nano verifier",
        "target_evidence": "full compiled AVR execution",
        "result": "PASS_SIMULATOR",
        "evidence": "avr-lms-w8-e2e.json",
    },
    {
        "profile": "NanoPQ SLH-DSA-SHA2-128s",
        "role": "rare authorization + PSK session",
        "standard": "FIPS 205",
        "public_key_bytes": 32,
        "ciphertext_or_signature_bytes": 7856,
        "flash_bytes": 19268,
        "state_model": "stateless authority and verifier",
        "target_evidence": "full compiled AVR execution",
        "result": "PASS_SIMULATOR",
        "evidence": "avr-slh-e2e.json",
    },
)

FIELDS = (
    "profile",
    "role",
    "standard",
    "public_key_bytes",
    "ciphertext_or_signature_bytes",
    "flash_bytes",
    "static_sram_bytes",
    "executed_peak_stack_bytes",
    "total_peak_sram_bytes",
    "sram_headroom_bytes",
    "authorization_ms_at_16mhz",
    "state_model",
    "target_evidence",
    "result",
)


def load_rows() -> list[dict[str, object]]:
    output: list[dict[str, object]] = []
    for source in ROWS:
        row = dict(source)
        evidence_name = row.pop("evidence", None)
        if evidence_name:
            report = json.loads((EVIDENCE / str(evidence_name)).read_text())
            if report["result"] != "PASS":
                raise SystemExit(f"{evidence_name}: simulator result is not PASS")
            memory = report["memory"]
            authorization = report["post_quantum_authorization"]
            row.update(
                {
                    "static_sram_bytes": memory["linker_static_sram_bytes"],
                    "executed_peak_stack_bytes": memory[
                        "executed_peak_stack_bytes"
                    ],
                    "total_peak_sram_bytes": memory[
                        "executed_total_peak_sram_bytes"
                    ],
                    "sram_headroom_bytes": memory[
                        "executed_sram_headroom_bytes"
                    ],
                    "authorization_ms_at_16mhz": authorization[
                        "milliseconds_at_16mhz"
                    ],
                }
            )
            if row["total_peak_sram_bytes"] > 1792:
                raise SystemExit(f"{row['profile']}: 1792-byte gate failed")
            if row["sram_headroom_bytes"] < 256:
                raise SystemExit(f"{row['profile']}: 256-byte headroom gate failed")
        output.append(row)
    return output


def main() -> None:
    rows = load_rows()
    csv_path = EVIDENCE / "same-target-benchmark.csv"
    json_path = EVIDENCE / "same-target-benchmark.json"
    with csv_path.open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS)
        writer.writeheader()
        writer.writerows(rows)
    json_path.write_text(json.dumps({"schema": 1, "rows": rows}, indent=2) + "\n")
    print(f"PASS: wrote {csv_path.relative_to(ROOT)} and {json_path.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
