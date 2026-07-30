# Physical Nano runbook

## Equipment

- classic Arduino Nano with ATmega328P;
- USB cable and working bootloader;
- Linux/macOS POSIX host;
- optional potentiometer or sensor on A0;
- built-in LED on PB5/D13.

## Build

```bash
npm ci --cache .npm-cache
make verify
make benchmark
make peer
```

Default image:

```text
build/avr-lms-w4/nanopq.hex
```

## Locate the serial port

Typical Linux ports are `/dev/ttyUSB0` or `/dev/ttyACM0`. Ensure the current
user can access the port. Do not run the peer while `avrdude` is uploading.

## Clear prior demo state

```bash
make factory-reset PORT=/dev/ttyUSB0
```

This writes the firmware's zero-initialized 36-byte EEPROM section, making both
journal slots invalid. The next boot must perform post-quantum enrollment.

## Flash

Old Nano bootloader:

```bash
make flash PORT=/dev/ttyUSB0
```

New bootloader:

```bash
make flash PORT=/dev/ttyUSB0 UPLOAD_BAUD=115200
```

`make flash` selects LMS H5/W4. The firmware UART runs at 115,200 baud
independently of the bootloader setting.

## Run the peer

```bash
build/host/nanopq-peer \
  --port /dev/ttyUSB0 \
  --state build/nanopq-peer.state \
  --led on \
  --tamper-demo
```

Expected milestones:

```text
ENROLL: RFC 8554 LMS H5/W4 policy received; streaming 2348 bytes with verifier-driven flow control
PASS: Nano verified the streamed signature without buffering it in SRAM
PASS: mutual HMAC/HKDF session established at boot epoch 1
PASS: Ascon-AEAD128 sensor record ...
PASS: tampered command rejected without ratchet advance ...
PASS: valid encrypted LED-on command accepted ...
PASS: replayed command rejected ...
RESULT: PASS ...
```

The LED should remain unchanged for the tampered command, change for the valid
command, and remain unchanged for the replay.

## Reset demonstration

Stop the peer, press the Nano reset button, and rerun the same peer command.
The accepted epoch must increase and enrollment must not repeat. Deleting the
peer state file while keeping the Nano EEPROM is intentionally detected as an
epoch-policy mismatch and requires an explicit operator recovery decision.

## Alternative profiles

```bash
make factory-reset PORT=/dev/ttyUSB0
make flash-lms-w8 PORT=/dev/ttyUSB0
build/host/nanopq-peer --port /dev/ttyUSB0 --tamper-demo
```

```bash
make factory-reset PORT=/dev/ttyUSB0
make flash-slh PORT=/dev/ttyUSB0
build/host/nanopq-peer --port /dev/ttyUSB0 --tamper-demo
```

LMS/W8 should enroll more slowly despite its shorter signature. SLH-DSA has no
stateful-signer requirement.

## Evidence to record

For a review, capture:

- exact Nano board and bootloader;
- compiler version and full build command;
- `avr-size` output;
- peer console log;
- video of tamper, valid command, replay, and reset behavior;
- physical runtime measured with a host timestamp;
- supply voltage/current and energy if available;
- SRAM canary or debug measurement on the physical image.

Do not relabel simulator timing or SRAM as physical-board measurement.

