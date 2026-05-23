/**
 * Basic smoke tests for the WSJTX library.
 *
 * Kept intentionally fast (<5 s) so they can run in CI on every PR.
 * Heavy round-trip and option-coverage tests live in `wsjtx.test.ts`.
 */

import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { WSJTXLib, WSJTXMode, WSJTXError } from '../src/index.js';

describe('WSJTX library — smoke', () => {
  let lib: WSJTXLib;

  beforeEach(() => {
    lib = new WSJTXLib({ maxThreads: 4 });
  });

  it('constructs a library instance', () => {
    assert.ok(lib instanceof WSJTXLib);
  });

  it('reports FT8 default encode sample rate of 12 kHz', () => {
    assert.strictEqual(lib.getSampleRate(WSJTXMode.FT8), 12000);
  });

  it('supports 48 kHz encode sample rate opt-in', () => {
    const result = spawnSync(process.execPath, [
      '--input-type=module',
      '--eval',
      "import { WSJTXLib, WSJTXMode } from './dist/src/index.js'; const lib = new WSJTXLib({ encodeSampleRate: 48000 }); if (lib.getSampleRate(WSJTXMode.FT8) !== 48000) process.exit(1);",
    ], {
      cwd: process.cwd(),
      encoding: 'utf8',
    });
    assert.strictEqual(result.status, 0, result.stderr || result.stdout);
  });

  it('reports FT8 supports both encode and decode', () => {
    assert.ok(lib.isEncodingSupported(WSJTXMode.FT8));
    assert.ok(lib.isDecodingSupported(WSJTXMode.FT8));
  });

  it('reports Q65 supports both encode and decode', () => {
    assert.ok(lib.isEncodingSupported(WSJTXMode.Q65));
    assert.ok(lib.isDecodingSupported(WSJTXMode.Q65));
    assert.strictEqual(lib.getSampleRate(WSJTXMode.Q65), 12000);
    assert.strictEqual(lib.getTransmissionDuration(WSJTXMode.Q65), 60.0);
  });

  it('reports JT65 is decode-only', () => {
    assert.strictEqual(lib.isEncodingSupported(WSJTXMode.JT65), false);
    assert.ok(lib.isDecodingSupported(WSJTXMode.JT65));
  });

  it('numeric mode enum values match expectations', () => {
    assert.strictEqual(WSJTXMode.FT8, 0);
    assert.strictEqual(WSJTXMode.FT4, 1);
    assert.strictEqual(WSJTXMode.Q65, 6);
    assert.strictEqual(WSJTXMode.JT65JT9, 8);
    assert.strictEqual(WSJTXMode.WSPR, 9);
  });

  it('returns capabilities for all 10 modes', () => {
    const caps = lib.getAllModeCapabilities();
    assert.strictEqual(caps.length, 10);
  });

  it('rejects invalid mode in decode', async () => {
    await assert.rejects(
      () => lib.decode(999 as unknown as WSJTXMode, new Float32Array(1000), { frequency: 1500 }),
      WSJTXError,
    );
  });

  it('rejects negative frequency in decode', async () => {
    await assert.rejects(
      () => lib.decode(WSJTXMode.FT8, new Float32Array(1000), { frequency: -1 }),
      WSJTXError,
    );
  });

  it('rejects invalid encode sample rate', () => {
    assert.throws(
      () => new WSJTXLib({ encodeSampleRate: 44100 as 12000 }),
      WSJTXError,
    );
  });

  it('rejects empty audio in decode', async () => {
    await assert.rejects(
      () => lib.decode(WSJTXMode.FT8, new Float32Array(0), { frequency: 1500 }),
      WSJTXError,
    );
  });

  it('pullMessages returns an array', () => {
    assert.ok(Array.isArray(lib.pullMessages()));
  });

  it('Float32→Int16 audio conversion produces an Int16Array', async () => {
    const out = await lib.convertAudioFormat(new Float32Array([-1, 0, 0.5, 1]), 'int16');
    assert.ok(out instanceof Int16Array);
  });

  it('WSJTXError has a code field and extends Error', () => {
    const e = new WSJTXError('boom', 'CODE');
    assert.ok(e instanceof Error);
    assert.strictEqual(e.code, 'CODE');
  });

  it('decode of silence completes successfully with empty messages', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(12000 * 13), {
      frequency: 1500,
      threads: 1,
    });
    assert.strictEqual(r.success, true);
    assert.deepStrictEqual(r.messages, []);
  });

  it('Q65 encode accepts legacy thread-count argument and emits a 60 s frame', async () => {
    const encoded = await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, 1);
    assert.ok(encoded.audioData instanceof Float32Array);
    assert.strictEqual(encoded.sampleRate, 12000);
    assert.strictEqual(encoded.audioData.length, 12000 * 60);
    assert.ok(encoded.messageSent.trim().length > 0);
  });

  it('Q65 encode accepts object options for period and submode', async () => {
    const q65ThirtyB = await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, {
      threads: 1,
      q65Period: 30,
      q65Submode: 'B',
    });
    assert.strictEqual(q65ThirtyB.audioData.length, 12000 * 30);

    const q65OneTwentyE = await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, {
      threads: 1,
      q65Period: 120,
      q65Submode: 4,
    });
    assert.strictEqual(q65OneTwentyE.audioData.length, 12000 * 120);
  });

  it('Q65 self round-trip decodes a generated 30A frame', async () => {
    const encoded = await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, {
      threads: 1,
      q65Period: 30,
      q65Submode: 'A',
    });
    const decoded = await lib.decode(WSJTXMode.Q65, encoded.audioData, {
      frequency: 1500,
      txFrequency: 1500,
      threads: 1,
      lowFreq: 0,
      highFreq: 5000,
      tolerance: 5000,
      q65Period: 30,
      q65Submode: 'A',
      q65MaxDrift: 50,
      q65ClearAveraging: true,
    });

    assert.strictEqual(decoded.success, true);
    assert.ok(decoded.messages.some((m) => m.text.trim() === 'CQ K1ABC FN20'));
  });

  it('rejects invalid Q65 encode options before reaching native code', async () => {
    await assert.rejects(
      () => lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, { q65Period: 45 as 60 }),
      WSJTXError,
    );
    await assert.rejects(
      () => lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, { q65Submode: 'F' as 'A' }),
      WSJTXError,
    );
  });

  it('Q65 decode accepts period, submode, drift, and averaging controls', async () => {
    const r = await lib.decode(WSJTXMode.Q65, new Float32Array(12000 * 30), {
      frequency: 1500,
      threads: 1,
      lowFreq: 200,
      highFreq: 4000,
      tolerance: 50,
      q65Period: 30,
      q65Submode: 'B',
      q65MaxDrift: 50,
      q65ClearAveraging: true,
      q65SingleDecode: true,
      q65Averaging: true,
    });
    assert.strictEqual(r.success, true);
    assert.ok(Array.isArray(r.messages));
  });

  it('decode accepts dxCall, dxGrid, and freq range options without crashing', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(12000 * 13), {
      frequency: 1500,
      threads: 1,
      dxCall: 'K1ABC',
      dxGrid: 'FN20',
      lowFreq: 200,
      highFreq: 4000,
      tolerance: 20,
    });
    assert.strictEqual(r.success, true);
  });
});