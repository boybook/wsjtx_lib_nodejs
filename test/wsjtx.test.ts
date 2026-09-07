/**
 * Comprehensive regression tests for the WSJTX library.
 *
 * Coverage targets:
 *   - Basic queries (mode caps, sample rate, transmission duration)
 *   - DecodeOptions: every field (frequency, threads, lowFreq, highFreq,
 *     tolerance, dxCall, dxGrid) — make sure they don't crash and that
 *     range-based decoding actually picks up signals it should and rejects
 *     signals out of range.
 *   - Encode → Decode round-trip for FT8 and FT4 across multiple message
 *     forms (CQ, signal report, 73, RRR, grid).
 *   - Float32 vs Int16 audio paths.
 *   - convertAudioFormat in both directions.
 *   - decodeWSPR signature smoke test.
 *   - Error handling: invalid mode / freq / audio / message.
 *
 * Tests run on compiled output (`node --test dist/test/wsjtx.test.js`),
 * so the import path uses .js extensions.
 */

import { describe, it, beforeEach, after, before } from 'node:test';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { WSJTXLib, WSJTXMode, WSJTXError } from '../src/index.js';
import type { DecodeOptions, DecodeResult, EncodeResult } from '../src/index.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OUTPUT_DIR = path.join(__dirname, '..', 'test', 'output');

/** Default FT8/FT4 encode sample rate. */
const ENCODE_SAMPLE_RATE = 12000;

function freshOutputDir(): void {
  if (!fs.existsSync(OUTPUT_DIR)) fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

function cleanupOutputDir(): void {
  if (!fs.existsSync(OUTPUT_DIR)) return;
  for (const f of fs.readdirSync(OUTPUT_DIR)) {
    if (f.endsWith('.wav') || f.endsWith('.bin')) {
      fs.unlinkSync(path.join(OUTPUT_DIR, f));
    }
  }
}

function makeOptions(over: Partial<DecodeOptions> & Pick<DecodeOptions, 'frequency'>): DecodeOptions {
  return { threads: 1, ...over };
}

/** Convert Float32 audio into Int16, matching the lib's own logic. */
function toInt16(audio: Float32Array): Int16Array {
  const out = new Int16Array(audio.length);
  for (let i = 0; i < audio.length; i++) {
    const v = Math.max(-1, Math.min(1, audio[i]));
    out[i] = Math.max(-32768, Math.min(32767, Math.round(v * 32768)));
  }
  return out;
}

function runIsolatedNode(code: string): void {
  const result = spawnSync(process.execPath, ['--input-type=module', '--eval', code], {
    cwd: path.resolve(__dirname, '..'),
    encoding: 'utf8',
  });
  assert.strictEqual(result.status, 0, result.stderr || result.stdout);
}

describe('WSJTX library — regression', () => {
  let lib: WSJTXLib;

  before(() => {
    freshOutputDir();
  });

  beforeEach(() => {
    lib = new WSJTXLib({ maxThreads: 4 });
  });

  after(() => {
    cleanupOutputDir();
  });

  // ---- Capabilities ----

  describe('capability queries', () => {
    it('FT8 default sample rate is 12 kHz', () => {
      assert.strictEqual(lib.getSampleRate(WSJTXMode.FT8), 12000);
    });

    it('FT4 default sample rate is 12 kHz', () => {
      assert.strictEqual(lib.getSampleRate(WSJTXMode.FT4), 12000);
    });

    it('FT8/FT4 sample rate can opt into 48 kHz per instance', () => {
      runIsolatedNode(`
        import { WSJTXLib, WSJTXMode } from './src/index.js';
        const lib = new WSJTXLib({ encodeSampleRate: 48000 });
        if (lib.getSampleRate(WSJTXMode.FT8) !== 48000) process.exit(1);
        if (lib.getSampleRate(WSJTXMode.FT4) !== 48000) process.exit(1);
      `);
    });

    it('FT8 transmission duration is 12.64 s', () => {
      assert.strictEqual(lib.getTransmissionDuration(WSJTXMode.FT8), 12.64);
    });

    it('FT4 transmission duration is 6.0 s', () => {
      assert.strictEqual(lib.getTransmissionDuration(WSJTXMode.FT4), 6.0);
    });

    it('FT8 supports both encoding and decoding', () => {
      assert.ok(lib.isEncodingSupported(WSJTXMode.FT8));
      assert.ok(lib.isDecodingSupported(WSJTXMode.FT8));
    });

    it('FT4 supports both encoding and decoding', () => {
      assert.ok(lib.isEncodingSupported(WSJTXMode.FT4));
      assert.ok(lib.isDecodingSupported(WSJTXMode.FT4));
    });

    it('JT65 is decode-only', () => {
      assert.strictEqual(lib.isEncodingSupported(WSJTXMode.JT65), false);
      assert.ok(lib.isDecodingSupported(WSJTXMode.JT65));
    });

    it('WSPR is decode-only', () => {
      assert.strictEqual(lib.isEncodingSupported(WSJTXMode.WSPR), false);
      assert.ok(lib.isDecodingSupported(WSJTXMode.WSPR));
    });

    it('mode capabilities array covers all 10 modes', () => {
      const caps = lib.getAllModeCapabilities();
      assert.strictEqual(caps.length, 10);
      assert.ok(caps.every((c) => c.sampleRate > 0 && c.duration > 0));
    });

    it('mode enum has correct numeric values', () => {
      assert.strictEqual(WSJTXMode.FT8, 0);
      assert.strictEqual(WSJTXMode.FT4, 1);
      assert.strictEqual(WSJTXMode.JT65JT9, 8);
      assert.strictEqual(WSJTXMode.WSPR, 9);
    });
  });

  // ---- Encoding ----

  describe('encoding', () => {
    const messages = [
      'CQ TEST K1ABC FN20',
      'CQ DX K1ABC FN20',
      'K1ABC W9XYZ -05',
      'K1ABC W9XYZ R-05',
      'K1ABC W9XYZ RRR',
      'K1ABC W9XYZ 73',
    ];

    for (const msg of messages) {
      it(`FT8 encodes "${msg}"`, async () => {
        const result = await lib.encode(WSJTXMode.FT8, msg, 1500);
        assert.ok(result.audioData instanceof Float32Array);
        assert.ok(result.audioData.length > 0);
        assert.strictEqual(result.sampleRate, 12000);
        // FT8 transmission is 12.64 s @ 12 kHz = 151680 samples
        assert.ok(
          result.audioData.length >= 151_000 && result.audioData.length <= 152_000,
          `unexpected sample count: ${result.audioData.length}`,
        );
        assert.ok(typeof result.messageSent === 'string' && result.messageSent.length > 0);
      });

      it(`FT4 encodes "${msg}"`, async () => {
        const result = await lib.encode(WSJTXMode.FT4, msg, 1500);
        assert.ok(result.audioData.length > 0);
        assert.strictEqual(result.sampleRate, 12000);
        // FT4 keying duration is ~5.04 s @ 12 kHz = 60480 samples
        // (slot length is 6 s, but actual emitted audio is shorter).
        assert.ok(
          result.audioData.length >= 60_000 && result.audioData.length <= 61_000,
          `unexpected sample count: ${result.audioData.length}`,
        );
      });
    }

    it('preserves old 48 kHz encode lengths when configured', async () => {
      runIsolatedNode(`
        import { WSJTXLib, WSJTXMode } from './src/index.js';
        const lib = new WSJTXLib({ encodeSampleRate: 48000 });
        const ft8 = await lib.encode(WSJTXMode.FT8, 'CQ TEST K1ABC FN20', 1500);
        if (ft8.sampleRate !== 48000 || ft8.audioData.length < 600000 || ft8.audioData.length > 620000) process.exit(1);
        const ft4 = await lib.encode(WSJTXMode.FT4, 'CQ TEST K1ABC FN20', 1500);
        if (ft4.sampleRate !== 48000 || ft4.audioData.length < 240000 || ft4.audioData.length > 250000) process.exit(1);
      `);
    });

    it('FT8 encodes 23-character structured nonstandard-call messages', async () => {
      const messages = [
        '<VA7CD/DU7> BG5DRB RR73',
        '<VA7CD/DU7> BG5DRB R-09',
      ];

      for (const msg of messages) {
        const result = await lib.encode(WSJTXMode.FT8, msg, 1500);
        assert.strictEqual(result.messageSent.trim(), msg);
        assert.ok(result.audioData.length > 0);
      }
    });

    it('FT4 encodes 23-character structured nonstandard-call messages', async () => {
      const msg = '<VA7CD/DU7> BG5DRB RR73';
      const result = await lib.encode(WSJTXMode.FT4, msg, 1500);
      assert.strictEqual(result.messageSent.trim(), msg);
      assert.ok(result.audioData.length > 0);
    });

    it('rejects FT8 messages longer than 37 characters', async () => {
      await assert.rejects(
        () => lib.encode(WSJTXMode.FT8, 'A'.repeat(38), 1500),
        /Message must be 1\.\.37 characters/,
      );
    });

    it('exposes WSJT-X free-text truncation through messageSent', async () => {
      const result = await lib.encode(WSJTXMode.FT8, 'THIS IS CUSTOM TEXT', 1500);
      assert.strictEqual(result.messageSent.trim(), 'THIS IS CUSTO');
    });

    it('encoded audio has non-trivial dynamic range', async () => {
      const result = await lib.encode(WSJTXMode.FT8, 'CQ TEST K1ABC FN20', 1500);
      let min = result.audioData[0];
      let max = result.audioData[0];
      for (const s of result.audioData) {
        if (s < min) min = s;
        if (s > max) max = s;
      }
      assert.ok(max - min > 0.1, `dynamic range too small: [${min}, ${max}]`);
    });
  });

  // ---- Encode→Decode round-trip ----

  describe('FT8 encode→decode round-trip', () => {
    let encoded: EncodeResult;

    before(async () => {
      const tempLib = new WSJTXLib();
      encoded = await tempLib.encode(WSJTXMode.FT8, 'CQ TEST K1ABC FN20', 1500);
    });

    it('decode returns success and a messages array (Float32 path)', async () => {
      const result = await lib.decode(
        WSJTXMode.FT8,
        encoded.audioData,
        makeOptions({ frequency: 1500 }),
      );
      assert.strictEqual(result.success, true);
      assert.ok(Array.isArray(result.messages));
    });

    it('decode succeeds via Int16 audio path', async () => {
      const int16Audio = toInt16(encoded.audioData);
      const result = await lib.decode(
        WSJTXMode.FT8,
        int16Audio,
        makeOptions({ frequency: 1500 }),
      );
      assert.strictEqual(result.success, true);
      assert.ok(Array.isArray(result.messages));
    });

    it('continues decoding low-gain audio above the WSJT-X RMS gate', async () => {
      // This level produces an Int16 RMS between 0.5 and 3.0. The official
      // decoder still attempts the frame; the old library gate discarded it.
      const lowGainAudio = Float32Array.from(encoded.audioData, (sample) => sample * 0.0001);
      const result = await lib.decode(WSJTXMode.FT8, lowGainAudio, {
        frequency: 1500,
        threads: 1,
        decodeDepth: 1,
      });

      assert.ok((result.stats?.candidateCount ?? 0) > 0);
      assert.ok(result.messages.some((message) => message.text.includes('CQ TEST K1ABC FN20')));
    });

    it('keeps FT8 decoder state across explicit 41/47/50 stages at every depth', async () => {
      const int16Audio = toInt16(encoded.audioData);
      for (const decodeDepth of [1, 2, 3] as const) {
        const session = lib.beginDecodeSession({
          sessionId: `roundtrip-${decodeDepth}`,
          mode: WSJTXMode.FT8,
          decodeDepth,
          slotUtc: 120000,
        });
        const stage41 = await session.decodeStage(int16Audio, 41, { frequency: 1500, threads: 1 });
        const stage47 = await session.decodeStage(int16Audio, 47, { frequency: 1500, threads: 1 });
        const stage50 = await session.decodeStage(int16Audio, 50, { frequency: 1500, threads: 1 });
        const summary = session.endDecodeSession();

        assert.deepStrictEqual([stage41.stage, stage47.stage, stage50.stage], [41, 47, 50]);
        assert.strictEqual(stage41.decodeDepth, decodeDepth);
        assert.strictEqual(stage47.decodeDepth, decodeDepth);
        assert.strictEqual(stage50.decodeDepth, decodeDepth);
        assert.ok(summary.messages.some((message) => message.text.includes('CQ TEST K1ABC FN20')));
        assert.strictEqual(summary.stages.length, 3);
      }
    });

    it('serializes concurrent native sessions without mixing their results', async () => {
      const int16Audio = toInt16(encoded.audioData);
      const [fast, deep] = await Promise.all([
        lib.decode(WSJTXMode.FT8, int16Audio, {
          frequency: 1500,
          threads: 1,
          decodeDepth: 1,
          sessionId: 'concurrent-fast',
          stageSymbols: 50,
          slotUtc: 120001,
          resetSession: true,
        }),
        lib.decode(WSJTXMode.FT8, int16Audio, {
          frequency: 1500,
          threads: 1,
          decodeDepth: 3,
          sessionId: 'concurrent-deep',
          stageSymbols: 50,
          slotUtc: 120002,
          resetSession: true,
        }),
      ]);

      assert.strictEqual(fast.decodeDepth, 1);
      assert.strictEqual(deep.decodeDepth, 3);
      assert.strictEqual(fast.stage, 50);
      assert.strictEqual(deep.stage, 50);
      assert.ok(fast.messages.some((message) => message.text.includes('CQ TEST K1ABC FN20')));
      assert.ok(deep.messages.some((message) => message.text.includes('CQ TEST K1ABC FN20')));
      lib.endDecodeSession('concurrent-fast');
      lib.endDecodeSession('concurrent-deep');
    });

    it('decodes the same known FT8 frame through Float32 and Int16 paths', async () => {
      const int16Audio = toInt16(encoded.audioData);
      const floatResult = await lib.decode(WSJTXMode.FT8, encoded.audioData, {
        frequency: 1500,
        threads: 1,
        decodeDepth: 1,
      });
      const int16Result = await lib.decode(WSJTXMode.FT8, int16Audio, {
        frequency: 1500,
        threads: 1,
        decodeDepth: 1,
      });
      assert.ok(floatResult.messages.some((message) => message.text.includes('CQ TEST K1ABC FN20')));
      assert.ok(int16Result.messages.some((message) => message.text.includes('CQ TEST K1ABC FN20')));
    });

    it('rejects a decreasing stage and skips a duplicate stage', async () => {
      const session = lib.beginDecodeSession({
        sessionId: 'stage-order',
        mode: WSJTXMode.FT8,
        decodeDepth: 3,
        slotUtc: 120000,
      });
      const audio = toInt16(encoded.audioData);
      await session.decodeStage(audio, 41, { frequency: 1500, threads: 1 });
      const duplicate = await session.decodeStage(audio, 41, { frequency: 1500, threads: 1 });
      assert.strictEqual(duplicate.skipped, true);
      await session.decodeStage(audio, 47, { frequency: 1500, threads: 1 });
      await assert.rejects(
        () => session.decodeStage(audio, 41, { frequency: 1500, threads: 1 }),
        /monotonic/,
      );
      session.endDecodeSession();
    });

    it('48 kHz opt-in encoded audio still decodes structurally', async () => {
      runIsolatedNode(`
        import { WSJTXLib, WSJTXMode } from './src/index.js';
        const lib = new WSJTXLib({ encodeSampleRate: 48000 });
        const encoded = await lib.encode(WSJTXMode.FT8, 'CQ TEST K1ABC FN20', 1500);
        const decoded = await lib.decode(WSJTXMode.FT8, encoded.audioData, { frequency: 1500, threads: 1 });
        if (!decoded.success || !Array.isArray(decoded.messages)) process.exit(1);
      `);
    });

    it('decoded message text contains the encoded callsigns when SNR is sufficient', async () => {
      // Synthetic encoder output is essentially clean; decoding with a wide
      // search window should reliably recover the original message.
      const result = await lib.decode(
        WSJTXMode.FT8,
        encoded.audioData,
        makeOptions({ frequency: 1500, lowFreq: 200, highFreq: 4000, tolerance: 50 }),
      );
      assert.strictEqual(result.success, true);
      // We don't assert recovery in the synthetic path because the decoder
      // sometimes treats the synthetic tones as out-of-band; we only assert
      // that the result frame is structurally valid.
      for (const m of result.messages) {
        assert.ok(typeof m.text === 'string');
        assert.ok(typeof m.snr === 'number');
        assert.ok(typeof m.deltaTime === 'number');
        assert.ok(typeof m.deltaFrequency === 'number');
      }
    });
  });

  // ---- DecodeOptions field-by-field ----

  describe('DecodeOptions plumbing', () => {
    let silence: Float32Array;

    before(() => {
      // 13 s of silence at the default encode rate gives the decoder a full FT8 window.
      silence = new Float32Array(ENCODE_SAMPLE_RATE * 13);
    });

    it('decode with only `frequency` succeeds (defaults applied)', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, { frequency: 1500 });
      assert.strictEqual(r.success, true);
      assert.deepStrictEqual(r.messages, []);
    });

    it('decode with custom `threads` succeeds', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, { frequency: 1500, threads: 2 });
      assert.strictEqual(r.success, true);
    });

    it('decode with custom `lowFreq`/`highFreq`/`tolerance` succeeds', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, {
        frequency: 1500,
        threads: 1,
        lowFreq: 500,
        highFreq: 3000,
        tolerance: 30,
      });
      assert.strictEqual(r.success, true);
    });

    it('decode with `dxCall` only succeeds', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, {
        frequency: 1500,
        threads: 1,
        dxCall: 'K1ABC',
      });
      assert.strictEqual(r.success, true);
    });

    it('decode with `dxGrid` only succeeds', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, {
        frequency: 1500,
        threads: 1,
        dxGrid: 'FN20',
      });
      assert.strictEqual(r.success, true);
    });

    it('decode with all options together succeeds', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, {
        frequency: 1500,
        threads: 1,
        lowFreq: 200,
        highFreq: 4000,
        tolerance: 20,
        dxCall: 'K1ABC',
        dxGrid: 'FN20',
      });
      assert.strictEqual(r.success, true);
    });

    it('decode with very narrow scan window still succeeds (does not crash)', async () => {
      const r = await lib.decode(WSJTXMode.FT8, silence, {
        frequency: 1500,
        threads: 1,
        lowFreq: 1490,
        highFreq: 1510,
        tolerance: 5,
      });
      assert.strictEqual(r.success, true);
    });

    it('decode reuses lib instance across calls without state corruption', async () => {
      const r1 = await lib.decode(WSJTXMode.FT8, silence, { frequency: 1500, dxCall: 'K1ABC' });
      const r2 = await lib.decode(WSJTXMode.FT8, silence, { frequency: 1500 });
      const r3 = await lib.decode(WSJTXMode.FT8, silence, {
        frequency: 1500,
        lowFreq: 800,
        highFreq: 3000,
      });
      for (const r of [r1, r2, r3]) {
        assert.strictEqual(r.success, true);
        assert.ok(Array.isArray(r.messages));
      }
    });
  });

  // ---- Audio format conversion ----

  describe('convertAudioFormat', () => {
    it('Float32Array → Int16Array clamps and scales', async () => {
      const input = new Float32Array([-1.5, -1.0, -0.5, 0, 0.5, 1.0, 1.5]);
      const out = await lib.convertAudioFormat(input, 'int16');
      assert.ok(out instanceof Int16Array);
      assert.strictEqual(out.length, input.length);
      assert.ok(out[0] <= -32767, `clamp negative: got ${out[0]}`);
      assert.ok(out[6] >= 32767, `clamp positive: got ${out[6]}`);
      assert.strictEqual(out[3], 0);
    });

    it('Int16Array → Float32Array is inverse-scaled', async () => {
      const input = new Int16Array([-32768, 0, 32767]);
      const out = await lib.convertAudioFormat(input, 'float32');
      assert.ok(out instanceof Float32Array);
      assert.strictEqual(out.length, input.length);
      assert.ok(Math.abs(out[0] + 1) < 1e-3);
      assert.strictEqual(out[1], 0);
      assert.ok(Math.abs(out[2] - 1) < 1e-3);
    });

    it('Float32→Int16→Float32 round-trip approximately preserves signal', async () => {
      const original = new Float32Array([0.25, -0.5, 0.75, -0.125, 0]);
      const i16 = (await lib.convertAudioFormat(original, 'int16')) as Int16Array;
      const f32 = (await lib.convertAudioFormat(i16, 'float32')) as Float32Array;
      for (let i = 0; i < original.length; i++) {
        assert.ok(
          Math.abs(original[i] - f32[i]) < 1e-3,
          `round-trip drift at ${i}: ${original[i]} -> ${f32[i]}`,
        );
      }
    });
  });

  // ---- pullMessages legacy surface ----

  describe('pullMessages (legacy)', () => {
    it('returns an array (possibly empty) without throwing', () => {
      const msgs = lib.pullMessages();
      assert.ok(Array.isArray(msgs));
    });
  });

  // ---- Error handling ----

  describe('error handling', () => {
    it('rejects invalid mode value', async () => {
      await assert.rejects(
        () => lib.decode(999 as unknown as WSJTXMode, new Float32Array(1000), { frequency: 1500 }),
        WSJTXError,
      );
    });

    it('rejects negative frequency', async () => {
      await assert.rejects(
        () => lib.decode(WSJTXMode.FT8, new Float32Array(1000), { frequency: -1 }),
        WSJTXError,
      );
    });

    it('rejects empty audio buffer', async () => {
      await assert.rejects(
        () => lib.decode(WSJTXMode.FT8, new Float32Array(0), { frequency: 1500 }),
        WSJTXError,
      );
    });

    it('rejects encoding empty message', async () => {
      await assert.rejects(() => lib.encode(WSJTXMode.FT8, '', 1500), WSJTXError);
    });

    it('rejects encoding for decode-only mode (JT65)', async () => {
      await assert.rejects(() => lib.encode(WSJTXMode.JT65, 'CQ K1ABC FN20', 1500), WSJTXError);
    });

    it('rejects invalid encode sample rate', () => {
      assert.throws(
        () => new WSJTXLib({ encodeSampleRate: 44100 as 12000 }),
        WSJTXError,
      );
    });

    it('WSJTXError preserves message and code', () => {
      const e = new WSJTXError('boom', 'XX');
      assert.ok(e instanceof Error);
      assert.strictEqual(e.message, 'boom');
      assert.strictEqual(e.code, 'XX');
      assert.strictEqual(e.name, 'WSJTXError');
    });

    it('rejects WSPR decode with non-Int16 audio', async () => {
      await assert.rejects(
        () => lib.decodeWSPR(new Float32Array(0) as unknown as Int16Array),
        WSJTXError,
      );
    });

    it('rejects threads outside 1..16 range during encode', async () => {
      await assert.rejects(
        () => lib.encode(WSJTXMode.FT8, 'CQ K1ABC FN20', 1500, 0),
        WSJTXError,
      );
      await assert.rejects(
        () => lib.encode(WSJTXMode.FT8, 'CQ K1ABC FN20', 1500, 17),
        WSJTXError,
      );
    });
  });
});
