import {
  createHash,
  createHmac,
  randomBytes,
  timingSafeEqual,
} from 'node:crypto';
import { ml_kem512 } from '@noble/post-quantum/ml-kem.js';

export const WIRE_VERSION = 1;
export const SUITE = 0x0001;
export const FRAME = Object.freeze({
  HELLO: 1,
  CHALLENGE: 2,
  CIPHERTEXT_FRAGMENT: 3,
  CIPHERTEXT_ACK: 4,
  DEVICE_FINISHED: 5,
  SERVER_FINISHED: 6,
  DATA: 7,
  ERROR: 127,
});
export const MODE = Object.freeze({
  FULL_PQ_EACH_SESSION: 0,
  PQ_BOOTSTRAP_RATCHET: 1,
});

const HEADER_BYTES = 10;
const TAG_BYTES = 16;
const TRANSCRIPT_PREFIX = Buffer.from('C0PQ/1 transcript');
const MASK64 = (1n << 64n) - 1n;

function u16(value) {
  const out = Buffer.alloc(2);
  out.writeUInt16BE(value);
  return out;
}

function u64(value) {
  const out = Buffer.alloc(8);
  out.writeBigUInt64BE(BigInt(value));
  return out;
}

export function encodeHeader(type, flags, payloadLength, sessionId) {
  if (payloadLength > 255) throw new RangeError('payload too large');
  const output = Buffer.alloc(HEADER_BYTES);
  output[0] = 0x43;
  output[1] = 0x30;
  output[2] = WIRE_VERSION;
  output[3] = type;
  output[4] = flags;
  output[5] = payloadLength;
  output.writeUInt32BE(sessionId >>> 0, 6);
  return output;
}

export function decodeHeader(frame) {
  const bytes = Buffer.from(frame);
  if (
    bytes.length < HEADER_BYTES
    || bytes[0] !== 0x43
    || bytes[1] !== 0x30
    || bytes[2] !== WIRE_VERSION
    || bytes[5] + HEADER_BYTES !== bytes.length
  ) throw new Error('invalid C0-PQLink frame header');
  return {
    type: bytes[3],
    flags: bytes[4],
    payloadLength: bytes[5],
    sessionId: bytes.readUInt32BE(6),
  };
}

function hmac(key, ...parts) {
  const context = createHmac('sha256', key);
  for (const part of parts) context.update(part);
  return context.digest();
}

function hkdfExtract(salt, ikm) {
  return hmac(salt ?? Buffer.alloc(32), ikm);
}

function hkdfExpand(prk, info, length) {
  if (length > 255 * 32) throw new RangeError('HKDF output too large');
  const output = Buffer.alloc(length);
  let previous = Buffer.alloc(0);
  let produced = 0;
  for (let counter = 1; produced < length; counter += 1) {
    previous = hmac(prk, previous, info, Buffer.from([counter]));
    const take = Math.min(previous.length, length - produced);
    previous.copy(output, produced, 0, take);
    produced += take;
  }
  return output;
}

function authTag(key, label, data) {
  return hmac(key, Buffer.from(label), data).subarray(0, TAG_BYTES);
}

function tagEqual(left, right) {
  const a = Buffer.from(left);
  const b = Buffer.from(right);
  return a.length === b.length && timingSafeEqual(a, b);
}

export function deriveHelloKey(psk) {
  const early = hkdfExtract(null, psk);
  return hkdfExpand(early, Buffer.from('C0PQ/1 hello key'), 32);
}

export function deriveSessionAuthKey({
  psk,
  suite,
  epoch,
  keyId,
  deviceNonce,
  serverNonce,
}) {
  const early = hkdfExtract(null, psk);
  return hmac(
    early,
    Buffer.from('C0PQ/1 session auth'),
    u16(suite),
    u64(epoch),
    keyId,
    deviceNonce,
    serverNonce,
  );
}

function expandTextLabel(secret, label, transcriptHash, length) {
  return hkdfExpand(
    secret,
    Buffer.concat([Buffer.from(label), Buffer.from([0]), transcriptHash]),
    length,
  );
}

export function deriveSessionKeys(psk, kemSharedSecret, transcriptHash) {
  const early = hkdfExtract(null, psk);
  const derived = hkdfExpand(early, Buffer.from('C0PQ/1 derived'), 32);
  const handshake = hkdfExtract(derived, kemSharedSecret);
  return {
    deviceFinishedKey: expandTextLabel(
      handshake, 'C0PQ/1 device finished', transcriptHash, 32,
    ),
    serverFinishedKey: expandTextLabel(
      handshake, 'C0PQ/1 server finished', transcriptHash, 32,
    ),
    clientTrafficKey: expandTextLabel(
      handshake, 'C0PQ/1 client traffic', transcriptHash, 16,
    ),
    serverTrafficKey: expandTextLabel(
      handshake, 'C0PQ/1 server traffic', transcriptHash, 16,
    ),
    clientNonceBase: expandTextLabel(
      handshake, 'C0PQ/1 client nonce', transcriptHash, 16,
    ),
    serverNonceBase: expandTextLabel(
      handshake, 'C0PQ/1 server nonce', transcriptHash, 16,
    ),
    clientChainKey: expandTextLabel(
      handshake, 'C0PQ/1 client chain', transcriptHash, 32,
    ),
    serverChainKey: expandTextLabel(
      handshake, 'C0PQ/1 server chain', transcriptHash, 32,
    ),
  };
}

function finishedTag(key, type, sessionId, transcriptHash) {
  const header = encodeHeader(type, 0, TAG_BYTES, sessionId);
  return authTag(
    key,
    'C0PQ/1 finished tag',
    Buffer.concat([header, transcriptHash]),
  );
}

function loadLE(bytes, offset = 0, length = 8) {
  let value = 0n;
  for (let index = 0; index < length; index += 1) {
    value |= BigInt(bytes[offset + index]) << BigInt(8 * index);
  }
  return value;
}

function storeLE(value, length = 8) {
  const output = Buffer.alloc(length);
  let current = value;
  for (let index = 0; index < length; index += 1) {
    output[index] = Number(current & 0xffn);
    current >>= 8n;
  }
  return output;
}

function rotateRight64(value, count) {
  const shift = BigInt(count);
  return ((value >> shift) | (value << (64n - shift))) & MASK64;
}

function asconRound(state, constant) {
  state[2] ^= BigInt(constant);
  state[0] ^= state[4];
  state[4] ^= state[3];
  state[2] ^= state[1];
  let t0 = state[0] ^ ((~state[1] & MASK64) & state[2]);
  let t1 = state[1] ^ ((~state[2] & MASK64) & state[3]);
  let t2 = state[2] ^ ((~state[3] & MASK64) & state[4]);
  let t3 = state[3] ^ ((~state[4] & MASK64) & state[0]);
  const t4 = state[4] ^ ((~state[0] & MASK64) & state[1]);
  t1 ^= t0;
  t0 ^= t4;
  t3 ^= t2;
  t2 = ~t2 & MASK64;
  state[0] = t0 ^ rotateRight64(t0, 19) ^ rotateRight64(t0, 28);
  state[1] = t1 ^ rotateRight64(t1, 61) ^ rotateRight64(t1, 39);
  state[2] = t2 ^ rotateRight64(t2, 1) ^ rotateRight64(t2, 6);
  state[3] = t3 ^ rotateRight64(t3, 10) ^ rotateRight64(t3, 17);
  state[4] = t4 ^ rotateRight64(t4, 7) ^ rotateRight64(t4, 41);
  for (let index = 0; index < 5; index += 1) state[index] &= MASK64;
}

const P12 = [0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b];
const P8 = P12.slice(4);

function permute(state, constants) {
  for (const constant of constants) asconRound(state, constant);
}

function initializeAscon(key, nonce) {
  const key0 = loadLE(key);
  const key1 = loadLE(key, 8);
  const state = [
    0x00001000808c0001n,
    key0,
    key1,
    loadLE(nonce),
    loadLE(nonce, 8),
  ];
  permute(state, P12);
  state[3] ^= key0;
  state[4] ^= key1;
  return state;
}

function pad(position) {
  return 1n << BigInt(8 * position);
}

function absorbAssociatedData(state, associatedData) {
  let offset = 0;
  if (associatedData.length > 0) {
    while (associatedData.length - offset >= 16) {
      state[0] ^= loadLE(associatedData, offset);
      state[1] ^= loadLE(associatedData, offset + 8);
      permute(state, P8);
      offset += 16;
    }
    const remaining = associatedData.length - offset;
    if (remaining >= 8) {
      state[0] ^= loadLE(associatedData, offset);
      state[1] ^= loadLE(associatedData, offset + 8, remaining - 8);
      state[1] ^= pad(remaining - 8);
    } else {
      state[0] ^= loadLE(associatedData, offset, remaining);
      state[0] ^= pad(remaining);
    }
    permute(state, P8);
  }
  state[4] ^= 0x8000000000000000n;
}

function finalizeAscon(state, key) {
  const key0 = loadLE(key);
  const key1 = loadLE(key, 8);
  state[2] ^= key0;
  state[3] ^= key1;
  permute(state, P12);
  state[3] ^= key0;
  state[4] ^= key1;
  return Buffer.concat([storeLE(state[3]), storeLE(state[4])]);
}

export function asconEncrypt(key, nonce, associatedData, plaintext) {
  const message = Buffer.from(plaintext);
  const state = initializeAscon(key, nonce);
  absorbAssociatedData(state, Buffer.from(associatedData));
  const ciphertext = Buffer.alloc(message.length);
  let offset = 0;
  while (message.length - offset >= 16) {
    state[0] ^= loadLE(message, offset);
    state[1] ^= loadLE(message, offset + 8);
    storeLE(state[0]).copy(ciphertext, offset);
    storeLE(state[1]).copy(ciphertext, offset + 8);
    permute(state, P8);
    offset += 16;
  }
  const remaining = message.length - offset;
  if (remaining >= 8) {
    state[0] ^= loadLE(message, offset);
    state[1] ^= loadLE(message, offset + 8, remaining - 8);
    storeLE(state[0]).copy(ciphertext, offset);
    storeLE(state[1], remaining - 8).copy(ciphertext, offset + 8);
    state[1] ^= pad(remaining - 8);
  } else {
    state[0] ^= loadLE(message, offset, remaining);
    storeLE(state[0], remaining).copy(ciphertext, offset);
    state[0] ^= pad(remaining);
  }
  return { ciphertext, tag: finalizeAscon(state, key) };
}

function clearLowBytes(value, length) {
  let output = value;
  for (let index = 0; index < length; index += 1) {
    output &= ~(0xffn << BigInt(8 * index));
  }
  return output & MASK64;
}

export function asconDecrypt(key, nonce, associatedData, ciphertext, tag) {
  const input = Buffer.from(ciphertext);
  const state = initializeAscon(key, nonce);
  absorbAssociatedData(state, Buffer.from(associatedData));
  const plaintext = Buffer.alloc(input.length);
  let offset = 0;
  while (input.length - offset >= 16) {
    const c0 = loadLE(input, offset);
    const c1 = loadLE(input, offset + 8);
    storeLE(state[0] ^ c0).copy(plaintext, offset);
    storeLE(state[1] ^ c1).copy(plaintext, offset + 8);
    state[0] = c0;
    state[1] = c1;
    permute(state, P8);
    offset += 16;
  }
  const remaining = input.length - offset;
  if (remaining >= 8) {
    const c0 = loadLE(input, offset);
    const c1 = loadLE(input, offset + 8, remaining - 8);
    storeLE(state[0] ^ c0).copy(plaintext, offset);
    storeLE(state[1] ^ c1, remaining - 8).copy(plaintext, offset + 8);
    state[0] = c0;
    state[1] = clearLowBytes(state[1], remaining - 8) | c1;
    state[1] ^= pad(remaining - 8);
  } else {
    const c0 = loadLE(input, offset, remaining);
    storeLE(state[0] ^ c0, remaining).copy(plaintext, offset);
    state[0] = clearLowBytes(state[0], remaining) | c0;
    state[0] ^= pad(remaining);
  }
  const expected = finalizeAscon(state, key);
  return tagEqual(expected, tag) ? plaintext : null;
}

function recordNonce(base, sequence) {
  const output = Buffer.from(base);
  const encoded = u64(sequence);
  for (let index = 0; index < 8; index += 1) output[8 + index] ^= encoded[index];
  return output;
}

function ratchetStep(chainKey) {
  return {
    messageKey: hmac(
      chainKey, Buffer.from('C0PQ/1 ratchet message'),
    ).subarray(0, 16),
    nextChainKey: hmac(chainKey, Buffer.from('C0PQ/1 ratchet next')),
  };
}

function parseHello(frame, deviceLookup) {
  const header = decodeHeader(frame);
  if (
    frame.length !== 60
    || header.type !== FRAME.HELLO
    || header.flags !== 0
    || header.payloadLength !== 50
  ) throw new Error('invalid hello shape');
  const suite = frame.readUInt16BE(10);
  const deviceId = frame.subarray(12, 20);
  const epoch = frame.readBigUInt64BE(20);
  const deviceNonce = frame.subarray(28, 44);
  const device = deviceLookup(deviceId);
  if (!device || suite !== SUITE) throw new Error('unknown device or suite');
  const expected = authTag(
    deriveHelloKey(device.psk),
    'C0PQ/1 hello frame',
    frame.subarray(0, 44),
  );
  if (!tagEqual(expected, frame.subarray(44))) throw new Error('hello authentication failed');
  return {
    header, suite, deviceId, epoch, deviceNonce, device,
  };
}

function encodeChallenge({
  sessionId,
  psk,
  epoch,
  keyId,
  deviceNonce,
  serverNonce,
}) {
  const core = Buffer.concat([
    encodeHeader(FRAME.CHALLENGE, 0, 74, sessionId),
    u16(SUITE),
    u64(epoch),
    keyId,
    deviceNonce,
    serverNonce,
  ]);
  const sessionAuthKey = deriveSessionAuthKey({
    psk,
    suite: SUITE,
    epoch,
    keyId,
    deviceNonce,
    serverNonce,
  });
  return {
    frame: Buffer.concat([
      core,
      authTag(sessionAuthKey, 'C0PQ/1 challenge frame', core),
    ]),
    sessionAuthKey,
  };
}

function encodeAck(session, fragmentIndex) {
  const core = Buffer.concat([
    encodeHeader(FRAME.CIPHERTEXT_ACK, 0, 18, session.sessionId),
    Buffer.from([fragmentIndex, 0]),
  ]);
  return Buffer.concat([
    core,
    authTag(session.sessionAuthKey, 'C0PQ/1 ciphertext ack', core),
  ]);
}

export class C0PQPeer {
  constructor({
    keyPair,
    devices,
    epoch = 1n,
    keyId,
    random = randomBytes,
    onPlaintext = (plaintext) => plaintext,
  }) {
    if (!keyPair?.publicKey || !keyPair?.secretKey) {
      throw new TypeError('ML-KEM-512 keyPair is required');
    }
    if (
      keyPair.publicKey.length !== 800
      || keyPair.secretKey.length !== 1632
    ) {
      throw new RangeError('invalid ML-KEM-512 key lengths');
    }
    this.keyPair = {
      publicKey: Buffer.from(keyPair.publicKey),
      secretKey: Buffer.from(keyPair.secretKey),
    };
    this.epoch = BigInt(epoch);
    this.keyId = keyId
      ? Buffer.from(keyId)
      : createHash('sha256').update(this.keyPair.publicKey).digest().subarray(0, 16);
    if (this.keyId.length !== 16) {
      throw new RangeError('keyId must be 16 bytes');
    }
    this.random = random;
    this.onPlaintext = onPlaintext;
    this.sessions = new Map();
    this.devices = devices instanceof Map
      ? devices
      : new Map(Object.entries(devices ?? {}));
  }

  lookupDevice(deviceId) {
    const device = this.devices.get(Buffer.from(deviceId).toString('hex'));
    if (!device) return null;
    const psk = Buffer.from(device.psk);
    const mode = device.mode ?? MODE.PQ_BOOTSTRAP_RATCHET;
    if (
      psk.length !== 32
      || psk.every((value) => value === 0)
    ) throw new RangeError('device PSK must be 32 non-placeholder bytes');
    if (
      mode !== MODE.FULL_PQ_EACH_SESSION
      && mode !== MODE.PQ_BOOTSTRAP_RATCHET
    ) throw new RangeError('invalid device mode');
    return {
      ...device,
      psk,
      mode,
    };
  }

  handleHello(frame) {
    const parsed = parseHello(frame, (id) => this.lookupDevice(id));
    const existing = this.sessions.get(parsed.header.sessionId);
    if (existing) {
      if (!tagEqual(existing.helloCore, frame.subarray(0, 44))) {
        throw new Error('session identifier collision');
      }
      return existing.challengeFrame;
    }
    if (parsed.epoch !== this.epoch) throw new Error('epoch mismatch');
    const serverNonce = Buffer.from(this.random(16));
    if (serverNonce.length !== 16) {
      throw new Error('peer random source returned the wrong length');
    }
    const challenge = encodeChallenge({
      sessionId: parsed.header.sessionId,
      psk: parsed.device.psk,
      epoch: this.epoch,
      keyId: this.keyId,
      deviceNonce: parsed.deviceNonce,
      serverNonce,
    });
    const session = {
      sessionId: parsed.header.sessionId,
      device: parsed.device,
      helloCore: Buffer.from(frame.subarray(0, 44)),
      challengeCore: Buffer.from(challenge.frame.subarray(0, 68)),
      challengeFrame: challenge.frame,
      sessionAuthKey: challenge.sessionAuthKey,
      ciphertext: Buffer.alloc(768),
      received: new Array(16).fill(false),
      established: false,
      sendSequence: 0n,
      receiveSequence: 0n,
    };
    this.sessions.set(session.sessionId, session);
    return challenge.frame;
  }

  handleFragment(frame, header) {
    const session = this.sessions.get(header.sessionId);
    if (
      !session
      || header.flags !== 0
      || frame.length < 29
    ) throw new Error('unknown or invalid fragment session');
    const index = frame[10];
    const total = frame[11];
    const length = frame[12];
    const coreLength = 13 + length;
    if (
      total !== 16
      || index >= total
      || length !== 48
      || frame.length !== coreLength + TAG_BYTES
    ) throw new Error('invalid fragment shape');
    const expected = authTag(
      session.sessionAuthKey,
      'C0PQ/1 ciphertext fragment',
      frame.subarray(0, coreLength),
    );
    if (!tagEqual(expected, frame.subarray(coreLength))) {
      throw new Error('fragment authentication failed');
    }
    const chunk = frame.subarray(13, coreLength);
    const offset = index * 48;
    if (
      session.received[index]
      && !tagEqual(session.ciphertext.subarray(offset, offset + length), chunk)
    ) throw new Error('conflicting fragment replay');
    chunk.copy(session.ciphertext, offset);
    session.received[index] = true;
    return encodeAck(session, index);
  }

  finishSession(frame, header) {
    const session = this.sessions.get(header.sessionId);
    if (
      !session
      || header.flags !== 0
      || header.payloadLength !== TAG_BYTES
      || frame.length !== 26
    ) throw new Error('unknown or invalid finished session');
    if (session.serverFinishedFrame) {
      const duplicateExpected = finishedTag(
        session.keys.deviceFinishedKey,
        FRAME.DEVICE_FINISHED,
        session.sessionId,
        session.transcriptHash,
      );
      if (!tagEqual(duplicateExpected, frame.subarray(10))) {
        throw new Error('device Finished verification failed');
      }
      return session.serverFinishedFrame;
    }
    if (!session.received.every(Boolean)) throw new Error('ciphertext incomplete');
    const transcriptHash = createHash('sha256')
      .update(TRANSCRIPT_PREFIX)
      .update(session.helloCore)
      .update(session.challengeCore)
      .update(session.ciphertext)
      .digest();
    const kemSharedSecret = Buffer.from(
      ml_kem512.decapsulate(session.ciphertext, this.keyPair.secretKey),
    );
    session.keys = deriveSessionKeys(
      session.device.psk,
      kemSharedSecret,
      transcriptHash,
    );
    session.transcriptHash = transcriptHash;
    const expected = finishedTag(
      session.keys.deviceFinishedKey,
      FRAME.DEVICE_FINISHED,
      session.sessionId,
      transcriptHash,
    );
    if (!tagEqual(expected, frame.subarray(10))) {
      session.keys = undefined;
      throw new Error('device Finished verification failed');
    }
    session.established = true;
    session.serverFinishedFrame = Buffer.concat([
      encodeHeader(FRAME.SERVER_FINISHED, 0, TAG_BYTES, session.sessionId),
      finishedTag(
        session.keys.serverFinishedKey,
        FRAME.SERVER_FINISHED,
        session.sessionId,
        transcriptHash,
      ),
    ]);
    return session.serverFinishedFrame;
  }

  openRecord(session, frame, header) {
    if (!session.established || header.flags !== 0 || header.payloadLength < 24) {
      throw new Error('invalid client record');
    }
    const sequence = frame.readBigUInt64BE(10);
    if (sequence !== session.receiveSequence) throw new Error('record replay or gap');
    const ciphertextLength = header.payloadLength - 24;
    if (ciphertextLength > 48) throw new Error('record too large');
    let messageKey = session.keys.clientTrafficKey;
    let nextChainKey;
    if (session.device.mode === MODE.PQ_BOOTSTRAP_RATCHET) {
      ({ messageKey, nextChainKey } = ratchetStep(session.keys.clientChainKey));
    }
    const nonce = recordNonce(session.keys.clientNonceBase, sequence);
    const plaintext = asconDecrypt(
      messageKey,
      nonce,
      frame.subarray(0, 18),
      frame.subarray(18, 18 + ciphertextLength),
      frame.subarray(18 + ciphertextLength),
    );
    if (plaintext === null) throw new Error('record authentication failed');
    if (nextChainKey) session.keys.clientChainKey = nextChainKey;
    session.receiveSequence += 1n;
    return plaintext;
  }

  sealRecord(session, plaintext) {
    const message = Buffer.from(plaintext);
    if (message.length > 48) throw new RangeError('record plaintext too large');
    let messageKey = session.keys.serverTrafficKey;
    let nextChainKey;
    if (session.device.mode === MODE.PQ_BOOTSTRAP_RATCHET) {
      ({ messageKey, nextChainKey } = ratchetStep(session.keys.serverChainKey));
    }
    const header = encodeHeader(
      FRAME.DATA,
      1,
      8 + message.length + TAG_BYTES,
      session.sessionId,
    );
    const sequence = u64(session.sendSequence);
    const associatedData = Buffer.concat([header, sequence]);
    const nonce = recordNonce(session.keys.serverNonceBase, session.sendSequence);
    const sealed = asconEncrypt(messageKey, nonce, associatedData, message);
    if (nextChainKey) session.keys.serverChainKey = nextChainKey;
    session.sendSequence += 1n;
    return Buffer.concat([
      associatedData,
      sealed.ciphertext,
      sealed.tag,
    ]);
  }

  handleData(frame, header) {
    const session = this.sessions.get(header.sessionId);
    if (!session) throw new Error('unknown data session');
    if (
      session.lastClientFrame
      && tagEqual(session.lastClientFrame, frame)
    ) {
      return session.lastResponseFrame
        ? Buffer.from(session.lastResponseFrame)
        : null;
    }
    const plaintext = this.openRecord(session, frame, header);
    const response = this.onPlaintext(plaintext, session);
    const responseFrame = response == null
      ? null : this.sealRecord(session, response);
    session.lastClientFrame = Buffer.from(frame);
    session.lastResponseFrame = responseFrame
      ? Buffer.from(responseFrame) : null;
    return responseFrame;
  }

  handleFrame(input) {
    const frame = Buffer.from(input);
    const header = decodeHeader(frame);
    switch (header.type) {
      case FRAME.HELLO:
        return this.handleHello(frame);
      case FRAME.CIPHERTEXT_FRAGMENT:
        return this.handleFragment(frame, header);
      case FRAME.DEVICE_FINISHED:
        return this.finishSession(frame, header);
      case FRAME.DATA:
        return this.handleData(frame, header);
      default:
        throw new Error(`unsupported frame type ${header.type}`);
    }
  }
}

export function generateKeyPair(seed) {
  return ml_kem512.keygen(seed);
}
