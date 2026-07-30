#!/usr/bin/env python3
"""Create a deterministic source, evidence, and binary release ZIP."""

from __future__ import annotations

import hashlib
import os
import shutil
import tempfile
import zipfile
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]
VERSION = "1.0.0"
TOP = f"NanoPQ-SecureLink-{VERSION}"
DIST = ROOT / "dist"
OUTPUT = DIST / f"{TOP}.zip"
EXCLUDED_TOP_LEVEL = {"build", "dist", "node_modules"}
EXCLUDED_NAMES = {".npm-cache", "__pycache__", ".DS_Store"}
ARTIFACTS = {
    "build/avr-lms-w4/nanopq.hex": "artifacts/firmware/nanopq-lms-w4.hex",
    "build/avr-lms-w4/nanopq.elf": "artifacts/firmware/nanopq-lms-w4.elf",
    "build/avr-lms-w4/nanopq.map": "artifacts/firmware/nanopq-lms-w4.map",
    "build/avr-lms-w4/factory-reset.eep": (
        "artifacts/firmware/factory-reset.eep"
    ),
    "build/avr-lms-w8/nanopq.hex": "artifacts/firmware/nanopq-lms-w8.hex",
    "build/avr-lms-w8/nanopq.elf": "artifacts/firmware/nanopq-lms-w8.elf",
    "build/avr-lms-w8/nanopq.map": "artifacts/firmware/nanopq-lms-w8.map",
    "build/avr-slh/nanopq.hex": "artifacts/firmware/nanopq-slh.hex",
    "build/avr-slh/nanopq.elf": "artifacts/firmware/nanopq-slh.elf",
    "build/avr-slh/nanopq.map": "artifacts/firmware/nanopq-slh.map",
    "build/host/nanopq-peer": "artifacts/host/nanopq-peer-linux-x86_64",
}
ZIP_TIME = (2026, 7, 30, 0, 0, 0)


def source_files() -> list[Path]:
    files: list[Path] = []
    for path in ROOT.rglob("*"):
        relative = path.relative_to(ROOT)
        if not path.is_file():
            continue
        if relative.parts[0] in EXCLUDED_TOP_LEVEL:
            continue
        if any(part in EXCLUDED_NAMES for part in relative.parts):
            continue
        if path.suffix in {".pyc", ".zip"}:
            continue
        files.append(relative)
    return sorted(files, key=lambda item: item.as_posix())


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def add_file(
    archive: zipfile.ZipFile,
    source: Path,
    archive_name: str,
) -> None:
    info = zipfile.ZipInfo(archive_name, ZIP_TIME)
    executable = bool(source.stat().st_mode & 0o111)
    info.external_attr = ((0o755 if executable else 0o644) & 0xffff) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    archive.writestr(info, source.read_bytes())


def main() -> None:
    for source in ARTIFACTS:
        if not (ROOT / source).is_file():
            raise SystemExit(f"missing release artifact: {source}")

    DIST.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="nanopq-release-") as temporary:
        stage = Path(temporary) / TOP
        stage.mkdir()
        for relative in source_files():
            destination = stage / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, destination)
        for source, destination_name in ARTIFACTS.items():
            destination = stage / destination_name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / source, destination)

        sums = []
        for path in sorted(stage.rglob("*")):
            if path.is_file():
                relative = path.relative_to(stage).as_posix()
                sums.append(f"{sha256(path)}  {relative}")
        (stage / "SHA256SUMS").write_text("\n".join(sums) + "\n")

        with zipfile.ZipFile(
            OUTPUT,
            "w",
            compression=zipfile.ZIP_DEFLATED,
            compresslevel=9,
        ) as archive:
            for path in sorted(stage.rglob("*")):
                if path.is_file():
                    relative = path.relative_to(stage).as_posix()
                    add_file(archive, path, f"{TOP}/{relative}")

    with zipfile.ZipFile(OUTPUT) as archive:
        names = archive.namelist()
        for name in names:
            pure = PurePosixPath(name)
            if pure.is_absolute() or ".." in pure.parts:
                raise SystemExit(f"unsafe ZIP member: {name}")
        if f"{TOP}/SHA256SUMS" not in names:
            raise SystemExit("release ZIP is missing SHA256SUMS")

    print(
        f"PASS: wrote {OUTPUT.relative_to(ROOT)} "
        f"({len(names)} files, sha256={sha256(OUTPUT)})"
    )


if __name__ == "__main__":
    main()
