/**
 * wsjtx-lib — Node.js binding for the WSJT-X 3.0.2 backend.
 *
 * Public surface:
 *   - WSJTXLib.encode(mode, message, frequency)
 *   - WSJTXLib.decode(mode, audio, options)
 *   - WSJTXLib.decodeWSPR(audio, options)
 *   - WSJTXLib.convertAudioFormat(audio, target)
 *   - capability/sample-rate query helpers (FT8/FT4 default encode rate: 12 kHz)
 */

import {
  WSJTXMode,
  type DecodeResult,
  type EncodeResult,
  type WSPRResult,
  type WSPRDecodeOptions,
  type WSJTXMessage,
  type AudioData,
  WSJTXError,
  type WSJTXConfig,
  type ModeCapabilities,
  type DecodeOptions,
  type DecodeSessionOptions,
  type DecodeStageResult,
  type DecodeSessionSummary,
  type WSJTXDecodeStage,
} from './types.js';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const __dirname = path.dirname(fileURLToPath(import.meta.url));

interface NativeBinding {
  WSJTXLib: new (config?: { encodeSampleRate?: number }) => NativeWSJTXLib;
}

interface NativeDecodeOptions {
  frequency: number;
  txFrequency: number;
  threads: number;
  lowFreq: number;
  highFreq: number;
  tolerance: number;
  myCall: string;
  myGrid: string;
  dxCall: string;
  dxGrid: string;
  apDecode: boolean;
  decodeDepth: number;
  qsoProgress: number;
  stageSymbols?: number;
  slotUtc?: number;
  resetSession?: boolean;
  nagain?: boolean;
  emeDelayMs?: number;
  sessionId?: string;
}

interface NativeWSJTXLib {
  decode(mode: number, audio: AudioData, opts: NativeDecodeOptions, cb: (e: Error | null, r: DecodeResult) => void): void;
  encode(mode: number, message: string, frequency: number, threads: number, cb: (e: Error | null, r: EncodeResult) => void): void;
  decodeWSPR(audio: Float32Array, opts: Record<string, unknown>, cb: (e: Error | null, r: WSPRResult[]) => void): void;
  pullMessages(): WSJTXMessage[];
  isEncodingSupported(mode: number): boolean;
  isDecodingSupported(mode: number): boolean;
  getSampleRate(mode: number): number;
  getTransmissionDuration(mode: number): number;
  convertAudioFormat(audio: AudioData, target: 'float32' | 'int16', cb: (e: Error | null, r: AudioData) => void): void;
  endDecodeSession(sessionId: string): boolean;
}

function loadNativeBinding(): NativeBinding['WSJTXLib'] {
  const binding = require('node-gyp-build')(path.resolve(__dirname, '..', '..')) as NativeBinding;
  return binding.WSJTXLib;
}

const NativeWSJTXLib = loadNativeBinding();

const DEFAULT_CONFIG: Required<WSJTXConfig> = {
  maxThreads: 4,
  encodeSampleRate: 12000,
  debug: false,
  defaultLowFreq: 200,
  defaultHighFreq: 4000,
  defaultTolerance: 20,
};

const FREQ_MIN = 0;
const FREQ_MAX = 30_000_000;
const THREADS_MIN = 1;
const THREADS_MAX = 16;
const MESSAGE_MAX_LEN = 37;

export class WSJTXLib {
  private readonly native: NativeWSJTXLib;
  private readonly config: Required<WSJTXConfig>;

  constructor(config: WSJTXConfig = {}) {
    this.config = { ...DEFAULT_CONFIG, ...config };
    this.validateEncodeSampleRate(this.config.encodeSampleRate);
    this.native = new NativeWSJTXLib({ encodeSampleRate: this.config.encodeSampleRate });
  }

  async decode(mode: WSJTXMode, audioData: AudioData, options: DecodeOptions): Promise<DecodeResult> {
    this.validateMode(mode);
    this.validateAudio(audioData);
    this.validateFrequency(options.frequency);
    if (options.decodeDepth !== undefined
      && (!Number.isInteger(options.decodeDepth) || options.decodeDepth < 1 || options.decodeDepth > 3)) {
      throw new WSJTXError('decodeDepth must be 1, 2, or 3', 'INVALID');
    }
    if (options.stageSymbols !== undefined && ![41, 47, 49, 50].includes(options.stageSymbols)) {
      throw new WSJTXError('stageSymbols must be 41, 47, 49, or 50', 'INVALID');
    }
    if (!this.isDecodingSupported(mode)) {
      throw new WSJTXError('Decoding not supported for this mode', 'UNSUPPORTED');
    }

    const opts: NativeDecodeOptions = {
      frequency: options.frequency,
      txFrequency: options.txFrequency ?? options.frequency,
      threads: options.threads ?? this.config.maxThreads,
      lowFreq: options.lowFreq ?? this.config.defaultLowFreq,
      highFreq: options.highFreq ?? this.config.defaultHighFreq,
      tolerance: options.tolerance ?? this.config.defaultTolerance,
      myCall: options.myCall ?? '',
      myGrid: options.myGrid ?? '',
      dxCall: options.dxCall ?? '',
      dxGrid: options.dxGrid ?? '',
      apDecode: options.apDecode ?? true,
      decodeDepth: options.decodeDepth ?? 1,
      qsoProgress: options.qsoProgress ?? 0,
      ...(options.stageSymbols !== undefined ? { stageSymbols: options.stageSymbols } : {}),
      ...(options.slotUtc !== undefined ? { slotUtc: options.slotUtc } : {}),
      ...(options.resetSession !== undefined ? { resetSession: options.resetSession } : {}),
      ...(options.nagain !== undefined ? { nagain: options.nagain } : {}),
      ...(options.emeDelayMs !== undefined ? { emeDelayMs: options.emeDelayMs } : {}),
      ...(options.sessionId !== undefined ? { sessionId: options.sessionId } : {}),
    };

    return new Promise((resolve, reject) => {
      this.native.decode(mode, audioData, opts, (err, result) => {
        if (err) reject(new WSJTXError(err.message, 'DECODE_ERROR'));
        else resolve({
          ...result,
          ...(options.stageSymbols !== undefined ? { stage: options.stageSymbols as WSJTXDecodeStage } : {}),
          ...(options.decodeDepth !== undefined ? { decodeDepth: options.decodeDepth } : {}),
        });
      });
    });
  }

  beginDecodeSession(options: DecodeSessionOptions): WSJTXDecodeSession {
    this.validateMode(options.mode);
    if (options.mode !== WSJTXMode.FT8 && options.mode !== WSJTXMode.FT4) {
      throw new WSJTXError('Staged decode is supported for FT8 and FT4 only', 'UNSUPPORTED');
    }
    if (!Number.isInteger(options.decodeDepth) || options.decodeDepth < 1 || options.decodeDepth > 3) {
      throw new WSJTXError('decodeDepth must be 1, 2, or 3', 'INVALID');
    }
    if (!options.sessionId || options.sessionId.length > 63) {
      throw new WSJTXError('sessionId must be 1..63 characters', 'INVALID');
    }
    return new WSJTXDecodeSession(this, options);
  }

  endDecodeSession(sessionId: string): void {
    if (!this.native.endDecodeSession(sessionId)) {
      throw new WSJTXError('Failed to end decode session', 'DECODE_ERROR');
    }
  }

  async encode(
    mode: WSJTXMode,
    message: string,
    frequency: number,
    threads: number = this.config.maxThreads,
  ): Promise<EncodeResult> {
    this.validateMode(mode);
    this.validateMessage(message);
    this.validateFrequency(frequency);
    this.validateThreads(threads);
    if (!this.isEncodingSupported(mode)) {
      throw new WSJTXError('Encoding not supported for this mode', 'UNSUPPORTED');
    }

    return new Promise((resolve, reject) => {
      this.native.encode(mode, message, frequency, threads, (err, result) => {
        if (err) reject(new WSJTXError(err.message, 'ENCODE_ERROR'));
        else resolve(result);
      });
    });
  }

  async decodeWSPR(audioData: Int16Array, options: WSPRDecodeOptions = {}): Promise<WSPRResult[]> {
    if (!(audioData instanceof Int16Array) || audioData.length === 0) {
      throw new WSJTXError('audioData must be a non-empty Int16Array', 'INVALID');
    }

    const opts = {
      dialFrequency: 14_095_600,
      callsign: '',
      locator: '',
      quickMode: false,
      useHashTable: true,
      passes: 2,
      subtraction: true,
      ...options,
    };

    return new Promise((resolve, reject) => {
      this.native.decodeWSPR(audioData as unknown as Float32Array, opts, (err, results) => {
        if (err) reject(new WSJTXError(err.message, 'WSPR_ERROR'));
        else resolve(results);
      });
    });
  }

  pullMessages(): WSJTXMessage[] {
    return this.native.pullMessages();
  }

  isEncodingSupported(mode: WSJTXMode): boolean {
    return this.native.isEncodingSupported(mode);
  }

  isDecodingSupported(mode: WSJTXMode): boolean {
    return this.native.isDecodingSupported(mode);
  }

  getSampleRate(mode: WSJTXMode): number {
    return this.native.getSampleRate(mode);
  }

  getTransmissionDuration(mode: WSJTXMode): number {
    return this.native.getTransmissionDuration(mode);
  }

  getAllModeCapabilities(): ModeCapabilities[] {
    const numericModes = Object.values(WSJTXMode).filter((v): v is number => typeof v === 'number');
    return numericModes.map((mode) => ({
      mode: mode as WSJTXMode,
      encodingSupported: this.isEncodingSupported(mode as WSJTXMode),
      decodingSupported: this.isDecodingSupported(mode as WSJTXMode),
      sampleRate: this.getSampleRate(mode as WSJTXMode),
      duration: this.getTransmissionDuration(mode as WSJTXMode),
    }));
  }

  async convertAudioFormat(audioData: AudioData, targetFormat: 'float32' | 'int16'): Promise<AudioData> {
    return new Promise((resolve, reject) => {
      this.native.convertAudioFormat(audioData, targetFormat, (err, result) => {
        if (err) reject(err);
        else resolve(result);
      });
    });
  }

  private validateMode(mode: WSJTXMode): void {
    if (!Object.values(WSJTXMode).includes(mode)) {
      throw new WSJTXError('Invalid mode', 'INVALID');
    }
  }

  private validateFrequency(freq: number): void {
    if (!Number.isInteger(freq) || freq < FREQ_MIN || freq > FREQ_MAX) {
      throw new WSJTXError('Invalid frequency', 'INVALID');
    }
  }

  private validateEncodeSampleRate(sampleRate: number): void {
    if (sampleRate !== 12000 && sampleRate !== 48000) {
      throw new WSJTXError('encodeSampleRate must be 12000 or 48000', 'INVALID');
    }
  }

  private validateThreads(threads: number): void {
    if (!Number.isInteger(threads) || threads < THREADS_MIN || threads > THREADS_MAX) {
      throw new WSJTXError(`Threads must be ${THREADS_MIN}..${THREADS_MAX}`, 'INVALID');
    }
  }

  private validateMessage(message: string): void {
    if (typeof message !== 'string' || message.length === 0 || message.length > MESSAGE_MAX_LEN) {
      throw new WSJTXError(`Message must be 1..${MESSAGE_MAX_LEN} characters`, 'INVALID');
    }
  }

  private validateAudio(audio: AudioData): void {
    const isTyped = audio instanceof Float32Array || audio instanceof Int16Array;
    if (!isTyped || audio.length === 0) {
      throw new WSJTXError('audioData must be a non-empty Float32Array or Int16Array', 'INVALID');
    }
  }
}

export class WSJTXDecodeSession {
  private ended = false;
  private firstStage = true;
  private readonly stages: WSJTXDecodeStage[] = [];
  private readonly messages: WSJTXMessage[] = [];
  private readonly messageKeys = new Set<string>();

  constructor(private readonly owner: WSJTXLib, private readonly options: DecodeSessionOptions) {}

  /** Returns true when a scheduler retry repeats the latest numeric stage. */
  isStageDuplicate(stage: WSJTXDecodeStage): boolean {
    const previous = this.stages.at(-1);
    return typeof stage === 'number' && typeof previous === 'number' && stage === previous;
  }

  async decodeStage(
    audioData: AudioData,
    stage: WSJTXDecodeStage,
    options: Partial<Pick<DecodeOptions, 'frequency'>> & Omit<DecodeOptions, 'frequency' | 'sessionId' | 'stageSymbols' | 'slotUtc' | 'decodeDepth'> = {},
  ): Promise<DecodeStageResult> {
    if (this.ended) throw new WSJTXError('Decode session has ended', 'INVALID');
    const stageSymbols = typeof stage === 'number' ? stage : 50;
    if (this.options.mode === WSJTXMode.FT8 && ![41, 47, 49, 50].includes(stageSymbols)) {
      throw new WSJTXError('FT8 stage must be 41, 47, 49, or 50', 'INVALID');
    }
    const previousStage = this.stages.at(-1);
    if (typeof stage === 'number' && typeof previousStage === 'number') {
      if (stage < previousStage) throw new WSJTXError('Decode stages must be monotonic', 'INVALID');
      if (stage === previousStage) {
        return {
          success: true,
          messages: [],
          stage,
          decodeDepth: this.options.decodeDepth,
          newMessages: [],
          allMessages: [...this.messages],
          skipped: true,
          skipReason: 'duplicate-stage',
        };
      }
    }
    const decodeOptions: DecodeOptions = {
      ...options,
      frequency: options.frequency ?? 0,
      decodeDepth: this.options.decodeDepth,
      sessionId: this.options.sessionId,
      stageSymbols,
      resetSession: this.firstStage,
    };
    if (this.options.slotUtc !== undefined) decodeOptions.slotUtc = this.options.slotUtc;
    const result = await this.owner.decode(this.options.mode, audioData, decodeOptions);
    this.firstStage = false;
    this.stages.push(stage);
    const newMessages = result.messages.filter((message) => {
      const key = `${message.text.trim()}|${message.deltaFrequency}|${message.deltaTime.toFixed(3)}`;
      if (this.messageKeys.has(key)) return false;
      this.messageKeys.add(key);
      this.messages.push(message);
      return true;
    });
    return { ...result, stage, newMessages, allMessages: [...this.messages] };
  }

  endDecodeSession(): DecodeSessionSummary {
    if (!this.ended) {
      this.owner.endDecodeSession(this.options.sessionId);
      this.ended = true;
    }
    return {
      sessionId: this.options.sessionId,
      mode: this.options.mode,
      decodeDepth: this.options.decodeDepth,
      messages: [...this.messages],
      stages: [...this.stages],
    };
  }
}

export { WSJTXMode, WSJTXError };
export type {
  DecodeResult,
  EncodeResult,
  WSPRResult,
  WSPRDecodeOptions,
  WSJTXMessage,
  AudioData,
  WSJTXConfig,
  DecodeOptions,
  DecodeSessionOptions,
  DecodeStageResult,
  DecodeSessionSummary,
  WSJTXDecodeStage,
  ModeCapabilities,
};
