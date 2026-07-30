# Agent Report: AGENT-011-REMY-COMPACTOR

## 1. Agent Identity
- **Agent Name:** REMY COMPACTOR
- **ID:** AGENT-011-RCO
- **Specialization:** Stack Frame Minimization & Compiler-Guided Memory Layout

## 2. Core Hypothesis
The 611-byte stack frame observed during operations is a software engineering failure, not a mathematical necessity. It represents an inefficient translation of mathematical algorithms into machine-executable instructions, characterized by excessive local variable allocations, deep call chains without tail-call optimization, and overlapping lifetimes of large temporaries.

## 3. Analysis of Current Deep Stack Frames
In the current codebase, the primary causes for deep stack frames are:
- **Large Local Temporaries:** Allocating arrays and complex state variables directly on the stack inside functions.
- **Deep Call Graphs:** Recursive or deeply nested function calls that multiply the cost of smaller stack frames.
- **Compiler Inlining Deficiencies:** Inlining functions that contain large local variables can inflate the caller's stack frame beyond necessary bounds.
- **Missing Lifetime Scoping:** Temporaries whose actual usage lifetimes do not overlap are still allocated simultaneously in the stack frame due to a lack of tight block scoping.

## 4. Caller-Allocated Arena Proposal
Instead of having callee functions blindly allocate temporaries on the stack, the caller should pass a pointer to a pre-allocated memory workspace (an "arena"). 
- **Mechanism:** Pass a `uint8_t* workspace` or similar structure down the call chain.
- **Benefit:** Gives the caller complete control over memory bounds and allows reuse of the same memory block for sequential function calls, bypassing stack accumulation.

## 5. Global Arena in .bss Strategy
For strictly single-threaded environments (common in embedded systems or constrained targets), a statically allocated global arena in the `.bss` section can be used.
- **Mechanism:** Define a large global array (e.g., `static uint8_t global_crypto_arena[MAX_SIZE];`) initialized to zero.
- **Benefit:** Moves the memory burden entirely off the stack. It eliminates runtime stack overflow risks associated with deep call chains and provides deterministic memory sizing at compile time.

## 6. `__attribute__((noinline))` Strategy
Paradoxically, preventing function inlining can sometimes reduce peak stack usage.
- **Mechanism:** Apply `__attribute__((noinline))` to functions containing large local buffers.
- **Benefit:** By forcing the compiler to create a separate stack frame for the function, the large local buffers are only allocated when that specific function is called, and are popped off immediately after. If inlined, the caller's stack frame permanently balloons to accommodate the callee's temporaries, even during execution paths where the callee's logic is not active.

## 7. Linker Overlay Section Design
By manipulating the linker script, we can create overlay sections where mutually exclusive components share the same physical SRAM addresses.
- **Mechanism:** Define an `OVERLAY` in the linker script for distinct modules (e.g., key generation vs. signing).
- **Benefit:** Drastically reduces total SRAM footprint without runtime overhead. Since key generation and signing never run concurrently, their memory spaces can safely overlap, maximizing memory utilization.

## 8. Expected SRAM Savings
Without changing the underlying mathematical algorithms, implementing these memory layout and stack management strategies is projected to yield:
- **Stack Reduction:** Elimination of the 611-byte stack frame issue, reducing peak stack usage by up to 70-80%.
- **SRAM Savings:** Hundreds of bytes recovered through global arenas and linker overlays.
- **Stability:** Complete prevention of stack overflow hardware faults.

## 9. Concrete Next Steps
1. **Audit Stack Usage:** Run `-fstack-usage` and `su` file analysis on the current build to pinpoint exact functions responsible for the 611-byte frame.
2. **Refactor Callee Functions:** Modify the top 3 worst-offending functions to accept a caller-allocated arena pointer.
3. **Apply Attributes:** Introduce block scoping and `__attribute__((noinline))` on large-buffer functions that are currently being aggressively inlined.
4. **Implement Global .bss Arena:** Draft a prototype of the global workspace for the primary entry points and validate thread safety requirements.
5. **Linker Script Update:** Prototype an `OVERLAY` section in the linker script for mutually exclusive operational phases.
