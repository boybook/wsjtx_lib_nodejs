# WSJTX Library for Node.js

[![npm version](https://badge.fury.io/js/wsjtx-lib.svg)](https://badge.fury.io/js/wsjtx-lib)

A high-performance Node.js C++ extension for digital amateur radio protocols, providing TypeScript support and async/await interfaces for WSJTX library functionality.

## Features

- 🚀 **High Performance**: Native C++ implementation with multi-threading support
- 📡 **Multiple Modes**: Support for FT8, FT4, JT4, JT65, JT9, FST4, Q65, FST4W, and WSPR
- 🔧 **TypeScript Support**: Full TypeScript definitions and modern ES modules
- ⚡ **Async/Await**: Promise-based API for non-blocking operations
- 🎵 **Audio Processing**: Support for both Float32Array and Int16Array audio formats
- 🌍 **Cross-Platform**: Prebuilt binaries for Windows, macOS, and Linux
- 📊 **WSPR Decoding**: Specialized support for WSPR IQ data processing

## Supported Modes

| Mode | Encoding | Decoding | Sample Rate | Duration | Bandwidth |
|------|----------|----------|-------------|----------|-----------|
| FT8  | ✅       | ✅       | 48 kHz      | 12.6s    | ~50 Hz    |
| FT4  | ✅       | ✅       | 48 kHz      | 6.0s     | ~80 Hz    |
| JT4  | ❌       | ✅       | 11.025 kHz  | 47.1s    | Variable  |
| JT65 | ❌       | ✅       | 11.025 kHz  | 46.8s    | ~180 Hz   |
| JT9  | ❌       | ✅       | 12 kHz      | 49.0s    | ~16 Hz    |
| FST4 | ❌       | ✅       | 12 kHz      | 60.0s    | Variable  |
| Q65  | ❌       | ✅       | 12 kHz      | 60.0s    | Variable  |
| FST4W| ❌       | ✅       | 12 kHz      | 120.0s   | Variable  |
| WSPR | ❌       | ✅       | 12 kHz      | 110.6s   | ~6 Hz     |

## Installation

### NPM Installation (Recommended)

The package includes prebuilt binaries for major platforms:

```bash
npm install wsjtx-lib
```

**Supported platforms with prebuilt binaries:**
- Linux x64
- macOS ARM64 (Apple Silicon)
- Windows x64

### Building from Source

Only needed if prebuilt binaries are not available for your platform.

#### Prerequisites

- Node.js 16+ 
- CMake 3.15+
- C++ compiler with C++17 support
- FFTW3 library (single precision)
- Boost libraries
- Fortran compiler (gfortran)

#### macOS

```bash
# Install dependencies using Homebrew
brew install cmake fftw boost gcc pkg-config

# Clone and build
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs
npm install
npm run build
```

#### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y \
  cmake \
  build-essential \
  gfortran \
  libfftw3-dev \
  libboost-all-dev \
  pkg-config

# Clone and build
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs
npm install
npm run build
```

#### Windows

Use MSYS2/MinGW-w64 for best compatibility:

```bash
# Install MSYS2, then in MSYS2 MINGW64 terminal:
pacman -S --needed \
  base-devel \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-pkg-config \
  mingw-w64-x86_64-fftw \
  mingw-w64-x86_64-boost \
  mingw-w64-x86_64-gcc-fortran \
  mingw-w64-x86_64-nodejs

# Clone and build
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs
npm install
npm run build
```

## Quick Start

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';

async function example() {
    // Create library instance
    const lib = new WSJTXLib();
    
    // Encode an FT8 message
    const encodeResult = await lib.encode(
        WSJTXMode.FT8,
        'CQ DX BH1ABC OM88',
        1000  // Audio frequency in Hz (typically 500-3000 Hz)
    );
    
    console.log(`Generated ${encodeResult.audioData.length} audio samples`);
    console.log(`Message sent: "${encodeResult.messageSent}"`);
    
    // Decode audio data (example with proper resampling for FT8)
    const audioData = new Float32Array(48000 * 13); // 13 seconds at 48kHz
    // ... fill audioData with actual audio samples ...
    
    const decodeResult = await lib.decode(
        WSJTXMode.FT8,
        audioData,
        1000  // Same audio frequency used for encoding
    );
    
    // Get decoded messages
    const messages = lib.pullMessages();
    messages.forEach(msg => {
        console.log(`Decoded: "${msg.text}" (SNR: ${msg.snr} dB, ΔT: ${msg.deltaTime}s)`);
    });
}
```

## API Reference

### WSJTXLib Class

#### Constructor

```typescript
new WSJTXLib(config?: WSJTXConfig)
```

Creates a new WSJTX library instance.

**Parameters:**
- `config` (optional): Configuration options
  - `maxThreads`: Maximum number of threads (1-16, default: 4)
  - `debug`: Enable debug logging (default: false)

#### Methods

##### `decode(mode, audioData, frequency, threads?): Promise<DecodeResult>`

Decode digital radio signals from audio data.

**Parameters:**
- `mode`: WSJTXMode enum value
- `audioData`: Float32Array or Int16Array of audio samples
- `frequency`: Audio frequency in Hz (typically 500-3000 Hz)
- `threads`: Number of threads to use (optional, default: 4)

**Returns:** Promise resolving to DecodeResult with success status

**Note:** For optimal FT8 decoding, audio may need resampling. See examples for details.

##### `encode(mode, message, frequency, threads?): Promise<EncodeResult>`

Encode a message into audio waveform for transmission.

**Parameters:**
- `mode`: WSJTXMode enum value
- `message`: Message text to encode (1-22 characters)
- `frequency`: Audio frequency in Hz (typically 500-3000 Hz)
- `threads`: Number of threads to use (optional, default: 4)

**Returns:** Promise resolving to EncodeResult with audio data and actual message sent

##### `decodeWSPR(iqData, options?): Promise<WSPRResult[]>`

Decode WSPR signals from IQ data.

**Parameters:**
- `iqData`: Float32Array of interleaved I,Q samples
- `options`: WSPRDecodeOptions (optional)
  - `dialFrequency`: RF dial frequency in Hz (default: 14095600)
  - `callsign`: Station callsign
  - `locator`: Grid locator
  - `quickMode`: Enable quick decode mode (default: false)
  - `useHashTable`: Use hash table optimization (default: true)
  - `passes`: Number of decode passes (default: 2)
  - `subtraction`: Enable signal subtraction (default: true)

**Returns:** Promise resolving to array of WSPR decode results

##### `pullMessages(): WSJTXMessage[]`

Retrieve decoded messages from the internal queue.

**Returns:** Array of decoded messages

##### Utility Methods

- `isEncodingSupported(mode): boolean` - Check if encoding is supported for a mode
- `isDecodingSupported(mode): boolean` - Check if decoding is supported for a mode
- `getSampleRate(mode): number` - Get required sample rate for a mode
- `getTransmissionDuration(mode): number` - Get transmission duration for a mode
- `getAllModeCapabilities(): ModeCapabilities[]` - Get capabilities for all modes

##### Static Methods

- `convertAudioFormat(audioData, targetFormat): AudioData` - Convert between Float32Array and Int16Array

### Enums and Types

#### WSJTXMode

```typescript
enum WSJTXMode {
    FT8 = 0,
    FT4 = 1,
    JT4 = 2,
    JT65 = 3,
    JT9 = 4,
    FST4 = 5,
    Q65 = 6,
    FST4W = 7,
    WSPR = 8
}
```

#### WSJTXMessage

```typescript
interface WSJTXMessage {
    text: string;           // Decoded message text
    snr: number;            // Signal-to-noise ratio in dB
    deltaTime: number;      // Time offset in seconds
    deltaFrequency: number; // Frequency offset in Hz
}
```

#### EncodeResult

```typescript
interface EncodeResult {
    audioData: Float32Array;  // Generated audio waveform (48kHz sample rate)
    messageSent: string;      // Actual message encoded
}
```

#### WSPRResult

```typescript
interface WSPRResult {
    frequency: number;    // Signal frequency in Hz
    sync: number;         // Sync quality
    snr: number;          // Signal-to-noise ratio in dB
    deltaTime: number;    // Time offset in seconds
    drift: number;        // Frequency drift in Hz/minute
    jitter: number;       // Jitter metric
    message: string;      // Decoded message
    callsign: string;     // Decoded callsign
    locator: string;      // Decoded grid locator
    power: string;        // Decoded power in dBm
    cycles: number;       // Number of decode cycles
}
```

## Examples

### Complete FT8 Encode-Decode Cycle

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';
import * as fs from 'fs';
import * as wav from 'wav';

async function ft8Example() {
    const lib = new WSJTXLib();
    const message = 'CQ DX BH1ABC OM88';
    const audioFrequency = 1000;
    
    // 1. Encode message
    const encodeResult = await lib.encode(WSJTXMode.FT8, message, audioFrequency);
    console.log(`Encoded: "${encodeResult.messageSent}"`);
    
    // 2. Save as WAV file
    const audioInt16 = new Int16Array(encodeResult.audioData.length);
    for (let i = 0; i < encodeResult.audioData.length; i++) {
        audioInt16[i] = Math.round(encodeResult.audioData[i] * 32767);
    }
    
    const writer = new wav.FileWriter('ft8_test.wav', {
        channels: 1,
        sampleRate: lib.getSampleRate(WSJTXMode.FT8),
        bitDepth: 16
    });
    
    const buffer = Buffer.from(audioInt16.buffer);
    writer.write(buffer);
    writer.end();
    
    // 3. Read back and decode
    // Note: For optimal decode, you may need resampling
    const resampled = resampleTo12kHz(encodeResult.audioData);
    const audioForDecode = new Int16Array(resampled.length);
    for (let i = 0; i < resampled.length; i++) {
        audioForDecode[i] = Math.round(resampled[i] * 32767);
    }
    
    lib.pullMessages(); // Clear queue
    const decodeResult = await lib.decode(WSJTXMode.FT8, audioForDecode, audioFrequency);
    
    const messages = lib.pullMessages();
    console.log(`Decoded ${messages.length} messages`);
}

// Helper function for resampling (48kHz -> 12kHz)
function resampleTo12kHz(audioData48k: Float32Array): Float32Array {
    const audioData12k = new Float32Array(Math.floor(audioData48k.length / 4));
    for (let i = 0; i < audioData12k.length; i++) {
        audioData12k[i] = audioData48k[i * 4];
    }
    return audioData12k;
}
```

### WSPR Decoding

```typescript
import { WSJTXLib } from 'wsjtx-lib';

async function wsprExample() {
    const lib = new WSJTXLib();
    
    // IQ data (interleaved I,Q samples)
    const iqData = new Float32Array(2 * 12000 * 120); // 2 minutes of IQ data
    // ... fill with actual IQ data from SDR ...
    
    const options = {
        dialFrequency: 14095600,  // 20m WSPR frequency
        callsign: 'BH1ABC',
        locator: 'OM88',
        quickMode: false,
        passes: 2
    };
    
    const results = await lib.decodeWSPR(iqData, options);
    
    console.log('WSPR Decode Results:');
    results.forEach(result => {
        console.log(`${result.callsign} ${result.locator} ${result.power}dBm (SNR: ${result.snr}dB)`);
    });
}
```

### Audio Format Conversion

```typescript
import { WSJTXLib } from 'wsjtx-lib';

// Convert Float32Array to Int16Array
const floatData = new Float32Array([0.5, -0.5, 0.25, -0.25]);
const intData = WSJTXLib.convertAudioFormat(floatData, 'int16');
console.log(intData); // Int16Array [16384, -16384, 8192, -8192]

// Convert back to Float32Array
const backToFloat = WSJTXLib.convertAudioFormat(intData, 'float32');
console.log(backToFloat); // Float32Array [0.5, -0.5, 0.25, -0.25] (approximately)
```

### Multiple Message Types

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';

async function multipleMessages() {
    const lib = new WSJTXLib();
    const audioFrequency = 1000;
    
    const messages = [
        'CQ DX BH1ABC OM88',      // CQ call
        'BH1ABC BH2DEF +05',      // Signal report
        'BH2DEF BH1ABC R-12',     // Report acknowledgment
        'BH1ABC BH2DEF RRR',      // Received acknowledgment
        'BH2DEF BH1ABC 73'        // End contact
    ];
    
    for (const message of messages) {
        const result = await lib.encode(WSJTXMode.FT8, message, audioFrequency);
        console.log(`"${message}" -> "${result.messageSent}" (${result.audioData.length} samples)`);
    }
}
```

## Error Handling

The library throws `WSJTXError` for all operation failures:

```typescript
import { WSJTXError } from 'wsjtx-lib';

try {
    await lib.decode(WSJTXMode.FT8, audioData, 1000);
} catch (error) {
    if (error instanceof WSJTXError) {
        console.error(`WSJTX Error [${error.code}]: ${error.message}`);
        
        // Common error codes:
        // - INVALID_MODE: Invalid mode parameter
        // - INVALID_FREQUENCY: Invalid frequency parameter
        // - INVALID_AUDIO_DATA: Invalid audio data format/size
        // - INVALID_MESSAGE: Invalid message text
        // - DECODE_ERROR: Decoding operation failed
        // - ENCODE_ERROR: Encoding operation failed
    } else {
        console.error('Unexpected error:', error);
    }
}
```

## Important Notes

1. **Audio Frequency**: The `frequency` parameter is the audio tone frequency within your audio passband (typically 500-3000 Hz), not the RF frequency.

2. **Sample Rates**: Different modes require different sample rates. Use `lib.getSampleRate(mode)` to get the correct rate.

3. **Audio Resampling**: For optimal FT8 decoding, audio may need to be resampled from 48kHz to 12kHz. See examples for implementation.

4. **Thread Safety**: Each WSJTXLib instance should be used from a single thread. Create separate instances for concurrent operations.

5. **Message Queue**: The `pullMessages()` method clears the internal message queue. Call it regularly to avoid memory buildup.

## Building from Source (Advanced)

For detailed build instructions when prebuilt binaries are not available, see [BUILD.md](BUILD.md).

```bash
# Clone with submodules
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx_lib_nodejs

# Install dependencies
npm install

# Build native module and TypeScript
npm run build

# Run tests
npm test

# Run comprehensive tests
npm run test:full

# Run examples
node examples/examples.js
```

## Development

### Project Structure

```
wsjtx_lib_nodejs/
├── src/                 # TypeScript source files
├── native/              # C++ wrapper code
├── wsjtx_lib/          # Git submodule (wsjtx_lib library)
├── test/               # Test files
├── examples/           # Usage examples
├── dist/               # Compiled TypeScript output
├── prebuilds/          # Prebuilt binaries for distribution
└── build/              # CMake build directory
```

### Scripts

- `npm run build` - Build both native module and TypeScript
- `npm run build:native` - Build only the native C++ module
- `npm run build:ts` - Build only TypeScript
- `npm test` - Run basic tests (CI-friendly)
- `npm run test:full` - Run comprehensive tests
- `npm run clean` - Clean build artifacts
- `npm run package` - Package prebuilt binaries for distribution

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## License

This project is licensed under the GPL-3.0 License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the excellent [wsjtx_lib](https://github.com/paulh002/wsjtx_lib) library by PA0PHH
- WSJT-X development team for the original algorithms by K1JT
- Amateur radio community for protocol specifications