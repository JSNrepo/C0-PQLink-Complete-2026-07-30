# Agent Report: ELENA WIREDGRAPH (AGENT-008-EWG)

## 1. Identity & Specialization
**Agent Name:** ELENA WIREDGRAPH  
**Agent ID:** AGENT-008-EWG  
**Specialization:** Inverse Protocol Design & Decapsulator-Optimized Architecture  

## 2. Core Hypothesis: Nano as Decapsulator
In traditional post-quantum Key Encapsulation Mechanisms (KEMs), embedded devices are often burdened with both key generation and encapsulation duties. Our core hypothesis flips this model: the Arduino Nano (or similarly constrained device) should operate strictly as a *decapsulator*. By exclusively receiving the KEM and offloading encapsulation and complex key pair generation to more capable gateways or peers, we dramatically reduce the processing, memory footprint, and entropy requirements on the Nano.

## 3. INVERSE-KEM Protocol Proposal
The INVERSE-KEM protocol standardizes an asymmetric capability model:
- **Phase 1: Provisioning:** A long-term static public/private key pair is generated externally. The private key is flashed/stored securely on the Nano's EEPROM. The public key is published to the network.
- **Phase 2: Handshake Initiation:** A computationally powerful peer (e.g., a server or gateway) generates an ephemeral symmetric key, encapsulates it against the Nano's public key, and transmits the resulting ciphertext.
- **Phase 3: Fragmented Reception:** The Nano receives the ciphertext in chunks.
- **Phase 4: Decapsulation:** The Nano reassembles/streams the ciphertext into the decapsulation engine using its stored private key, recovering the shared symmetric key.
- **Phase 5: Secure Channel:** Subsequent communication uses the derived symmetric key via a lightweight authenticated encryption scheme (like AES-GCM or ChaCha20-Poly1305).

## 4. Fragment Processing & SRAM Reduction
A major bottleneck in PQC on constrained devices is storing a full KEM ciphertext (often >1KB) in SRAM. By implementing fragment processing, the Nano never stores the entire ciphertext in SRAM simultaneously. Instead, as network fragments arrive:
- They are processed on-the-fly via a streaming decapsulation algorithm (e.g., streaming Keccak/SHA3 absorption).
- Intermediate state hashes are updated incrementally.
- Memory is immediately freed for the next fragment.
This eliminates the need for large contiguous SRAM buffers, effectively turning a space-complexity problem into a time-complexity (latency) trade-off, which is highly favorable for the Nano.

## 5. EEPROM Layout for Long-Term Secret Key
To ensure the long-term secret key is secure and efficiently accessed without crowding SRAM, it is stored in the non-volatile EEPROM (or flash). A typical 1KB EEPROM layout:
- `0x000 - 0x003`: Magic Header / Version ID (4 bytes)
- `0x004 - 0x00F`: Cryptographic Metadata / Key ID / CRC32 (12 bytes)
- `0x010 - 0x3FF`: Secret Key Data (e.g., Kyber512 secret key, approximately 1632 bytes in standard form, but can be compressed or stored in flash if EEPROM is strictly 1KB. For a standard ATmega328P with 1KB EEPROM, smaller keys or compressed seed representations are required, expanding into SRAM only during the decapsulation window).
*Note: If the key exceeds the EEPROM size, it must be stored in PROGMEM (Flash) with appropriate access safeguards.*

## 6. SRAM Budget Breakdown (Decapsulation-Only)
By stripping out encapsulation and key generation, the SRAM budget (out of the 2048 bytes available on ATmega328P) becomes highly optimized:
- **Network/Serial Buffer:** 128 bytes
- **Streaming Hash State (Keccak):** 200 bytes
- **Decapsulation Working Variables (Polynomial arithmetic):** ~800 bytes
- **Derived Symmetric Key Buffer:** 32 bytes
- **System Stack & Heap Overhead:** ~500 bytes
- **Remaining Margin:** ~388 bytes for application logic.

## 7. Comparison with Encapsulation-on-Nano Approaches
| Metric | Encapsulation on Nano | Decapsulation on Nano (Inverse) |
|---|---|---|
| **SRAM Usage** | > 1500 bytes (requires PRNG state + public key buffer) | < 1200 bytes (streaming processing) |
| **Entropy Requirement** | High (must generate secure ephemeral keys) | Zero (only uses static stored key) |
| **Execution Time** | Moderate to High | Low to Moderate (no random sampling) |
| **Code Size (Flash)** | High (includes RNG and Encap logic) | Low (Decap logic only) |

## 8. Concrete Next Steps
1. **Develop Streaming Keccak Wrapper:** Implement an iterative Keccak state absorber that works with Serial/SPI interrupt buffers.
2. **Flash Memory Key Compression:** Evaluate storing the private key seed in EEPROM/PROGMEM and expanding it on-the-fly to fit the constrained layout.
3. **Draft PoC Decapsulator:** Write a proof-of-concept C implementation for Kyber or a similar KEM that strictly performs decapsulation on ATmega328P.
4. **Benchmark:** Measure exact cycle counts and SRAM high-water marks for the PoC.
