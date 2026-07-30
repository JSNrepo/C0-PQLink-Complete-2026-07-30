#!/usr/bin/env node
import dgram from 'node:dgram';
import { readFile } from 'node:fs/promises';
import { C0PQPeer, generateKeyPair, MODE } from './protocol.mjs';

const configPath = process.argv[2] ?? 'reference-peer/config.json';
const config = JSON.parse(await readFile(configPath, 'utf8'));

function decodeHex(name, value, length) {
  if (
    typeof value !== 'string'
    || value.length !== length * 2
    || !/^[0-9a-f]+$/i.test(value)
  ) throw new Error(`${name} must encode exactly ${length} bytes`);
  return Buffer.from(value, 'hex');
}

function parseMode(value) {
  if (value === 'FULL_PQ_EACH_SESSION') return MODE.FULL_PQ_EACH_SESSION;
  if (value === 'PQ_BOOTSTRAP_RATCHET') return MODE.PQ_BOOTSTRAP_RATCHET;
  throw new Error(`unsupported session mode ${value}`);
}

const keySeed = decodeHex('mlKemKeySeedHex', config.mlKemKeySeedHex, 64);
const devices = new Map(
  config.devices.map((device) => [
    decodeHex('deviceIdHex', device.deviceIdHex, 8).toString('hex'),
    {
      psk: decodeHex('pskHex', device.pskHex, 32),
      mode: parseMode(device.mode),
    },
  ]),
);
const protocol = new C0PQPeer({
  keyPair: generateKeyPair(keySeed),
  devices,
  epoch: BigInt(config.epoch),
  onPlaintext: (plaintext) => {
    process.stdout.write(`sensor ${plaintext.toString('utf8')}\n`);
    return Buffer.from('accepted');
  },
});
const socket = dgram.createSocket('udp4');
socket.on('message', (message, remote) => {
  try {
    const response = protocol.handleFrame(message);
    if (response) socket.send(response, remote.port, remote.address);
  } catch (error) {
    process.stderr.write(`rejected ${remote.address}: ${error.message}\n`);
  }
});
socket.bind(config.port ?? 47050, config.host ?? '0.0.0.0', () => {
  const address = socket.address();
  process.stdout.write(`C0-PQLink reference peer on ${address.address}:${address.port}\n`);
});
