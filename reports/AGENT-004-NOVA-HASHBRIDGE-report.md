# EEPROM-Integrated Key Caching & Hybrid Memory Architecture Report

## 1. Agent Information
- **Agent Name:** NOVA HASHBRIDGE
- **Agent ID:** AGENT-004-NHB
- **Specialization:** EEPROM-Integrated Key Caching & Hybrid Memory Architecture

## 2. Core Hypothesis
By utilizing the ATmega328P's 1024-byte EEPROM for caching relatively static or periodically regenerated cryptographic keys and seeds, we can significantly relieve pressure on the severely constrained 2KB SRAM. Since cryptographic operations in hash-based signatures (like SPHINCS+ or XMSS) require substantial state and temporary key storage, offloading these to non-volatile memory allows the SRAM to be dedicated exclusively to active computation (e.g., hash tree node evaluation, stack frames, and active context).

## 3. EEPROM Memory Layout Proposal
The ATmega328P features 1024 bytes of EEPROM. The proposed hybrid architecture partitions this space as follows:
- **0x000 - 0x01F (32 bytes):** Wear-leveling metadata (write counters, active slot pointers).
- **0x020 - 0x03F (32 bytes):** Root Secret Seed (`SK.seed`) - Static long-term storage.
- **0x040 - 0x05F (32 bytes):** Public Seed (`PK.seed`) - Static long-term storage.
- **0x060 - 0x15F (256 bytes):** WOTS+ / FORS Private Key caching area (dynamic, rotating slots).
- **0x160 - 0x25F (256 bytes):** Hash Tree Node Cache (intermediate roots for frequently accessed subtrees).
- **0x260 - 0x3FF (416 bytes):** Reserved for future expansions or additional key material caching.

## 4. Wear-Leveling Strategy
The ATmega328P EEPROM is rated for approximately 100,000 write/erase cycles. To prevent premature wear from dynamic key caching:
1. **Read-Before-Write:** Always read the target byte before writing. If the value is already correct, bypass the write operation entirely.
2. **Circular Buffering:** The dynamic caching area (0x060-0x15F) will be divided into multiple slots. A rotating pointer (stored in the wear-leveling metadata section) dictates the active slot. Writes are distributed evenly across these slots.
3. **Differential Updates:** When updating cached states, only modify the specific bytes that have changed, minimizing erase/write cycles.

## 5. Key Regeneration from Seed Stored in EEPROM
Storing all generated keys is impossible due to memory constraints. Instead, the core `SK.seed` and `PK.seed` reside permanently in EEPROM. 
When a specific key (e.g., a WOTS+ private key chain) or a subtree node is required, it is dynamically regenerated using a Pseudo-Random Function (PRF) seeded directly from the EEPROM. If the regenerated key is needed multiple times within a short execution window, it is temporarily stored in the EEPROM dynamic cache area to preserve SRAM, and systematically evicted or overwritten when no longer required.

## 6. SRAM Savings Analysis
- Storing a standard WOTS+ key chain or large signature state directly in SRAM can easily consume 256-512 bytes.
- By relocating the static seeds (64 bytes) and temporary key caches (256 bytes) to EEPROM, we successfully free up to 320 bytes of SRAM.
- This represents ~15.6% of the total 2048 bytes of SRAM on the ATmega328P. This newly available space is highly critical, as it can prevent stack overflows during deeply nested hash function calls (e.g., SHA-256 or SHAKE256) and allow successful algorithm execution.

## 7. Comparison with SRAM-Only Approaches
- **SRAM-Only Architecture:** All keys, seeds, and intermediate states must reside in the limited 2KB SRAM. This creates extreme memory pressure, often leading to stack overflows or forcing aggressive on-the-fly recalculation of keys, which exponentially increases CPU cycles and execution time.
- **Hybrid Memory Architecture:** Leverages EEPROM as a secondary cache. While EEPROM has slower access times (reads take ~4 clock cycles; writes take ~3.3ms) compared to SRAM (1-2 clock cycles), the massive gain in available SRAM allows for more efficient algorithm implementations and deeper stack frames. The CPU penalty of reading from EEPROM is heavily offset by reducing the need to constantly recalculate keys from scratch.

## 8. Concrete Next Steps
1. **Develop an EEPROM HAL:** Create a Hardware Abstraction Layer tailored for block-based caching and integrated directly with the cryptographic core.
2. **Implement Wear-Leveling:** Write and verify the read-before-write and circular buffer algorithms.
3. **Latency Profiling:** Profile the read/write latency impact on a standard signature generation cycle to ensure timing constraints are met.
4. **SRAM Measurement:** Integrate the EEPROM cache into the existing WOTS+ implementation and measure exact SRAM savings via stack watermarking.
5. **Cycle Testing:** Stress-test the EEPROM up to 100,000 cycles on a test board to empirically validate the effectiveness of the wear-leveling strategy.
