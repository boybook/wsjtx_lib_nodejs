/**
 * Public types and enums for the wsjtx-lib Node.js binding.
 */

export enum WSJTXMode {
  FT8 = 0,
  FT4 = 1,
  JT4 = 2,
  JT65 = 3,
  JT9 = 4,
  FST4 = 5,
  Q65 = 6,
  FST4W = 7,
  JT65JT9 = 8,
  WSPR = 9,
}

export type AudioData = Float32Array | Int16Array;

export type WSJTXDecodeStage = 41 | 47 | 49 | 50 | 'ft4-partial' | 'ft4-final';

export interface WSJTXTime {
  hour: number;
  minute: number;
  second: number;
}

export interface WSJTXMessage {
  text: string;
  snr: number;
  deltaTime: number;
  deltaFrequency: number;
  /** seconds-of-day reported by the decoder (hh*3600 + mm*60 + ss) */
  timestamp: number;
  sync: number;
}

/**
 * Options accepted by `WSJTXLib.decode`.
 *
 * - frequency: nominal QSO frequency in Hz (decoder uses this as nfqso).
 * - txFrequency: transmit audio offset in Hz (decoder uses this as nftx).
 * - threads:   thread hint forwarded to the decoder. Defaults to maxThreads.
 * - myCall / myGrid / dxCall / dxGrid: AP decode context for the named station.
 * - lowFreq / highFreq / tolerance: scan window and tone tolerance in Hz
 *   (defaults: 200 / 4000 / 20). These are forwarded to the decoder via
 *   `setDecodeRange` and *do* take effect.
 * - apDecode: enables FT8/FT4 AP decode passes. Defaults to true.
 * - decodeDepth: WSJT-X decoder depth. Defaults to 1.
 * - qsoProgress: WSJT-X QSO progress stage. Defaults to 0.
 */
export interface DecodeOptions {
  frequency: number;
  txFrequency?: number;
  threads?: number;
  myCall?: string;
  myGrid?: string;
  dxCall?: string;
  dxGrid?: string;
  lowFreq?: number;
  highFreq?: number;
  tolerance?: number;
  apDecode?: boolean;
  decodeDepth?: number;
  qsoProgress?: number;
  /** Internal/session stage. Omit for the compatible one-shot final decode. */
  stageSymbols?: number;
  /** Session identifier used to preserve WSJT-X state across stages. */
  sessionId?: string;
  /** Slot UTC as HHMMSS, used instead of the process clock for staged decode. */
  slotUtc?: number;
  /** Force a fresh stage state when starting a session. */
  resetSession?: boolean;
  /** WSJT-X targeted re-decode mode. */
  nagain?: boolean;
  /** EME delay compensation in milliseconds. */
  emeDelayMs?: number;
}

export interface DecodeResult {
  success: boolean;
  messages: WSJTXMessage[];
  error?: string;
  processingTimeMs?: number;
  stage?: WSJTXDecodeStage;
  decodeDepth?: number;
  stats?: DecodeStats;
}

export interface DecodeStats {
  stageSymbols: number;
  candidateCount: number;
  decodedCount: number;
  averageCount: number;
}

export interface DecodeSessionOptions {
  sessionId: string;
  mode: WSJTXMode;
  slotUtc?: number;
  decodeDepth: 1 | 2 | 3;
}

export interface DecodeStageResult extends DecodeResult {
  stage: WSJTXDecodeStage;
  newMessages: WSJTXMessage[];
  allMessages: WSJTXMessage[];
  skipped?: boolean;
  skipReason?: 'duplicate-stage';
}

export interface DecodeSessionSummary {
  sessionId: string;
  mode: WSJTXMode;
  decodeDepth: 1 | 2 | 3;
  messages: WSJTXMessage[];
  stages: WSJTXDecodeStage[];
}

export interface EncodeResult {
  audioData: Float32Array;
  messageSent: string;
  sampleRate: number;
}

export interface WSPRResult {
  frequency: number;
  sync: number;
  snr: number;
  deltaTime: number;
  drift: number;
  jitter: number;
  message: string;
  callsign: string;
  locator: string;
  power: string;
  cycles: number;
}

export interface WSPRDecodeOptions {
  dialFrequency?: number;
  callsign?: string;
  locator?: string;
  quickMode?: boolean;
  useHashTable?: boolean;
  passes?: number;
  subtraction?: boolean;
}

export class WSJTXError extends Error {
  constructor(message: string, public code?: string) {
    super(message);
    this.name = 'WSJTXError';
  }
}

export interface WSJTXConfig {
  /** Maximum threads used per decode call. Default 4. */
  maxThreads?: number;
  /** Process-global FT8/FT4 encode output sample rate. Default 12000. */
  encodeSampleRate?: 12000 | 48000;
  /** Reserved for future use; currently has no runtime effect. */
  debug?: boolean;
  /** Default lower scan limit in Hz, used when DecodeOptions.lowFreq is omitted. */
  defaultLowFreq?: number;
  /** Default upper scan limit in Hz, used when DecodeOptions.highFreq is omitted. */
  defaultHighFreq?: number;
  /** Default tone tolerance in Hz, used when DecodeOptions.tolerance is omitted. */
  defaultTolerance?: number;
}

export interface VersionInfo {
  wrapperVersion: string;
  libraryVersion: string;
  nodeVersion: string;
  buildDate: string;
}

export interface ModeCapabilities {
  mode: WSJTXMode;
  encodingSupported: boolean;
  decodingSupported: boolean;
  sampleRate: number;
  duration: number;
}

export type DecodeCallback = (error: Error | null, result: DecodeResult) => void;
export type EncodeCallback = (error: Error | null, result: EncodeResult) => void;
export type WSPRDecodeCallback = (error: Error | null, results: WSPRResult[]) => void;
