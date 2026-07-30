# Phase-one 50-mark scoring map

Evidence snapshot: 2026-07-30

This is the internal review checklist for the first Quant-A-Thon project
review. A polished explanation without the named artifact receives no internal
credit.

## Score map

| Criterion | Marks | Required review artifact | Demo sentence |
|---|---:|---|---|
| Methodology | 10 | Hyper-research verdict, primary-source matrix, claim ledger, threat model, controlled experiment design | “Every claim is tagged verified, derived, hypothesis, or pending, and every comparison keeps its original platform visible.” |
| Approach | 5 | One-page architecture showing exact ML-KEM RPE-32, authenticated compact fragments, bilateral confirmation, and Ascon records | “We preserve the standardized cryptography and change its execution and integration for a 2 KiB device.” |
| Feasibility | 5 | Named Nano ELF/map, stack high-water image/log, simulator trace, physical exchange, and `DOES NOT FIT` baseline | “Fit is a measured whole-program property, not a source-code estimate.” |
| Quantum technology utilization | 10 | Qiskit/Aer environment lock, executable circuits, backend configuration, seeds, raw counts, JSON evidence, circuit rendering | “The quantum attack demonstrations are actual Qiskit Aer runs with reproducible raw outputs.” |
| Quantum optimization | 10 | Optimization-level comparison, Grover iteration sweep, circuit depth/size/two-qubit gates, success probability, scaling explanation | “We optimize and measure the quantum circuits instead of treating Qiskit as an animation.” |
| Implementation | 10 | Importable Arduino library, live peer, deterministic ML-KEM oracle, physical sensor record, loss/tamper/replay/reboot tests | “A user imports the library and establishes a real post-quantum live session from the Nano.” |

## Quantum platform disclosure

Use this exact distinction in the review:

- Platform: **Qiskit 2.5.1 + Qiskit Aer 0.17.2**.
- Backend: `AerSimulator`.
- Current runs: local classical simulation of genuine quantum circuits.
- Current runs are **not** IBM Quantum hardware jobs.
- All shots, seeds, versions, transpiled metrics, and raw counts are retained.
- Hardware results may be added only with an actual IBM Quantum job ID and
  backend/calibration record.

## Quantum optimization gates

1. **Pass:** both circuits ran at Qiskit transpiler optimization levels 0–3.
2. **Pass:** JSON records qubits, depth, size, operation counts, two-qubit
   gates, compile time, simulation time, and result probability.
3. **Pass:** Grover iterations 0–5 ran with 4,096 shots per point.
4. **Pass:** the measured optimum was three iterations, with 3,937/4,096
   target hits (96.12%).
5. **Pass:** every raw count distribution is retained.
6. **Required in presentation:** toy Shor/Grover circuits demonstrate attack
   mechanics and asymptotic advantage; they neither break nor prove
   ML-KEM-512.

## Stop-claim list

Do not tell reviewers that:

- a simulator broke RSA-2048, ECC, ML-KEM, or Ascon;
- failure to break ML-KEM proves its security;
- the current C0 package already fits the Nano;
- a Cortex-M4 or ATmega1284P result is a Nano result;
- an experimental KEM is NIST-approved;
- AES or Ascon repairs a broken key exchange;
- the live channel is completely stateless.

## Full-mark evidence order

1. Show the exact target board and whole-program SRAM budget.
2. Show why existing implementations fail with real linked numbers.
3. Show the standards-preserving RPE-32 execution hypothesis.
4. Show deterministic equality with an independent ML-KEM implementation.
5. Show the simulator-linked Nano handshake and encrypted sensor record.
6. Show the same exchange on physical hardware.
7. Show real benchmark and energy traces.
8. Run the Qiskit attack demonstrations and expose raw results.
9. Show circuit optimization measurements.
10. Finish with limitations, experimental status, and next cryptanalysis gate.
