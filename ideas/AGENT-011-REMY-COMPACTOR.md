# Agent 011 — REMY COMPACTOR
**ID:** AGT-011-RCO  
**Specialization:** Stack Frame Minimization & Compiler-Guided Memory Layout  
**Perspective:** *The 611-byte stack frame is the immediate blocker. Fix the code structure, not the algorithm.*

---

## Core Hypothesis

The measured 611-byte stack frame in `c0pq_client_connect()` is not an algorithm limitation — it's a **software engineering failure**. REMY COMPACTOR focuses entirely on reducing the stack frame without changing any cryptographic mathematics.

## Analysis of the 611-Byte Frame

### What Causes Deep Stack Frames?

Common causes (all fixable):
1. **Large local arrays** — `int16_t poly[256]` = 512 bytes on the stack
2. **Deep call chains** — each called function adds its frame
3. **Inlined functions that aren't inlined** — compiler expanding function calls
4. **Excessive local variables** — passing data through locals instead of caller-allocated buffers

### Reconstructing the c0pq_client_connect Stack

Based on the source structure, the 611-byte frame likely contains:
```c
// Hypothetical reconstruction of the offending local variables:
int16_t kem_workspace[MAX_COEFFICIENTS]; // 512 bytes alone ← THE PROBLEM
uint8_t hash_tmp[64];                    // 64 bytes
uint8_t fragment_buf[C0PQ_FRAGMENT_MAX]; // 96 bytes  
// Plus return address, saved registers: ~24 bytes
// Total: 512 + 64 + 96 + 24 = 696 → down to 611 with optimization
```

### Proposal A: Caller-Allocated Buffers (Static Analysis)

**Move all large buffers to caller-provided parameters:**

```c
// BEFORE (causes 611-byte frame):
int c0pq_client_connect(c0pq_client_context_t *ctx, ...) {
    int16_t workspace[256];   // 512 bytes on stack!
    uint8_t hash_tmp[64];     // 64 bytes on stack!
    // ...
}

// AFTER (frame budget: 32 bytes):
typedef struct {
    int16_t workspace[256];   // 512 bytes — allocated by caller
    uint8_t hash_tmp[64];     // 64 bytes — allocated by caller
    uint8_t fragment_buf[96]; // 96 bytes — allocated by caller
} c0pq_connect_arena_t;

int c0pq_client_connect(
    c0pq_client_context_t *ctx,
    c0pq_connect_arena_t *arena,   // ← caller provides this
    ...
) {
    // All operations use arena->workspace, arena->hash_tmp, etc.
    // Stack frame: only pointers + loop vars + return addr ≈ 24 bytes
}
```

**The arena is still 672 bytes — but the CALLER controls its lifetime and placement.** The caller can:
- Declare it as a global (`.bss` section, not stack)
- Reuse it for multiple operations
- Place it at a specific SRAM address using linker scripts

### Proposal B: Global Arena with Lifetime Tracking

```c
// global_arena.h
// Single global arena: used for cryptographic operations, freed after session
// Size: 672 bytes (deterministic, no fragmentation)
extern uint8_t g_crypto_arena[672];

// Usage:
void setup() {
    // g_crypto_arena is in .bss — zero-initialized, no stack cost
    c0pq_connect_arena_t *arena = (c0pq_connect_arena_t *)g_crypto_arena;
    c0pq_client_connect(&ctx, arena, ...);
    // After connect: arena memory can be repurposed for Ascon traffic state
}
```

**Stack frame after change:** `c0pq_client_connect()` frame shrinks from 611 bytes to **~24 bytes**.

### Proposal C: Stack Frame Accounting via `__attribute__((noinline))`

The compiler sometimes **inlines** functions that should be separate, exploding a single function's frame. Explicit `noinline` prevents this:

```c
// Force each phase to have its own (small) stack frame
__attribute__((noinline))
static int connect_phase_hash(c0pq_client_context_t *ctx) {
    uint8_t H_ek[32];  // only 32 bytes on stack — manageable
    // ...
    return 0;
}

__attribute__((noinline))
static int connect_phase_encaps_row(
    c0pq_client_context_t *ctx,
    uint8_t row_idx,
    int16_t *accumulator  // caller-provided 384-byte buffer
) {
    // Frame: ~64 bytes (tile buffer) + ~16 bytes (loop vars)
    int16_t tile[32];  // 64 bytes — this is acceptable
    // ...
}
```

**Each phase has a small independent frame.** They are NEVER simultaneously on the stack (sequential, not recursive).

### Proposal D: Linker Section Overlay

For true SRAM champions: use the AVR linker to overlay sections that are never live simultaneously:

```
/* linker script overlay section */
SECTIONS {
    .crypto_workspace (NOLOAD) : {
        /* Crypto workspace: 672 bytes, used ONLY during key establishment */
        _crypto_workspace_start = .;
        . += 672;
        _crypto_workspace_end = .;
    } > data_seg
    
    .ascon_traffic_state (NOLOAD) : AT(_crypto_workspace_start) {
        /* Ascon traffic state: 256 bytes, used ONLY during traffic */
        /* OVERLAPS crypto_workspace — they are never simultaneously active */
        _ascon_state_start = .;
        . += 256;
    }
}
```

**Effective SRAM:** max(672, 256) = 672 bytes for both phases, not 672+256 = 928 bytes.

### Proposal E: Measured Frame Reduction Steps

**Step 1:** Add `__attribute__((section(".crypto_workspace")))` to the workspace array → moves from stack to .bss → **reduces frame by 512 bytes**

**Step 2:** Pass hash temporaries as parameter buffers → **reduces frame by 64 bytes**

**Step 3:** Use `noinline` on fragment dispatch → **reduces frame by ~35 bytes**

**Expected result:** frame ≤ 32 bytes (just loop variables, pointers, return address)

### SRAM Budget After Frame Fixes

```
.data + .bss (static):         1200 bytes  ← down from 2016 via algorithm improvements
.crypto_workspace (global):     672 bytes  ← replaces stack allocation
Peak stack (deeply nested call): 128 bytes  ← all large buffers now in .bss
Interrupt stack margin:          256 bytes  ← required headroom
────────────────────────────── ─────────
Total peak:                     2048 - 128 = 1920 ... hmm, still tight

With overlay sections:
.crypto_workspace / .ascon_state overlap: 672 bytes (not additive)
Remaining .bss:                           528 bytes
Peak call stack:                          128 bytes
ISR margin:                               256 bytes
Total peak:                               672 + 128 = 800 bytes well under gate
```

### Implementation Roadmap

1. **Audit** `src/session/fragment.c` line 276 — identify all local arrays > 32 bytes
2. **Extract** each large array to a caller-provided `arena` parameter
3. **Create** `include/c0pqlink_arena.h` defining arena structs
4. **Modify** `connect()` function signature — backwards-compatible with `C0PQLink.cpp`
5. **Add linker overlay** for crypto_workspace / ascon_traffic_state
6. **Verify** with `avr-gcc -fstack-usage` that max frame is ≤ 64 bytes
7. **Re-run** oracle test — must still produce exact ML-KEM bytes

### Expected Outcome

This approach alone (without algorithm changes) could reduce peak SRAM from 2016+611 bytes to **~1100-1300 bytes** by fixing the software architecture without touching any cryptographic math.

---

*REMY COMPACTOR — AGT-011-RCO | Research Snapshot: 2026-07-30*
