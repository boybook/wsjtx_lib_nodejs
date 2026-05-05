/**
 * Basic smoke tests for the WSJTX library.
 *
 * Kept intentionally fast (<5 s) so they can run in CI on every PR.
 * Heavy round-trip and option-coverage tests live in `wsjtx.test.ts`.
 */

import { describe, it, beforeEach } from 'node:test';
import assert from 'node:assert';
import { WSJTXLib, WSJTXMode, WSJTXError } from '../src/index.js';

describe('WSJTX library — smoke', () => {
  let lib: WSJTXLib;

  beforeEach(() => {
    lib = new WSJTXLib({ maxThreads: 4 });
  });

  it('constructs a library instance', () => {
    assert.ok(lib instanceof WSJTXLib);
  });

  it('reports FT8 sample rate of 48 kHz', () => {
    assert.strictEqual(lib.getSampleRate(WSJTXMode.FT8), 48000);
  });

  it('reports FT8 supports both encode and decode', () => {
    assert.ok(lib.isEncodingSupported(WSJTXMode.FT8));
    assert.ok(lib.isDecodingSupported(WSJTXMode.FT8));
  });

  it('reports JT65 is decode-only', () => {
    assert.strictEqual(lib.isEncodingSupported(WSJTXMode.JT65), false);
    assert.ok(lib.isDecodingSupported(WSJTXMode.JT65));
  });

  it('numeric mode enum values match expectations', () => {
    assert.strictEqual(WSJTXMode.FT8, 0);
    assert.strictEqual(WSJTXMode.FT4, 1);
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
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000 * 13), {
      frequency: 1500,
      threads: 1,
    });
    assert.strictEqual(r.success, true);
    assert.deepStrictEqual(r.messages, []);
  });

  it('decode accepts dxCall, dxGrid, and freq range options without crashing', async () => {
    const r = await lib.decode(WSJTXMode.FT8, new Float32Array(48000 * 13), {
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
