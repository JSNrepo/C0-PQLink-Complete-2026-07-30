#!/usr/bin/env python3
"""
Run this in qBraid Lab terminal to discover the correct API paths
for qbraid 0.12.x and print the exact working code for Act 5.
"""
import sys
print(f"Python: {sys.version}")

# Check qbraid version
try:
    import qbraid
    print(f"qbraid version: {qbraid.__version__}")
    print(f"qbraid location: {qbraid.__file__}")
except ImportError as e:
    print(f"qbraid not found: {e}")
    sys.exit(1)

# Explore runtime submodule
print()
print("=== qbraid.runtime contents ===")
try:
    import qbraid.runtime as rt
    attrs = [a for a in dir(rt) if not a.startswith('_')]
    print(f"Available: {attrs}")
except Exception as e:
    print(f"qbraid.runtime error: {e}")

# Try different provider import paths
print()
print("=== Provider import probes ===")
attempts = [
    "from qbraid.runtime import QbraidProvider",
    "from qbraid.runtime import QbraidRuntimeProvider",
    "from qbraid.runtime.native import QbraidProvider",
    "from qbraid.runtime.native import QbraidRuntimeProvider",
    "from qbraid import QbraidProvider",
    "from qbraid.providers import QbraidProvider",
]
working = None
for stmt in attempts:
    try:
        exec(stmt)
        print(f"  ✅ WORKS: {stmt}")
        working = stmt
        break
    except Exception as e:
        print(f"  ❌ {stmt}")
        print(f"      {type(e).__name__}: {e}")

# Try DeviceStatus
print()
print("=== DeviceStatus import probes ===")
status_attempts = [
    "from qbraid.runtime import DeviceStatus",
    "from qbraid.runtime.enums import DeviceStatus",
    "from qbraid.runtime.schemas import DeviceStatus",
    "from qbraid._version import __version__",
]
for stmt in status_attempts:
    try:
        exec(stmt)
        print(f"  ✅ WORKS: {stmt}")
        break
    except Exception as e:
        print(f"  ❌ {stmt} → {e}")

# Try listing available devices
print()
print("=== Try get_devices ===")
try:
    import qbraid.runtime as rt
    # Try calling get_devices
    provider_class = None
    for name in ['QbraidProvider', 'QbraidRuntimeProvider']:
        if hasattr(rt, name):
            provider_class = getattr(rt, name)
            print(f"Found provider: {name}")
            break
    
    if provider_class:
        try:
            p = provider_class()
            devices = p.get_devices()
            print(f"Devices available: {len(devices)}")
            for d in devices[:5]:
                print(f"  - {d}")
        except Exception as e:
            print(f"get_devices error: {e}")
            print("(May need API key set as env var QBRAID_API_KEY)")
except Exception as e:
    print(f"Error: {e}")

# Print Qiskit status too
print()
print("=== Qiskit ===")
try:
    from qiskit import __version__ as qv
    from qiskit_aer import AerSimulator
    sim = AerSimulator()
    print(f"Qiskit: {qv}")
    print(f"AerSimulator: {sim.name}")
    print("✅ Qiskit+Aer ready for simulation without real hardware")
except Exception as e:
    print(f"Qiskit error: {e}")
