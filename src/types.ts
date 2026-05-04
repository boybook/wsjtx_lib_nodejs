export enum WSJTXMode { FT8=0, FT4=1, JT4=2, JT65=3, JT9=4, FST4=5, Q65=6, FST4W=7, JT65JT9=8, WSPR=9 }
export type AudioData = Float32Array | Int16Array;
export interface WSJTXTime { hour: number; minute: number; second: number; }
export interface WSJTXMessage { text: string; snr: number; deltaTime: number; deltaFrequency: number; timestamp: number; sync: number; }
export interface DecodeOptions { frequency: number; threads?: number; dxCall?: string; dxGrid?: string; lowFreq?: number; highFreq?: number; tolerance?: number; }
export interface DecodeResult { success: boolean; messages: WSJTXMessage[]; error?: string; }
export interface EncodeResult { audioData: Float32Array; messageSent: string; }
export interface WSPRResult { frequency: number; sync: number; snr: number; deltaTime: number; drift: number; jitter: number; message: string; callsign: string; locator: string; power: string; cycles: number; }
export interface WSPRDecodeOptions { dialFrequency?: number; callsign?: string; locator?: string; quickMode?: boolean; useHashTable?: boolean; passes?: number; subtraction?: boolean; }
export class WSJTXError extends Error { constructor(message: string, public code?: string) { super(message); this.name = 'WSJTXError'; } }
export interface WSJTXConfig { maxThreads?: number; debug?: boolean; defaultLowFreq?: number; defaultHighFreq?: number; defaultTolerance?: number; }
export interface VersionInfo { wrapperVersion: string; libraryVersion: string; nodeVersion: string; buildDate: string; }
export interface ModeCapabilities { mode: WSJTXMode; encodingSupported: boolean; decodingSupported: boolean; sampleRate: number; duration: number; }
export type DecodeCallback = (error: Error|null, result: DecodeResult) => void;
export type EncodeCallback = (error: Error|null, result: EncodeResult) => void;
export type WSPRDecodeCallback = (error: Error|null, results: WSPRResult[]) => void;
