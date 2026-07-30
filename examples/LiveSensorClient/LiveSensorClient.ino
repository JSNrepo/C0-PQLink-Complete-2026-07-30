#include <C0PQLink.h>
#include <string.h>

/*
 * C0-PQLink transport template.
 *
 * This sketch is compile-safe but intentionally cannot connect until the
 * three fail-closed adapter methods below are replaced:
 *   1. PublicKeySource reads the 800-byte server ML-KEM-512 public key from
 *      flash/PROGMEM without copying it into SRAM.
 *   2. RandomSource uses a real TRNG, secure element, or correctly
 *      provisioned and persistently reseeded DRBG. Never use analogRead().
 *   3. Transport sends and receives opaque frames over LoRa, NB-IoT, UART
 *      modem, radio, or another packet link.
 *
 * See examples/README.md for complete adapter contracts and provisioning.
 */

class DevicePublicKey final : public C0PQLink::PublicKeySource {
public:
  uint8_t read(uint16_t offset) override {
    (void)offset;
    /*
     * Replace with a flash/PROGMEM read from the generated 800-byte public
     * key. Returning zero is a fail-closed placeholder, not provisioning.
     */
    return 0;
  }
};

class DeviceRandom final : public C0PQLink::RandomSource {
public:
  int fill(uint8_t *output, size_t length) override {
    /*
     * Replace with a board TRNG, secure element, or properly provisioned
     * DRBG. The placeholder refuses to claim that weak entropy is secure.
     */
    memset(output, 0, length);
    return -1;
  }
};

class DeviceTransport final : public C0PQLink::Transport {
public:
  int send(const uint8_t *frame, size_t frameLength) override {
    (void)frame;
    (void)frameLength;
    return -1;
  }

  int receive(
      uint8_t *frame,
      size_t frameCapacity,
      size_t &frameLength,
      uint32_t timeoutMs) override {
    (void)frame;
    (void)frameCapacity;
    (void)timeoutMs;
    frameLength = 0;
    return -1;
  }
};

static C0PQLink::Client pqClient;
static C0PQLink::Workspace pqWorkspace;
static DevicePublicKey publicKey;
static DeviceRandom randomSource;
static DeviceTransport transport;

static const uint8_t deviceId[8] = {
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
};

/* Replace with protected, per-device provisioning. */
static uint8_t devicePsk[32];
static uint8_t serverKeyId[16];

void setup() {
  memset(devicePsk, 0, sizeof(devicePsk));
  memset(serverKeyId, 0, sizeof(serverKeyId));
  if (pqClient.begin(
    deviceId,
    devicePsk,
    1,
    serverKeyId,
    publicKey,
    randomSource,
    transport,
    C0PQLink::PQ_BOOTSTRAP_RATCHET
  ) != C0PQLINK_OK) {
    /* All-zero provisioning is deliberately rejected here. */
    return;
  }
  /*
   * After real provisioning is installed, this returns C0PQLINK_ERR_RNG
   * until DeviceRandom::fill is replaced. A real application should enter
   * its communication-safe state on any error.
   */
  (void)pqClient.connect(pqWorkspace);
}

void loop() {
  /*
   * Once connected, encode at most C0PQ_RECORD_PLAINTEXT_MAX bytes and call
   * pqClient.seal(...). The application owns packet scheduling and sleep.
   */
}
