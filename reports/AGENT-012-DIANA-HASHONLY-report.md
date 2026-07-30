# Agent Report: AGENT-012-DHO

## 1. Agent Name, ID, Specialization
- **Name**: DIANA HASHONLY
- **ID**: AGENT-012-DHO
- **Specialization**: Hash-Based Post-Quantum Signatures & Hybrid KEM-PSK

## 2. Core Hypothesis
Pre-Shared Key (PSK) + Hash-Based Authentication is fundamentally simpler, more efficient, and often more robust for constrained IoT devices than attempting to implement full lattice-based Key Encapsulation Mechanisms (KEMs) like ML-KEM.

## 3. PSK-HBSS Protocol Design
The PSK-HBSS (Pre-Shared Key - Hash-Based Signature Scheme) protocol leverages a symmetric PSK for confidentiality and a hash-based signature scheme (like XMSS or LMS) for authentication.
1. **Commissioning phase**: Devices are provisioned with a 256-bit symmetric PSK out-of-band and a public key for the hash-based signature scheme.
2. **Key Derivation**: Ephemeral session keys are derived from the PSK and a non-repeating nonce/counter using a KDF (e.g., HKDF-SHA256).
3. **Message Authentication**: Each message or session negotiation is authenticated using the hash-based signature, proving the sender's identity and preventing spoofing, while the PSK provides forward secrecy and confidentiality against quantum adversaries (as AES-256 is quantum-resistant).

## 4. Why AES-256 + SHA256 already resists Grover's Algorithm
Grover's algorithm provides a quadratic speedup for unstructured search problems. 
- For symmetric encryption like AES-256, Grover's algorithm reduces the effective key strength by half, bringing it to 128 bits of post-quantum security. This is generally considered secure against any foreseeable quantum computer.
- For hash functions like SHA-256, collision resistance is affected by the Brassard-Høyer-Tapp (BHT) algorithm, which implies a generic collision search bound of $O(2^{n/3})$, though finding preimages via Grover's remains $O(2^{n/2})$. SHA-256 provides 128 bits of post-quantum preimage resistance. 
Therefore, symmetric constructions relying on AES-256 and SHA-256 are intrinsically post-quantum secure without the need for complex algebraic structures.

## 5. SRAM Budget Breakdown (~780 bytes)
A critical advantage of PSK + Hash-based auth is the low SRAM requirement, fitting comfortably within the tight constraints of devices like the Arduino Nano:
- **AES Context (AES-256-GCM)**: ~150 bytes
- **SHA-256 State**: ~104 bytes
- **XMSS/LMS Public Key Buffer**: ~64 bytes
- **Message/Ciphertext Buffer**: ~256 bytes
- **Nonce/Counters**: ~32 bytes
- **State Machine / Control Flow**: ~174 bytes
- **Total SRAM required**: ~780 bytes

## 6. XMSS Verification Feasibility on Nano
Hash-based signature schemes like eXtended Merkle Signature Scheme (XMSS) are stateful, making key generation and signing expensive and state-dependent. However, **verification** is stateless and highly efficient. 
On an Arduino Nano (ATmega328P, 2KB SRAM), verifying an XMSS signature involves hashing the message, executing WOTS+ (Winternitz One-Time Signature) verification, and computing a Merkle tree path. 
This process requires minimal RAM, consisting mainly of a few 32-byte buffers for intermediate hash values. With optimized SHA-256 implementations, XMSS verification is completely feasible within the Nano's computational and memory constraints.

## 7. Honest Assessment of Limitations
While promising, the PSK + Hash-based approach has notable limitations:
- **Commissioning Complexity**: Every device pair needs a secure, out-of-band mechanism to establish the PSK. Scaling this to millions of devices is a logistical nightmare compared to PKI.
- **State Management**: Hash-based signatures like XMSS require the signer to maintain strict state (the index of the one-time key used). If this state is lost, duplicated, or corrupted, the security of the entire scheme is compromised.
- **PSK Leakage**: If the static PSK is compromised from the device's non-volatile memory (e.g., via physical extraction), all past (if no perfect forward secrecy via ephemeral keys is strictly enforced) and future communications can be decrypted by an attacker. 

## 8. Comparison with ML-KEM Approach
| Feature | PSK + HBSS | ML-KEM (Kyber) |
| :--- | :--- | :--- |
| **SRAM Usage** | Very Low (~780 bytes) | High (Often > 2-3 KB for encaps/decaps) |
| **Computational Cost**| Low (mainly AES/SHA256) | Moderate to High (NTT, polynomial math) |
| **Code Size** | Small (reuses standard crypto) | Large (requires complex lattice math libraries) |
| **Key Distribution** | Out-of-band PSK | Public Key Infrastructure (PKI) |
| **Scalability** | Poor (O(N) keys or single central key) | Excellent (Standard PKI) |

## 9. Concrete Next Steps
1. Implement a proof-of-concept for PSK-HBSS targeting the Arduino Nano, focusing strictly on XMSS verification and AES-GCM decryption.
2. Benchmark execution time for XMSS verification on the ATmega328P to validate real-time constraints.
3. Design a secure bootloader or provisioning mechanism for safely injecting the PSK during device manufacturing.
4. Investigate lightweight stateless hash-based signatures (like SPHINCS+) to see if verification can be tuned for Nano, bypassing the strict state requirements of XMSS for the signer, though SPHINCS+ signatures are typically very large.
