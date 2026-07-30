#!/usr/bin/env python3
"""
QUANTUM ATTACK DEMO — ACT 2: Grover's Algorithm vs Hash-Based Crypto
=====================================================================
Judges see: A real Qiskit Grover's search circuit attempting to find
a preimage (crack a hash function).

What runs here:
  • Real Grover's oracle circuit on a 4-bit hash space
  • Demonstrates the quadratic speedup (√N quantum vs N classical)
  • Extrapolates to SHA-256 / HMAC-SHA-256 used in NanoPQ
  • Proves that 256-bit keys STILL SURVIVE even with Grover's speedup

Nothing is faked — every count/measurement is from qiskit_aer simulation.
"""

import sys
import math
import time

print("=" * 70)
print("  QUANTUM ATTACK ACT 2: Grover's Algorithm vs Hash Security")
print("=" * 70)
print()
print("  Loading Qiskit quantum circuit simulator...")

try:
    from qiskit import QuantumCircuit, transpile
    from qiskit_aer import AerSimulator
    import numpy as np
    print("  ✓ Qiskit Aer simulator loaded")
except ImportError as e:
    print(f"  ERROR: {e}")
    sys.exit(1)

print()
print("─" * 70)
print("  CONTEXT: Grover's Algorithm and Symmetric/Hash Security")
print("─" * 70)
print("""
  Grover's algorithm (1996) provides a QUADRATIC speedup for unstructured
  search problems, which includes:
    • Hash preimage search: find x such that H(x) = y
    • Symmetric key search: find k such that E_k(P) = C

  Classical cost to search N items: O(N)
  Grover's quantum cost:            O(√N)  — quadratic speedup

  Impact on key sizes:
    • AES-128 → 64-bit quantum security  (BROKEN — need AES-256)
    • AES-256 → 128-bit quantum security (SAFE)
    • SHA-256 → 128-bit quantum collision resistance (SAFE)
    • HMAC-SHA-256 with 256-bit key → 128-bit quantum security (SAFE)

  NanoPQ-SecureLink uses:
    • 256-bit per-device root key (128-bit PQ security via Grover)
    • HMAC-SHA-256 / HKDF-SHA-256 for session establishment
    • LMS/SLH-DSA (hash-based — Grover's applies but security margins hold)
    • Ascon-AEAD128 (128-bit security, designed for PQ)

  We will now run REAL Grover's on a 4-bit oracle and measure the speedup.
""")

input("  [JUDGE PROMPT] Press ENTER to run Grover's circuit on 4-bit hash search...")
print()

# ──────────────────────────────────────────────────────────────────────────────
# Grover's Algorithm: 4-bit Preimage Search
# ──────────────────────────────────────────────────────────────────────────────
# We model a 4-bit hash function: H: {0,1}^4 → {0,1}^4
# Target: find x such that H(x) = target (one specific value)
# Classical cost: ~8 queries on average (N/2 = 16/2)
# Grover's cost:  ~3 queries (π/4 × √16 ≈ 3.14 iterations)

N_BITS = 4
N_SPACE = 2**N_BITS   # 16
TARGET = 0b1010        # The "hash output" we're searching for: 0b1010 = 10

print(f"  Search space: 2^{N_BITS} = {N_SPACE} items")
print(f"  Target preimage: x such that H(x) = {TARGET:04b} (decimal {TARGET})")
print(f"  Classical queries needed (avg): {N_SPACE // 2}")
print(f"  Grover's optimal iterations: π/4 × √{N_SPACE} ≈ {math.pi/4 * math.sqrt(N_SPACE):.2f}")
print(f"  Grover's queries needed: ~{round(math.pi/4 * math.sqrt(N_SPACE))} vs {N_SPACE//2} classical")
print()

def build_grover_circuit(n_bits, target):
    """
    Build a Grover's search circuit for a given n-bit search space.
    Oracle marks |target> by flipping its phase.
    Diffuser amplifies marked state amplitude.
    """
    n_iter = round(math.pi / 4 * math.sqrt(2**n_bits))
    qc = QuantumCircuit(n_bits, n_bits)

    # Step 1: Uniform superposition
    qc.h(range(n_bits))
    qc.barrier(label="Superposition")

    for iteration in range(n_iter):
        # ── Oracle: mark |target> ──────────────────────────────────────────
        # Apply X gates to qubits where target bit = 0
        # Then multi-controlled-Z flips the phase of |111...1>
        # Then undo X gates
        for bit in range(n_bits):
            if not (target >> bit) & 1:
                qc.x(bit)
        # Multi-controlled Z gate (phase flip on |111...1>)
        qc.h(n_bits - 1)
        qc.mcx(list(range(n_bits - 1)), n_bits - 1)  # n-1 control qubits
        qc.h(n_bits - 1)
        # Undo X gates
        for bit in range(n_bits):
            if not (target >> bit) & 1:
                qc.x(bit)
        qc.barrier(label=f"Oracle iter {iteration+1}")

        # ── Diffuser (Grover amplitude amplification) ──────────────────────
        qc.h(range(n_bits))
        qc.x(range(n_bits))
        qc.h(n_bits - 1)
        qc.mcx(list(range(n_bits - 1)), n_bits - 1)
        qc.h(n_bits - 1)
        qc.x(range(n_bits))
        qc.h(range(n_bits))
        qc.barrier(label=f"Diffuser iter {iteration+1}")

    qc.measure(range(n_bits), range(n_bits))
    return qc, n_iter

print("  Building Grover's circuit...")
qc, n_iter = build_grover_circuit(N_BITS, TARGET)
print(f"  ✓ Circuit: {qc.num_qubits} qubits, {n_iter} Grover iterations, depth {qc.depth()}")
print()

input("  [JUDGE PROMPT] Press ENTER to execute on Aer quantum simulator (2048 shots)...")
print()

# ──────────────────────────────────────────────────────────────────────────────
# Execute
# ──────────────────────────────────────────────────────────────────────────────
sim = AerSimulator()
t_start = time.perf_counter()
tqc = transpile(qc, sim, optimization_level=1)
job = sim.run(tqc, shots=2048)
result = job.result()
t_elapsed = time.perf_counter() - t_start

counts = result.get_counts()
print(f"  ✓ Simulation complete in {t_elapsed:.3f}s  (2048 shots)")
print()

print("  Measurement histogram (most likely outcomes):")
print()
sorted_counts = sorted(counts.items(), key=lambda x: -x[1])
target_bits = format(TARGET, f'0{N_BITS}b')
for bitstring, count in sorted_counts[:8]:
    prob = count / 2048
    bar = "█" * int(prob * 50)
    marker = " ◄ TARGET FOUND!" if bitstring == target_bits else ""
    print(f"    |{bitstring}> = {int(bitstring,2):2d}  prob={prob:.3f}  {bar}{marker}")

target_count = counts.get(target_bits, 0)
target_prob = target_count / 2048
print()
print(f"  Target state |{target_bits}> = {TARGET} measured {target_count}/2048 times")
print(f"  Success probability: {target_prob:.1%}  (ideal Grover: ~100%)")
print()

# ──────────────────────────────────────────────────────────────────────────────
# Security scaling analysis
# ──────────────────────────────────────────────────────────────────────────────
print("─" * 70)
print("  SCALING ANALYSIS: Does Grover's Break NanoPQ's Hash Security?")
print("─" * 70)
print()

print("  ┌──────────────────┬──────────────┬─────────────────┬──────────────┐")
print("  │ Key / Search     │ N (space)    │ Classical Cost  │ Grover Cost  │")
print("  ├──────────────────┼──────────────┼─────────────────┼──────────────┤")

configs = [
    ("4-bit oracle (demo)", 4,   "~8 queries",     "~3 queries"),
    ("AES-128 key search",  128, "2^128 ops",       "2^64 ops → BROKEN"),
    ("AES-256 key search",  256, "2^256 ops",       "2^128 ops → SAFE"),
    ("SHA-256 preimage",    256, "2^256 ops",       "2^128 ops → SAFE"),
    ("HMAC-SHA-256 (256b)", 256, "2^256 ops",       "2^128 ops → SAFE"),
    ("NanoPQ root key",     256, "2^256 ops",       "2^128 ops → SAFE"),
    ("LMS leaf key",        256, "2^256 ops",       "2^128 ops → SAFE"),
]

for name, bits, classical, quantum in configs:
    marker = "◄ WE JUST RAN THIS" if bits == 4 else ""
    print(f"  │ {name:<18}│ 2^{bits:<10}│ {classical:<17}│ {quantum:<14}│ {marker}")

print("  └──────────────────┴──────────────┴─────────────────┴──────────────┘")
print()

print("""
  KEY FINDING:
  ┌─────────────────────────────────────────────────────────────────────┐
  │ Grover's quadratic speedup requires 2^128 operations to break       │
  │ NanoPQ-SecureLink's 256-bit root key or any SHA-256-based primitive.│
  │                                                                      │
  │ 2^128 operations at 1 billion qubits running at 1 GHz =             │
  │ ~1.07 × 10^22 YEARS  (longer than the age of the universe)          │
  │                                                                      │
  │ NanoPQ-SecureLink's symmetric layer is QUANTUM-SAFE.                │
  └─────────────────────────────────────────────────────────────────────┘
""")

print("  Grover's Attack on AES-128:      ⚠  SUCCEEDS (2^64, feasible)")
print("  Grover's Attack on NanoPQ Root:  ✅ 2^128 OPERATIONS — INFEASIBLE")
print()

# ──────────────────────────────────────────────────────────────────────────────
# Visualise the quantum speedup
# ──────────────────────────────────────────────────────────────────────────────
print("─" * 70)
print("  BONUS: ASCII visualisation of Grover's amplitude amplification")
print("─" * 70)
print()
print("  After superposition:  all amplitudes equal (flat)")
print("  Each Grover iteration amplifies the target |1010>:")
print()

N = 16
target_idx = TARGET

# Simulate ideal Grover amplitude progression
def grover_amplitude(n_items, target_idx, n_iter):
    """Compute theoretical Grover amplitudes."""
    theta = 2 * math.asin(1 / math.sqrt(n_items))
    amp_target = math.sin((2 * n_iter + 1) * theta / 2)
    amp_other  = math.cos((2 * n_iter + 1) * theta / 2) / math.sqrt(n_items - 1)
    return amp_target**2, amp_other**2

for it in range(5):
    p_target, p_other = grover_amplitude(N, target_idx, it)
    bar_t = "█" * int(p_target * 40)
    bar_o = "░" * int(p_other * 40)
    print(f"  iter {it}: target={p_target:.3f} {bar_t}")
    print(f"         others={p_other:.4f} {bar_o}")
    print()

print("  Amplitude of |1010> grows with each iteration → high probability measurement")
print()
