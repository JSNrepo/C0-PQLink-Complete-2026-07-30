# Quant-A-Thon judge gates

The panel names came from the team. Public sources were used only to
cross-check professional roles. The questions and evaluation priorities below
are **our inferences**, not statements made by the judges or organizers.

## Gate 1 — C. V. Sridhar

Public role: Mission Director, Andhra Pradesh State Quantum Mission /
Amaravati Quantum Valley. A Government of India PIB release identifies him
in that role and emphasizes quantum communication, cybersecurity, systems
engineering, skilling, research, and industry collaboration.

Likely mission-scale pressure:

- Can this migrate existing fleets instead of requiring hardware replacement?
- Is it standards-aligned and supportable across vendors?
- How are provisioning, key rotation, downgrade prevention, and failure
  recovery handled?
- Which claims are measured, and what remains before field rollout?

C0-PQLink evidence to show:

- importable C++ and native MicroPython surfaces over one C core;
- `LEGACY_PSK -> PSK_PLUS_PQ -> PQ_REQUIRED_WITH_PSK_AUTH`;
- authenticated A/B journal, key ID, epoch, and two key slots;
- cohort rollout and explicit stop conditions;
- exact ML-KEM-512 rather than a private national-scale dependency on a new
  primitive.

Do not claim:

- that immutable devices need no firmware change;
- production or national-infrastructure readiness;
- physical anti-rollback without platform support.

Public source:
[PIB — Amaravati Quantum & AI initiative](https://www.pib.gov.in/PressReleasePage.aspx?PRID=2231125).

## Gate 2 — Dhinakaran Vinayagamurthy

Public role: Manager, IBM Quantum India; his IBM profile also identifies
research interests in cryptography and security.

Likely cryptographic pressure:

- Is this truly FIPS 203 ML-KEM-512, or renamed/modified lattice math?
- Is interoperability independent, deterministic, and reproducible?
- What exactly is standardized, and what is a new protocol?
- Are the threat model, transcript, domain separation, Finished, nonces, and
  failure behavior precise?
- Is “constant time” supported by target evidence?

C0-PQLink evidence to show:

- eight byte-exact ciphertext/shared-secret oracle comparisons;
- canonical public-key rejection;
- documented labels, transcript, wire bytes, and bilateral Finished;
- SP 800-232 Ascon known-answer and tamper tests;
- source-level fixed-schedule AVR multiply design;
- claim ledger that leaves AVR disassembly, leakage testing, and audit
  explicitly pending.

Do not claim:

- a NIST-approved C0-PQLink protocol;
- a formal proof, FIPS validation, CAVP validation, or complete constant-time
  result.

Public source:
[IBM Research profile](https://research.ibm.com/people/dhinakaran-vinayagamurthy).

## Gate 3 — L. Venkata Subramaniam

Public role: QBit Force describes Dr. L. Venkata Subramaniam as leading
commercialization strategy, ecosystem partnerships, and corporate vision.

Likely product/ecosystem pressure:

- Can another developer import this without understanding lattice internals?
- Can it plug into different boards, radios, and gateways?
- Is there a runnable demonstration and a path from prototype to an ecosystem?
- Is the differentiation more than a paper benchmark?

C0-PQLink evidence to show:

- `#include <C0PQLink.h>` and `from c0pqlink import Client`;
- explicit public-key, RNG, and packet-transport adapters;
- fail-closed example rather than insecure demo entropy;
- generated paired demo provisioning;
- independently implemented replaceable peer and protocol document;
- portable CMake package and Apache-2.0 licensing.

Do not claim:

- Arduino Library Manager or PyPI publication;
- support for every board merely because `architectures=*`;
- that the reference peer is required infrastructure.

Public source:
[QBit Force company profile](https://www.qbitforcequantum.com/company).

## Gate 4 — Ravindra Barlingay

Public role: IITM CDoT Samgnya Technologies Foundation lists Ravindra
Barlingay as Chief Executive Officer. The foundation describes itself as a
National Quantum Mission initiative.

Likely communications/operations pressure:

- Does this protect actual live traffic rather than only a file or boot path?
- What happens on packet loss, duplication, tampering, and reconnect?
- Does the protocol fit constrained links and interoperate across endpoints?
- Where do keys live, and how can the peer be replaced?

C0-PQLink evidence to show:

- 60/84/77/28/26-byte handshake frames and at most 82-byte records;
- stop-and-wait retransmission of only one 48-byte fragment;
- injected loss of Challenge, fragment-7 ACK, Finished, and a protected data
  response with exact-frame retry;
- forged fragment, Finished, and record rejection;
- C client ↔ JavaScript peer decapsulation, Finished, sensor record, and
  protected response;
- reference peer explicitly replaceable by a gateway, telecom endpoint, or
  industrial controller.

Do not claim:

- complete LoRaWAN/NB-IoT compliance from frame size alone;
- measured airtime, duty-cycle, latency, or energy before field tests.

Public source:
[IITM CDoT Samgnya team](https://www.samgnya.in/team).

## Cross-panel pass/fail board

| Gate | Present evidence | Release blocker |
|---|---|---|
| Exact standardized primitive | Independent ML-KEM byte oracle and Ascon vector | Third-party crypto review |
| Real live connection | Cross-language handshake and protected sensor/ACK | Named-hardware run |
| Constrained design | Flash key callback and streamed ciphertext are verified; current 1,344-byte KEM workspace leads to a failed 2,016-byte static Nano link | RPE-32 prototype, target stack/SRAM high-water, and physical Nano exchange |
| Loss/active attack behavior | Automated injected loss, tamper, replay tests | Parser fuzzing and broader state-machine analysis |
| Migration | One-way states and torn-write journal | Fleet provisioning/revocation integration |
| Developer adoption | Arduino, CMake, native MicroPython, adapter example | Board/port-specific packages |
| Honest scope | Claim ledger and threat model | None—maintain this discipline |

## One-sentence answer to “Why a Node peer?”

“It is a replaceable independent test endpoint performing the standard
ML-KEM private-key decapsulation role; every constrained-device operation
remains on the device, and the documented protocol allows Node to be replaced
by the real gateway or controller.”
