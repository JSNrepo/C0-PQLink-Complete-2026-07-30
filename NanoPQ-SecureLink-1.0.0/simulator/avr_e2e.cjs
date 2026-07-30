#!/usr/bin/env node
"use strict";

const crypto = require("crypto");
const fs = require("fs");
const path = require("path");
const {
  CPU,
  avrInstruction,
  AVRADC,
  adcConfig,
  AVREEPROM,
  EEPROMMemoryBackend,
  AVRIOPort,
  portBConfig,
  AVRTimer,
  timer1Config,
  AVRUSART,
  usart0Config,
} = require("avr8js");

const F_CPU = 16_000_000;
const RXC0 = 0x80;
const MASK64 = (1n << 64n) - 1n;
const ROOT_KEY = Buffer.from(
  "1b476ce00b4587917ec4ee14a25e1af6" +
    "b53ab3396d6c281f5a7a84c716673342",
  "hex"
);
const DEVICE_ID = Buffer.from("03afe9d98940f1d2", "hex");

const FRAME = {
  HELLO: 1,
  CHALLENGE: 2,
  CLIENT_FINISHED: 3,
  SERVER_FINISHED: 4,
  DATA: 5,
  ENROLL_REQUIRED: 6,
  ENROLL_BEGIN: 7,
  ENROLL_RESULT: 8,
  ENROLL_CHUNK_READY: 9,
};

function expect(condition, message) {
  if (!condition) throw new Error(`assertion failed: ${message}`);
}

function loadIntelHex(filename) {
  const bytes = new Uint8Array(32 * 1024);
  let upper = 0;
  for (const rawLine of fs.readFileSync(filename, "utf8").split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line) continue;
    if (line[0] !== ":") throw new Error("invalid Intel HEX line");
    const count = Number.parseInt(line.slice(1, 3), 16);
    const address = Number.parseInt(line.slice(3, 7), 16);
    const type = Number.parseInt(line.slice(7, 9), 16);
    if (type === 0) {
      for (let index = 0; index < count; index++) {
        const absolute = upper + address + index;
        if (absolute >= bytes.length) throw new Error("HEX exceeds Nano flash");
        bytes[absolute] = Number.parseInt(
          line.slice(9 + index * 2, 11 + index * 2),
          16
        );
      }
    } else if (type === 4) {
      upper = Number.parseInt(line.slice(9, 13), 16) << 16;
    } else if (type === 1) {
      break;
    }
  }
  return new Uint16Array(bytes.buffer);
}

function parseDataEnd(mapPath) {
  const map = fs.readFileSync(mapPath, "utf8");
  const match = map.match(/0x([0-9a-fA-F]+)\s+_end = \./);
  if (!match) throw new Error("could not locate _end in AVR linker map");
  return Number.parseInt(match[1], 16) & 0xffff;
}

class NanoSimulation {
  constructor(program, eepromBackend) {
    this.frames = [];
    this.transmitLength = null;
    this.transmitBytes = [];
    this.cpu = new CPU(program, 2048);
    this.portB = new AVRIOPort(this.cpu, portBConfig);
    this.usart = new AVRUSART(this.cpu, usart0Config, F_CPU);
    this.timer1 = new AVRTimer(this.cpu, timer1Config);
    this.adc = new AVRADC(this.cpu, adcConfig);
    this.adc.channelValues[0] = 2.5;
    this.eeprom = new AVREEPROM(this.cpu, eepromBackend);
    this.trackStack = false;
    this.minimumStackPointer = 0x8ff;
    this.usart.onByteTransmit = (value) => {
      if (this.transmitLength === null) {
        this.transmitLength = value;
        this.transmitBytes = [];
      } else {
        this.transmitBytes.push(value);
        if (this.transmitBytes.length === this.transmitLength) {
          this.frames.push(Buffer.from(this.transmitBytes));
          this.transmitLength = null;
          this.transmitBytes = [];
        }
      }
    };
  }

  runUntil(predicate, maxInstructions, label) {
    for (let index = 0; index < maxInstructions; index++) {
      if (predicate()) return;
      avrInstruction(this.cpu);
      this.cpu.tick();
      if (this.trackStack && this.cpu.SP < this.minimumStackPointer) {
        this.minimumStackPointer = this.cpu.SP;
      }
    }
    throw new Error(`simulation timeout: ${label}`);
  }

  boot() {
    this.runUntil(
      () => this.usart.rxEnable && this.frames.length !== 0,
      10_000_000,
      "firmware boot"
    );
  }

  startStackTracking() {
    this.minimumStackPointer = this.cpu.SP;
    this.trackStack = true;
  }

  sendByte(value) {
    this.runUntil(
      () => this.usart.rxEnable &&
        !(this.cpu.data[usart0Config.UCSRA] & RXC0),
      30_000_000,
      "USART ready for host byte"
    );
    if (this.usart.writeByte(value, true) === false) {
      throw new Error("USART rejected a host byte");
    }
    this.runUntil(
      () => !(this.cpu.data[usart0Config.UCSRA] & RXC0),
      30_000_000,
      "firmware consume host byte"
    );
  }

  sendBuffer(buffer) {
    for (const value of buffer) this.sendByte(value);
  }

  sendFrame(frame) {
    expect(frame.length > 0 && frame.length <= 64, "valid frame length");
    this.sendByte(frame.length);
    this.sendBuffer(frame);
  }

  nextFrame(cursor, label, maxInstructions = 800_000_000) {
    this.runUntil(
      () => this.frames.length > cursor,
      maxInstructions,
      label
    );
    return this.frames[cursor];
  }

  get ledOn() {
    return this.portB.pinState(5) === 1;
  }
}

function header(type) {
  return Buffer.from([0x4e, 0x51, 1, type]);
}

function u32be(value) {
  const output = Buffer.alloc(4);
  output.writeUInt32BE(value >>> 0);
  return output;
}

function hmac(key, message) {
  return crypto.createHmac("sha256", key).update(message).digest();
}

function hmacLabel(key, label, data) {
  return hmac(
    key,
    Buffer.concat([Buffer.from(label, "ascii"), Buffer.from([0]), data])
  );
}

function verifyHello(hello) {
  return hello.length === 48 &&
    hello.subarray(0, 4).equals(header(FRAME.HELLO)) &&
    hello.subarray(4, 12).equals(DEVICE_ID) &&
    hello.readUInt32BE(12) !== 0 &&
    crypto.timingSafeEqual(
      hello.subarray(32, 48),
      hmacLabel(ROOT_KEY, "NPQ/1 hello", hello.subarray(0, 32))
        .subarray(0, 16)
    );
}

function makeChallenge(hello, serverNonce) {
  const output = Buffer.alloc(60);
  header(FRAME.CHALLENGE).copy(output);
  hello.subarray(12, 16).copy(output, 4);
  hello.subarray(16, 32).copy(output, 8);
  serverNonce.copy(output, 24);
  hmacLabel(
    ROOT_KEY,
    "NPQ/1 session id",
    output.subarray(0, 40)
  ).copy(output, 40, 0, 4);
  hmacLabel(
    ROOT_KEY,
    "NPQ/1 challenge",
    output.subarray(0, 44)
  ).copy(output, 44, 0, 16);
  return output;
}

function transcriptHash(hello, challenge) {
  return crypto.createHash("sha256")
    .update(Buffer.from("NPQ/1 transcript", "ascii"))
    .update(hello)
    .update(challenge)
    .digest();
}

function expandLabel(prk, label, transcript) {
  return hmac(
    prk,
    Buffer.concat([
      Buffer.from(label, "ascii"),
      Buffer.from([0]),
      transcript,
      Buffer.from([1]),
    ])
  );
}

function deriveServerSession(hello, challenge) {
  const transcript = transcriptHash(hello, challenge);
  const prk = hmac(transcript, ROOT_KEY);
  return {
    sendChain: expandLabel(prk, "NPQ/1 server chain", transcript),
    receiveChain: expandLabel(prk, "NPQ/1 client chain", transcript),
    sendNonceBase: expandLabel(
      prk,
      "NPQ/1 server nonce",
      transcript
    ).subarray(0, 16),
    receiveNonceBase: expandLabel(
      prk,
      "NPQ/1 client nonce",
      transcript
    ).subarray(0, 16),
    sessionId: challenge.readUInt32BE(40),
    bootEpoch: hello.readUInt32BE(12),
    sendSequence: 0,
    receiveSequence: 0,
  };
}

function makeFinished(type, hello, challenge) {
  const transcript = transcriptHash(hello, challenge);
  const prk = hmac(transcript, ROOT_KEY);
  const keyLabel = type === FRAME.CLIENT_FINISHED
    ? "NPQ/1 client finished"
    : "NPQ/1 server finished";
  const proofLabel = type === FRAME.CLIENT_FINISHED
    ? "NPQ/1 client proof"
    : "NPQ/1 server proof";
  const key = expandLabel(prk, keyLabel, transcript);
  const output = Buffer.alloc(24);
  header(type).copy(output);
  challenge.subarray(40, 44).copy(output, 4);
  hmacLabel(
    key,
    proofLabel,
    Buffer.concat([output.subarray(0, 8), transcript])
  ).copy(output, 8, 0, 16);
  return output;
}

function ratchet(chain, sequence) {
  const encoded = u32be(sequence);
  return {
    messageKey: hmacLabel(
      chain,
      "NPQ/1 message key",
      encoded
    ).subarray(0, 16),
    nextChain: hmacLabel(chain, "NPQ/1 next chain", encoded),
  };
}

function makeNonce(base, sequence) {
  const nonce = Buffer.from(base);
  const encoded = u32be(sequence);
  for (let index = 0; index < 4; index++) {
    nonce[12 + index] ^= encoded[index];
  }
  return nonce;
}

function loadLE(bytes, offset, length) {
  let value = 0n;
  for (let index = 0; index < length; index++) {
    value |= BigInt(bytes[offset + index]) << BigInt(8 * index);
  }
  return value & MASK64;
}

function storeLE(value, length) {
  const output = Buffer.alloc(length);
  for (let index = 0; index < length; index++) {
    output[index] = Number((value >> BigInt(8 * index)) & 0xffn);
  }
  return output;
}

function clearLowBytes(value, length) {
  let output = value;
  for (let index = 0; index < length; index++) {
    output &= ~(0xffn << BigInt(8 * index));
  }
  return output & MASK64;
}

function rotateRight64(value, count) {
  const shift = BigInt(count);
  return ((value >> shift) | (value << (64n - shift))) & MASK64;
}

function asconRound(state, constant) {
  let [x0, x1, x2, x3, x4] = state;
  x2 = (x2 ^ BigInt(constant)) & MASK64;
  x0 = (x0 ^ x4) & MASK64;
  x4 = (x4 ^ x3) & MASK64;
  x2 = (x2 ^ x1) & MASK64;
  let t0 = (x0 ^ (((~x1) & MASK64) & x2)) & MASK64;
  let t1 = (x1 ^ (((~x2) & MASK64) & x3)) & MASK64;
  let t2 = (x2 ^ (((~x3) & MASK64) & x4)) & MASK64;
  let t3 = (x3 ^ (((~x4) & MASK64) & x0)) & MASK64;
  let t4 = (x4 ^ (((~x0) & MASK64) & x1)) & MASK64;
  t1 = (t1 ^ t0) & MASK64;
  t0 = (t0 ^ t4) & MASK64;
  t3 = (t3 ^ t2) & MASK64;
  t2 = (~t2) & MASK64;
  state[0] = (t0 ^ rotateRight64(t0, 19) ^ rotateRight64(t0, 28)) & MASK64;
  state[1] = (t1 ^ rotateRight64(t1, 61) ^ rotateRight64(t1, 39)) & MASK64;
  state[2] = (t2 ^ rotateRight64(t2, 1) ^ rotateRight64(t2, 6)) & MASK64;
  state[3] = (t3 ^ rotateRight64(t3, 10) ^ rotateRight64(t3, 17)) & MASK64;
  state[4] = (t4 ^ rotateRight64(t4, 7) ^ rotateRight64(t4, 41)) & MASK64;
}

function asconPermute(state, firstRound) {
  for (let round = firstRound; round < 12; round++) {
    asconRound(state, ((15 - round) << 4) | round);
  }
}

function asconInitialize(key, nonce) {
  const key0 = loadLE(key, 0, 8);
  const key1 = loadLE(key, 8, 8);
  const state = [
    0x00001000808c0001n,
    key0,
    key1,
    loadLE(nonce, 0, 8),
    loadLE(nonce, 8, 8),
  ];
  asconPermute(state, 0);
  state[3] = (state[3] ^ key0) & MASK64;
  state[4] = (state[4] ^ key1) & MASK64;
  return state;
}

function asconAbsorbAssociatedData(state, data) {
  let offset = 0;
  let length = data.length;
  if (length !== 0) {
    while (length >= 16) {
      state[0] = (state[0] ^ loadLE(data, offset, 8)) & MASK64;
      state[1] = (state[1] ^ loadLE(data, offset + 8, 8)) & MASK64;
      asconPermute(state, 4);
      offset += 16;
      length -= 16;
    }
    if (length >= 8) {
      state[0] = (state[0] ^ loadLE(data, offset, 8)) & MASK64;
      state[1] = (
        state[1] ^ loadLE(data, offset + 8, length - 8) ^
        (1n << BigInt(8 * (length - 8)))
      ) & MASK64;
    } else {
      state[0] = (
        state[0] ^ loadLE(data, offset, length) ^
        (1n << BigInt(8 * length))
      ) & MASK64;
    }
    asconPermute(state, 4);
  }
  state[4] = (state[4] ^ 0x8000000000000000n) & MASK64;
}

function asconFinalize(state, key) {
  const key0 = loadLE(key, 0, 8);
  const key1 = loadLE(key, 8, 8);
  state[2] = (state[2] ^ key0) & MASK64;
  state[3] = (state[3] ^ key1) & MASK64;
  asconPermute(state, 0);
  state[3] = (state[3] ^ key0) & MASK64;
  state[4] = (state[4] ^ key1) & MASK64;
  return Buffer.concat([storeLE(state[3], 8), storeLE(state[4], 8)]);
}

function asconEncrypt(key, nonce, associatedData, plaintext) {
  const state = asconInitialize(key, nonce);
  asconAbsorbAssociatedData(state, associatedData);
  const ciphertext = Buffer.alloc(plaintext.length);
  let offset = 0;
  let length = plaintext.length;
  while (length >= 16) {
    state[0] = (state[0] ^ loadLE(plaintext, offset, 8)) & MASK64;
    state[1] = (state[1] ^ loadLE(plaintext, offset + 8, 8)) & MASK64;
    storeLE(state[0], 8).copy(ciphertext, offset);
    storeLE(state[1], 8).copy(ciphertext, offset + 8);
    asconPermute(state, 4);
    offset += 16;
    length -= 16;
  }
  if (length >= 8) {
    state[0] = (state[0] ^ loadLE(plaintext, offset, 8)) & MASK64;
    state[1] = (
      state[1] ^ loadLE(plaintext, offset + 8, length - 8)
    ) & MASK64;
    storeLE(state[0], 8).copy(ciphertext, offset);
    storeLE(state[1], length - 8).copy(ciphertext, offset + 8);
    state[1] = (
      state[1] ^ (1n << BigInt(8 * (length - 8)))
    ) & MASK64;
  } else {
    state[0] = (
      state[0] ^ loadLE(plaintext, offset, length)
    ) & MASK64;
    storeLE(state[0], length).copy(ciphertext, offset);
    state[0] = (state[0] ^ (1n << BigInt(8 * length))) & MASK64;
  }
  return { ciphertext, tag: asconFinalize(state, key) };
}

function asconDecrypt(key, nonce, associatedData, ciphertext, tag) {
  const state = asconInitialize(key, nonce);
  asconAbsorbAssociatedData(state, associatedData);
  const plaintext = Buffer.alloc(ciphertext.length);
  let offset = 0;
  let length = ciphertext.length;
  while (length >= 16) {
    const c0 = loadLE(ciphertext, offset, 8);
    const c1 = loadLE(ciphertext, offset + 8, 8);
    storeLE(state[0] ^ c0, 8).copy(plaintext, offset);
    storeLE(state[1] ^ c1, 8).copy(plaintext, offset + 8);
    state[0] = c0;
    state[1] = c1;
    asconPermute(state, 4);
    offset += 16;
    length -= 16;
  }
  if (length >= 8) {
    const c0 = loadLE(ciphertext, offset, 8);
    const c1 = loadLE(ciphertext, offset + 8, length - 8);
    storeLE(state[0] ^ c0, 8).copy(plaintext, offset);
    storeLE(state[1] ^ c1, length - 8).copy(plaintext, offset + 8);
    state[0] = c0;
    state[1] = (
      clearLowBytes(state[1], length - 8) | c1 ^
      (1n << BigInt(8 * (length - 8)))
    ) & MASK64;
  } else {
    const c0 = loadLE(ciphertext, offset, length);
    storeLE(state[0] ^ c0, length).copy(plaintext, offset);
    state[0] = (
      (clearLowBytes(state[0], length) | c0) ^
      (1n << BigInt(8 * length))
    ) & MASK64;
  }
  const expectedTag = asconFinalize(state, key);
  return crypto.timingSafeEqual(expectedTag, tag) ? plaintext : null;
}

function seal(session, plaintext) {
  const sequence = session.sendSequence;
  const outputHeader = Buffer.concat([
    header(FRAME.DATA),
    u32be(session.sessionId),
    u32be(sequence),
    Buffer.from([plaintext.length]),
  ]);
  const material = ratchet(session.sendChain, sequence);
  const nonce = makeNonce(session.sendNonceBase, sequence);
  const encrypted = asconEncrypt(
    material.messageKey,
    nonce,
    outputHeader,
    plaintext
  );
  session.sendChain = material.nextChain;
  session.sendSequence++;
  return Buffer.concat([
    outputHeader,
    encrypted.ciphertext,
    encrypted.tag,
  ]);
}

function openRecord(session, frame) {
  if (frame.length < 29 ||
      !frame.subarray(0, 4).equals(header(FRAME.DATA)) ||
      frame.readUInt32BE(4) !== session.sessionId ||
      frame.readUInt32BE(8) !== session.receiveSequence ||
      frame.length !== 29 + frame[12]) {
    return null;
  }
  const sequence = session.receiveSequence;
  const material = ratchet(session.receiveChain, sequence);
  const nonce = makeNonce(session.receiveNonceBase, sequence);
  const plaintextLength = frame[12];
  const plaintext = asconDecrypt(
    material.messageKey,
    nonce,
    frame.subarray(0, 13),
    frame.subarray(13, 13 + plaintextLength),
    frame.subarray(13 + plaintextLength)
  );
  if (plaintext !== null) {
    session.receiveChain = material.nextChain;
    session.receiveSequence++;
  }
  return plaintext;
}

function establish(board, cursor, hello, serverNonce) {
  expect(verifyHello(hello), "authenticated device hello");
  const challenge = makeChallenge(hello, serverNonce);
  const session = deriveServerSession(hello, challenge);
  board.sendFrame(challenge);
  const clientFinished = board.nextFrame(
    cursor++,
    "client key confirmation"
  );
  expect(
    clientFinished.equals(
      makeFinished(FRAME.CLIENT_FINISHED, hello, challenge)
    ),
    "client key confirmation"
  );
  board.sendFrame(makeFinished(FRAME.SERVER_FINISHED, hello, challenge));
  return { cursor, challenge, session };
}

function receiveSensor(board, cursor, session, trace, label) {
  const frame = board.nextFrame(cursor++, label);
  const plaintext = openRecord(session, frame);
  expect(
    plaintext !== null && plaintext.length === 7 && plaintext[0] === 1,
    `${label} authenticated sensor record`
  );
  const entry = {
    phase: label,
    adc: plaintext.readUInt16BE(1),
    boot_epoch: plaintext.readUInt32BE(3),
    sequence: frame.readUInt32BE(8),
  };
  trace.push(entry);
  return { cursor, entry };
}

function receiveStatus(board, cursor, session, accepted, trace, label) {
  const frame = board.nextFrame(cursor++, label);
  const plaintext = openRecord(session, frame);
  expect(
    plaintext !== null && plaintext.length === 3 &&
      plaintext[0] === 2 && plaintext[1] === accepted,
    label
  );
  trace.push({
    phase: label,
    accepted: plaintext[1] === 1,
    led: plaintext[2] === 1,
    sequence: frame.readUInt32BE(8),
  });
  return cursor;
}

function main() {
  const root = path.resolve(__dirname, "..");
  const hexPath = process.argv[2] || path.join(root, "build/avr/nanopq.hex");
  const mapPath = process.argv[3] || path.join(root, "build/avr/nanopq.map");
  const outputPath =
    process.argv[4] || path.join(root, "evidence/avr-e2e.json");
  const manifest = fs.readFileSync(
    path.join(root, "tests/vectors/cutover_manifest.bin")
  );
  const program = loadIntelHex(hexPath);
  const dataEnd = parseDataEnd(mapPath);
  const eeprom = new EEPROMMemoryBackend(1024);
  const trace = [];
  let board = new NanoSimulation(program, eeprom);
  let cursor = 0;
  board.boot();
  let frame = board.frames[cursor++];
  expect(
    frame.length === 5 && frame[3] === FRAME.ENROLL_REQUIRED,
    "factory Nano requests enrollment"
  );
  const authorizationId = frame[4];
  const authorization = authorizationId === 1
    ? {
        id: 1,
        name: "FIPS 205 SLH-DSA-SHA2-128s",
        signatureFile: "slhdsa_signature.bin",
        stateModel: "stateless signer",
      }
    : authorizationId === 2
      ? {
          id: 2,
          name: "RFC 8554 LMS_SHA256_M32_H5 + LMOTS_SHA256_N32_W4",
          signatureFile: "lms_w4_signature.bin",
          stateModel: "stateful off-device signer; stateless verifier",
        }
      : authorizationId === 3
        ? {
            id: 3,
            name: "RFC 8554 LMS_SHA256_M32_H5 + LMOTS_SHA256_N32_W8",
            signatureFile: "lms_w8_signature.bin",
            stateModel: "stateful off-device signer; stateless verifier",
          }
      : null;
  expect(authorization !== null, "known authorization algorithm");
  const signature = fs.readFileSync(
    path.join(root, "tests/vectors", authorization.signatureFile)
  );

  const stackPattern = 0xa5;
  const baselineStackPointer = board.cpu.SP;
  board.cpu.data.fill(stackPattern, dataEnd, baselineStackPointer);
  board.startStackTracking();

  board.sendFrame(Buffer.concat([header(FRAME.ENROLL_BEGIN), manifest]));
  let signatureOffset = 0;
  let signatureStartCycles = 0;
  let chunkRequests = 0;
  for (;;) {
    frame = board.nextFrame(cursor++, "streamed post-quantum enrollment");
    if (frame[3] === FRAME.ENROLL_CHUNK_READY) {
      expect(frame.length === 6, "well-formed enrollment chunk request");
      const requested = frame.readUInt16BE(4);
      expect(
        requested > 0 && signatureOffset + requested <= signature.length,
        "bounded signature chunk request"
      );
      if (signatureOffset === 0) signatureStartCycles = board.cpu.cycles;
      board.sendBuffer(
        signature.subarray(signatureOffset, signatureOffset + requested)
      );
      signatureOffset += requested;
      chunkRequests++;
    } else {
      expect(
        frame.length === 5 && frame[3] === FRAME.ENROLL_RESULT &&
          frame[4] === 1,
        "valid post-quantum enrollment accepted"
      );
      break;
    }
  }
  const signatureCycles = board.cpu.cycles - signatureStartCycles;
  expect(signatureOffset === signature.length, "complete signature consumed");
  trace.push({
    phase: "post_quantum_enrollment",
    algorithm: authorization.name,
    signature_bytes: signatureOffset,
    verifier_driven_chunks: chunkRequests,
    executed_cycles: signatureCycles,
  });

  const hello = board.nextFrame(cursor++, "first authenticated hello");
  expect(hello[3] === FRAME.HELLO && verifyHello(hello), "valid first hello");
  const firstEpoch = hello.readUInt32BE(12);
  expect(firstEpoch === 1, "first boot epoch is one");
  const firstHandshake = establish(
    board,
    cursor,
    hello,
    Buffer.from("202122232425262728292a2b2c2d2e2f", "hex")
  );
  cursor = firstHandshake.cursor;
  const session = firstHandshake.session;
  trace.push({
    phase: "mutual_handshake",
    boot_epoch: firstEpoch,
    session_id: session.sessionId,
  });

  let sensor = receiveSensor(
    board,
    cursor,
    session,
    trace,
    "sensor_before_tamper"
  );
  cursor = sensor.cursor;
  const command = seal(session, Buffer.from([0x10, 1]));
  const tampered = Buffer.from(command);
  tampered[tampered.length - 1] ^= 1;
  board.sendFrame(tampered);
  cursor = receiveStatus(
    board,
    cursor,
    session,
    0,
    trace,
    "tampered_command_rejected"
  );

  sensor = receiveSensor(
    board,
    cursor,
    session,
    trace,
    "sensor_after_tamper"
  );
  cursor = sensor.cursor;
  board.sendFrame(command);
  cursor = receiveStatus(
    board,
    cursor,
    session,
    1,
    trace,
    "valid_command_accepted"
  );
  expect(board.ledOn, "valid encrypted command controls physical LED pin");

  sensor = receiveSensor(
    board,
    cursor,
    session,
    trace,
    "sensor_before_replay"
  );
  cursor = sensor.cursor;
  board.sendFrame(command);
  cursor = receiveStatus(
    board,
    cursor,
    session,
    0,
    trace,
    "replayed_command_rejected"
  );

  let lowestTouched = baselineStackPointer;
  for (let address = dataEnd; address < baselineStackPointer; address++) {
    if (board.cpu.data[address] !== stackPattern) {
      lowestTouched = address;
      break;
    }
  }
  const stackTop = 0x8ff;
  const watermarkPeakStackBytes = stackTop - lowestTouched + 1;
  const minimumSpPeakStackBytes = stackTop - board.minimumStackPointer;
  const measuredPeakStackBytes = Math.max(
    watermarkPeakStackBytes,
    minimumSpPeakStackBytes
  );
  const measuredMinimumStackPointer = board.minimumStackPointer;
  const staticSramBytes = dataEnd - 0x100;
  const totalPeakSramBytes = staticSramBytes + measuredPeakStackBytes;
  const measuredHeadroomBytes = 2048 - totalPeakSramBytes;

  board = new NanoSimulation(program, eeprom);
  cursor = 0;
  board.boot();
  const secondHello = board.frames[cursor++];
  expect(
    secondHello[3] === FRAME.HELLO && verifyHello(secondHello),
    "authorized state survives reset without reenrollment"
  );
  const secondEpoch = secondHello.readUInt32BE(12);
  expect(secondEpoch === 2 && secondEpoch > firstEpoch, "boot epoch advances");
  const secondHandshake = establish(
    board,
    cursor,
    secondHello,
    Buffer.from("303132333435363738393a3b3c3d3e3f", "hex")
  );
  cursor = secondHandshake.cursor;
  expect(
    !secondHandshake.session.receiveChain.equals(session.receiveChain),
    "reset derives a fresh traffic chain"
  );
  receiveSensor(
    board,
    cursor,
    secondHandshake.session,
    trace,
    "sensor_after_reset"
  );
  trace.push({
    phase: "persistent_reset_safety",
    first_epoch: firstEpoch,
    second_epoch: secondEpoch,
    reenrollment_required: false,
  });

  const report = {
    schema: 1,
    label: "EXECUTED_COMPILED_NANOPQ_FIRMWARE_IN_AVR8JS",
    physical_hardware_label: "PHYSICAL_NANO_RUN_STILL_REQUIRED",
    target: "ATmega328P / classic Arduino Nano",
    simulator: "avr8js 0.21.0",
    result: "PASS",
    assertions: {
      post_quantum_signature_streamed_and_verified: true,
      signature_not_buffered_in_sram: true,
      mutual_session_key_confirmation: true,
      ascon_sensor_record_decrypted: true,
      tampered_command_rejected_without_ratchet_advance: true,
      valid_command_accepted: true,
      replayed_command_rejected: true,
      eeprom_authorization_survives_reset: true,
      boot_epoch_advances_after_reset: true,
      post_reset_traffic_keys_change: true,
    },
    post_quantum_authorization: {
      profile: authorization.name,
      state_model: authorization.stateModel,
      signature_bytes: signatureOffset,
      verifier_driven_chunks: chunkRequests,
      executed_cycles: signatureCycles,
      milliseconds_at_16mhz: Number(
        ((signatureCycles / F_CPU) * 1000).toFixed(3)
      ),
    },
    memory: {
      linker_static_sram_bytes: staticSramBytes,
      baseline_stack_pointer: baselineStackPointer,
      minimum_executed_stack_pointer: measuredMinimumStackPointer,
      lowest_touched_address: lowestTouched,
      executed_peak_stack_bytes: measuredPeakStackBytes,
      minimum_sp_peak_stack_bytes: minimumSpPeakStackBytes,
      watermark_peak_stack_bytes: watermarkPeakStackBytes,
      executed_total_peak_sram_bytes: totalPeakSramBytes,
      executed_sram_headroom_bytes: measuredHeadroomBytes,
      measurement:
        "maximum of minimum-SP tracking and 0xA5 SRAM watermark over the " +
        "executed full path",
    },
    trace,
    validation_boundary:
      "Compiled AVR execution and protocol evidence; not a physical-board " +
      "measurement, CAVP/CMVP validation, or a claim of public-key forward secrecy",
  };
  fs.mkdirSync(path.dirname(outputPath), { recursive: true });
  fs.writeFileSync(outputPath, JSON.stringify(report, null, 2) + "\n");
  console.log(JSON.stringify(report, null, 2));
}

if (require.main === module) {
  main();
}
