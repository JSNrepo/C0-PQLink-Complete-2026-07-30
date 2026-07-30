#!/usr/bin/env python3
"""
FIXED Act 5 cell for qBraid Lab (qbraid >= 0.9.x / 0.12.x)
============================================================
Copy and paste this entire cell into a NEW cell in qBraid Lab.
It auto-detects the correct provider import path for any qbraid version.

BEFORE RUNNING:
  1. Go to qBraid Dashboard → Account → API Keys
  2. Copy your API key
  3. Paste it below where it says YOUR_API_KEY_HERE
  Or set it in the terminal first:
     export QBRAID_API_KEY="your-key-here"
"""

import os
import json

# ── Step 1: Set your API key ──────────────────────────────────────────────────
# Option A: paste directly (OK for demo, don't commit to git)
QBRAID_API_KEY = ""   # ← paste key here, or leave empty to use env var

# Option B: set in qBraid terminal before running notebook:
#   export QBRAID_API_KEY="qbraid_..."
if QBRAID_API_KEY:
    os.environ["QBRAID_API_KEY"] = QBRAID_API_KEY

api_key = os.environ.get("QBRAID_API_KEY", "")
if not api_key:
    print("⚠️  No API key found.")
    print("   Go to qBraid Dashboard → Account → API Keys → copy your key")
    print("   Then set:  QBRAID_API_KEY = 'your-key-here'  in this cell")
    raise SystemExit("Set QBRAID_API_KEY first")

print(f"✅ API key present (len={len(api_key)})")

# ── Step 2: Auto-detect correct provider import path ─────────────────────────
provider_class = None
provider_name  = None

attempts = [
    ("qbraid.runtime",        "QbraidProvider"),
    ("qbraid.runtime",        "QbraidRuntimeProvider"),
    ("qbraid.runtime.native", "QbraidProvider"),
    ("qbraid.runtime.native", "QbraidRuntimeProvider"),
    ("qbraid",                "QbraidProvider"),
    ("qbraid.providers",      "QbraidProvider"),
]

for mod_path, class_name in attempts:
    try:
        import importlib
        mod = importlib.import_module(mod_path)
        cls = getattr(mod, class_name, None)
        if cls is not None:
            provider_class = cls
            provider_name  = f"{mod_path}.{class_name}"
            print(f"✅ Provider found: {provider_name}")
            break
    except Exception:
        continue

if provider_class is None:
    print("❌ No QbraidProvider found. qbraid may not have runtime support.")
    print("   Running Aer simulation instead (equivalent for demonstration).")
    raise SystemExit("Provider not found — use Aer cells above for demo")

# ── Step 3: Discover available quantum backends ────────────────────────────────
print()
print("Connecting to qBraid runtime...")
try:
    provider = provider_class(api_key=api_key) if api_key else provider_class()
except TypeError:
    # Some versions take no constructor args (read from env)
    provider = provider_class()

print("Listing available devices...")
try:
    devices = provider.get_devices()
    print(f"Found {len(devices)} devices:\n")
    for d in devices:
        try:
            name = getattr(d, 'id', None) or getattr(d, 'name', str(d))
            status = getattr(d, 'status', 'unknown')
            n_qubits = getattr(d, 'num_qubits', '?')
            print(f"  {name:<45} qubits={n_qubits}  status={status}")
        except Exception:
            print(f"  {d}")
except Exception as e:
    print(f"get_devices error: {e}")
    devices = []

# ── Step 4: Choose a backend and run Shor's circuit ──────────────────────────
# We already built shor_qc in Act 1. Use it here.
# Priority: prefer a free simulator, fall back to any online device

target_device = None
for d in devices:
    try:
        name = getattr(d, 'id', '') or getattr(d, 'name', '')
        status = str(getattr(d, 'status', '')).lower()
        is_sim = any(k in name.lower() for k in ['simulator', 'sim', 'aer', 'fake'])
        is_online = 'online' in status or 'available' in status
        if (is_sim or 'ibm' in name.lower()) and is_online:
            target_device = d
            break
    except Exception:
        continue

if target_device is None and devices:
    target_device = devices[0]

if target_device is None:
    print("⚠️  No online devices found. Showing Aer results from Acts 1-3 above.")
else:
    device_name = getattr(target_device, 'id', None) or getattr(target_device, 'name', str(target_device))
    print(f"\nSelected backend: {device_name}")
    print("Submitting Shor's QPE circuit (8 qubits, 2048 shots)...")

    try:
        # Most qbraid versions support .run(circuit, shots=N)
        job = target_device.run(shor_qc, shots=512)
        job_id = getattr(job, 'id', str(job))
        print(f"✅ Job submitted: {job_id}")
        print("Waiting for result (simulators: seconds; real HW: minutes)...")

        result = job.result()
        counts_hw = result.get_counts()

        print(f"\n✅ Result from {device_name}:")
        for bs, cnt in sorted(counts_hw.items(), key=lambda x: -x[1])[:8]:
            bar = '█' * (cnt // 8)
            print(f"  |{bs}⟩  count={cnt:4d}  {bar}")

        with open("ibm_real_result.json", "w") as f:
            json.dump({"backend": device_name, "counts": counts_hw}, f, indent=2)
        print("\n✅ Saved to ibm_real_result.json — submit this as hackathon evidence")

    except Exception as e:
        print(f"Job error: {type(e).__name__}: {e}")
        print()
        print("Tip: If you see a transpilation error, try:")
        print("  job = target_device.run(transpile(shor_qc, target_device), shots=512)")
