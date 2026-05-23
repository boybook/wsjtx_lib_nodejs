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

export type Q65Period = 30 | 60 | 120 | 300;
export type Q65Submode = 'A' | 'B' | 'C' | 'D' | 'E' | 0 | 1 | 2 | 3 | 4;

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

export interface Q65EncodeOptions {
  /** Q65 transmit/receive period in seconds. Defaults to 60. */
  q65Period?: Q65Period;
  /** Q65 submode A-E, or 0-4. Defaults to A / 0. */
  q65Submode?: Q65Submode;
}

export interface EncodeOptions extends Q65EncodeOptions {
  /** Worker thread hint. Defaults to WSJTXConfig.maxThreads. */
  threads?: number;
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
 * - q65Period / q65Submode: Q65-specific period and submode, also accepted
 *   by encode options so Q65 TX/RX can be configured symmetrically.
 * - q65MaxDrift: Q65 max drift control forwarded to the WSJT-X decoder.
 * - q65ClearAveraging: clear Q65 averaged-message state before decode.
 * - q65SingleDecode: request Q65 single-candidate decode behavior.
 * - q65Averaging: enable Q65 averaged decode passes.
 */
export interface DecodeOptions extends Q65EncodeOptions {
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
  q65MaxDrift?: number;
  q65ClearAveraging?: boolean;
  q65SingleDecode?: boolean;
  q65Averaging?: boolean;
}

export interface DecodeResult {
  success: boolean;
  messages: WSJTXMessage[];
  error?: string;
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
  /** Process-global FT8/FT4/Q65 encode output sample rate. Default 12000. */
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
