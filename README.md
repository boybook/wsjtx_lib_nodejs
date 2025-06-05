# WSJTX Library for Node.js

A high-performance Node.js C++ extension for digital amateur radio protocols, providing TypeScript support and async/await interfaces for WSJTX library functionality.

## Features

- 🚀 **High Performance**: Native C++ implementation with multi-threading support
- 📡 **Multiple Modes**: Support for FT8, FT4, JT4, JT65, JT9, FST4, Q65, FST4W, and WSPR
- 🔧 **TypeScript Support**: Full TypeScript definitions and modern ES modules
- ⚡ **Async/Await**: Promise-based API for non-blocking operations
- 🎵 **Audio Processing**: Support for both Float32Array and Int16Array audio formats
- 🌍 **Cross-Platform**: Works on Windows, macOS, and Linux
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

### Prerequisites

- Node.js 16+ 
- CMake 3.15+
- C++ compiler with C++17 support
- FFTW3 library
- Boost libraries
- Fortran compiler (gfortran)

### macOS

```bash
# Install dependencies using Homebrew
brew install cmake fftw boost gcc

# Clone and install
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx-lib-nodejs
npm install
```

### Linux (Ubuntu/Debian)

```bash
# Install dependencies
sudo apt-get update
sudo apt-get install cmake libfftw3-dev libboost-all-dev gfortran build-essential

# Clone and install
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx-lib-nodejs
npm install
```

### Windows

```bash
# Install dependencies using vcpkg or manually
# Ensure CMake, FFTW3, Boost, and MinGW-w64 are available

# Clone and install
git clone --recursive https://github.com/boybook/wsjtx_lib_nodejs.git
cd wsjtx-lib-nodejs
npm install
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
        'CQ TEST K1ABC FN20',
        1000  // Audio base frequency in Hz (typically 500-3000 Hz)
    );
    
    console.log(`Generated ${encodeResult.audioData.length} audio samples`);
    console.log(`Message: "${encodeResult.messageSent}"`);
    
    // Decode audio data
    const audioData = new Float32Array(48000 * 13); // 13 seconds at 48kHz
    // ... fill audioData with actual audio samples ...
    
    const decodeResult = await lib.decode(
        WSJTXMode.FT8,
        audioData,
        1000  // Same audio base frequency used for encoding
    );
    
    // Get decoded messages
    const messages = lib.pullMessages();
    messages.forEach(msg => {
        console.log(`Decoded: "${msg.text}" (SNR: ${msg.snr} dB)`);
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
- `frequency`: Audio base frequency in Hz (typically 500-3000 Hz for FT8)
- `threads`: Number of threads to use (optional, default: 4)

**Returns:** Promise resolving to DecodeResult with success status

##### `encode(mode, message, frequency, threads?): Promise<EncodeResult>`

Encode a message into audio waveform for transmission.

**Parameters:**
- `mode`: WSJTXMode enum value
- `message`: Message text to encode (1-22 characters)
- `frequency`: Audio base frequency in Hz (typically 500-3000 Hz for FT8)
- `threads`: Number of threads to use (optional, default: 4)

**Returns:** Promise resolving to EncodeResult with audio data and actual message sent

##### `decodeWSPR(iqData, options?): Promise<WSPRResult[]>`

Decode WSPR signals from IQ data.

**Parameters:**
- `iqData`: Float32Array of interleaved I,Q samples
- `options`: WSPRDecodeOptions (optional)

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
    timestamp: number;      // Unix timestamp
    sync: number;           // Sync quality metric
}
```

#### EncodeResult

```typescript
interface EncodeResult {
    audioData: Float32Array;  // Generated audio waveform
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

### Basic FT8 Encoding and Decoding

```typescript
import { WSJTXLib, WSJTXMode } from 'wsjtx-lib';

const lib = new WSJTXLib();

// Encode a message
const result = await lib.encode(WSJTXMode.FT8, 'CQ DX K1ABC FN20', 1500);
console.log(`Audio samples: ${result.audioData.length}`);

// Decode audio (replace with actual audio data)
const audioData = new Float32Array(48000 * 13);
const decodeResult = await lib.decode(WSJTXMode.FT8, audioData, 1500);

// Get messages
const messages = lib.pullMessages();
console.log(`Found ${messages.length} messages`);
```

> **Important Note**: The `frequency` parameter is the **audio base frequency** (typically 500-3000 Hz), not the RF frequency. For example, if you're operating on 20m FT8 (14.074 MHz RF), you might use 1500 Hz as the audio frequency within your transceiver's passband.

### WSPR Decoding

```typescript
import { WSJTXLib } from 'wsjtx-lib';

const lib = new WSJTXLib();

// IQ data (interleaved I,Q samples)
const iqData = new Float32Array(2 * 12000 * 120); // 2 minutes
// ... fill with actual IQ data ...

const options = {
    dialFrequency: 14095600,  // RF dial frequency for WSPR (this is different from audio frequency)
    callsign: 'K1ABC',
    locator: 'FN20',
    quickMode: false,
    passes: 2
};

const results = await lib.decodeWSPR(iqData, options);
results.forEach(result => {
    console.log(`${result.callsign} ${result.locator} ${result.power}dBm`);
});
```

### Audio Format Conversion

```typescript
import { WSJTXLib } from 'wsjtx-lib';

// Convert Float32Array to Int16Array
const floatData = new Float32Array([0.5, -0.5, 0.25]);
const intData = WSJTXLib.convertAudioFormat(floatData, 'int16');

// Convert back to Float32Array
const backToFloat = WSJTXLib.convertAudioFormat(intData, 'float32');
```

## Error Handling

The library throws `WSJTXError` for all operation failures:

```typescript
import { WSJTXError } from 'wsjtx-lib';

try {
    await lib.decode(WSJTXMode.FT8, audioData, 1500);
} catch (error) {
    if (error instanceof WSJTXError) {
        console.error(`WSJTX Error [${error.code}]: ${error.message}`);
    } else {
        console.error('Unexpected error:', error);
    }
}
```

## Building from Source

```bash
# Clone with submodules
git clone --recursive https://github.com/your-repo/wsjtx-lib-nodejs.git
cd wsjtx-lib-nodejs

# Install dependencies
npm install

# Build native module and TypeScript
npm run build

# Run tests
npm test

# Run example
node examples/basic_usage.js
```

## Development

### Project Structure

```
wsjtx-lib-nodejs/
├── src/                 # TypeScript source files
├── native/              # C++ wrapper code
├── wsjtx_lib/          # Git submodule (wsjtx_lib C library)
├── test/               # Test files
├── examples/           # Usage examples
├── dist/               # Compiled TypeScript output
└── build/              # CMake build directory
```

### Scripts

- `npm run build` - Build both native module and TypeScript
- `npm run build:native` - Build only the native C++ module
- `npm run build:ts` - Build only TypeScript
- `npm test` - Run all tests
- `npm run clean` - Clean build artifacts

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Based on the excellent [wsjtx_lib](https://github.com/original-repo/wsjtx_lib) C library
- WSJT-X development team for the original algorithms
- Amateur radio community for protocol specifications

## Support

- 📧 Email: support@example.com
- 🐛 Issues: [GitHub Issues](https://github.com/your-repo/wsjtx-lib-nodejs/issues)
- 📖 Documentation: [Wiki](https://github.com/your-repo/wsjtx-lib-nodejs/wiki)
- 💬 Discussions: [GitHub Discussions](https://github.com/your-repo/wsjtx-lib-nodejs/discussions) 