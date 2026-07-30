# AGENT-010-SPM (SIGMA PROGMEMIUS) Report

## 1. Identity
* **Agent Name:** SIGMA PROGMEMIUS
* **Agent ID:** AGENT-010-SPM
* **Specialization:** Flash Memory Architecture & PROGMEM-Resident Computation

## 2. Core Hypothesis
To overcome the severe memory limitations on Arduino Uno architectures (2KB SRAM), the core hypothesis proposes moving the A matrix out of volatile memory (SRAM) and into non-volatile program memory (PROGMEM, which has 32KB of flash storage). This shift is critical for executing memory-intensive cryptographic operations.

## 3. FLASHKEM Proposal & Pre-Computation Strategy
The FLASHKEM proposal focuses on pre-computing static elements of the cryptographic protocols and embedding them into the flash memory during compilation.
By pre-computing the A matrix and compiling it directly into the binary as PROGMEM data, we entirely avoid allocating dynamic memory for it during runtime. The strategy leverages the relatively large 32KB flash memory of standard microcontrollers to store immutable constants and matrices, fundamentally altering the memory usage profile of the Key Encapsulation Mechanism (KEM).

## 4. SRAM Savings
By storing the A matrix in flash memory, we successfully free **512 bytes** of SRAM. This is a massive savings, representing 25% of the total 2KB SRAM available on an ATmega328P. This recovered memory can be redirected towards dynamic stack usage, polynomial operations, and other volatile data processing requirements.

## 5. Build Pipeline for Per-Device Firmware
Implementing this strategy requires a specialized build pipeline:
* **Unique Generation:** The A matrix (and other device-specific cryptographic constants) must be generated for each individual device.
* **Header Generation:** A script (e.g., Python or Bash) will generate a unique C header file containing the PROGMEM arrays (e.g., `const uint8_t A_MATRIX[] PROGMEM = { ... };`).
* **Compilation:** The firmware is compiled individually for each device, embedding its specific pre-computed data.
* **Flashing:** The uniquely compiled binary is then flashed to the target device.
This creates a true per-device firmware deployment model.

## 6. Access Cost Analysis: `pgm_read_word_near()`
While moving data to PROGMEM saves SRAM, it introduces an access cost penalty:
* **Instruction Cost:** Reading from PROGMEM requires the use of specific macros like `pgm_read_byte_near()` or `pgm_read_word_near()`. These translate to the `LPM` (Load Program Memory) instruction in AVR assembly.
* **Cycle Penalty:** The `LPM` instruction typically takes 3 clock cycles, whereas a standard `LD` (Load from SRAM) takes 1-2 clock cycles. 
* **Trade-off Evaluation:** This small increase in computation time (due to extra cycles for memory fetches) is an overwhelmingly positive trade-off given that the alternative (SRAM exhaustion) results in catastrophic failure (stack collision/heap exhaustion).

## 7. Flash Layout Map
* `0x0000 - 0x01FF`: Bootloader (Optional / typical Uno bootloader space if top-aligned, though usually at end of flash. For application space, interrupt vectors sit at `0x0000`).
* `0x0000 - 0x00FF`: Interrupt Vector Table
* `0x0100 - 0x1FFF`: Core Firmware Instructions (.text section)
* `0x2000 - 0x6FFF`: FLASHKEM Pre-computed Data (PROGMEM)
  * `A Matrix`: 512 bytes
  * `Static Tables / Twiddle Factors`: Variable
* `0x7000 - 0x7FFF`: Bootloader (Optiboot typically resides here on Uno)

## 8. Key Rotation Strategy
Because the A matrix and pre-computed constants are baked into the flash memory, traditional dynamic key rotation is impossible without a firmware update.
* **Strategy:** Over-The-Air (OTA) or wired firmware updates become the mechanism for key rotation. 
* When keys need to be rotated, the centralized build pipeline generates a new unique binary with the new A matrix and flashes the device, entirely replacing the old cryptographic state.

## 9. Concrete Next Steps
1. **Prototype the Build Script:** Develop a Python script to generate a dummy A matrix as a C header file utilizing PROGMEM directives.
2. **Refactor Matrix Multiplication:** Rewrite the matrix multiplication routines to use `pgm_read_word_near()` when accessing the A matrix operands.
3. **Benchmarking:** Profile the execution time to measure the exact cycle penalty introduced by the `LPM` instructions against the baseline.
4. **Integration:** Merge the PROGMEM-based matrix multiplication back into the main KEM protocol flow and test on physical hardware (Arduino Uno) to confirm the 512 bytes of SRAM savings and ensure stability without stack overflows.
