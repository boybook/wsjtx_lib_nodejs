# Q65 transmit/receive chain

This branch exposes Q65 through the existing `WSJTXLib.encode()` and `WSJTXLib.decode()` APIs with mode-specific options, while preserving the legacy FT8/FT4-style call shape.

## Implemented scope

- Q65 is reported as both encode-capable and decode-capable.
- Transmit uses the WSJT-X `genq65_()` encoder and `genwave_()` waveform generator.
- Receive routes audio through WSJT-X multimode decoder mode `66`.
- Q65 decode callbacks are forwarded into the Node-visible decoded-message queue.
- Q65 period and submode are public encode and decode options.
- Q65 drift and averaging controls are public decode options.

## Public API

Legacy thread-count argument remains supported:

```ts
await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, 1);
```

Object options can be used for Q65-specific transmit settings:

```ts
await lib.encode(WSJTXMode.Q65, 'CQ K1ABC FN20', 1500, {
  threads: 1,
  q65Period: 30,
  q65Submode: 'B',
});
```

The same period/submode options can be used on receive:

```ts
await lib.decode(WSJTXMode.Q65, audio, {
  frequency: 1500,
  threads: 1,
  q65Period: 30,
  q65Submode: 'B',
  q65MaxDrift: 50,
  q65ClearAveraging: true,
  q65SingleDecode: true,
  q65Averaging: true,
});
```

## Q65 transmit parameters

| Option | Accepted values | Default |
|---|---|---:|
| `q65Period` | `30`, `60`, `120`, `300` | `60` |
| `q65Submode` | `'A'..'E'` or `0..4` | `'A'` / `0` |
| `threads` | `1..16` | `WSJTXConfig.maxThreads` |

The `frequency` argument is the audio offset in Hz, not the RF dial frequency.

The encoder emits a complete frame with length `q65Period * sampleRate` samples. At the default 12 kHz sample rate this is 360000, 720000, 1440000, or 3600000 samples for Q65-30/60/120/300 respectively.

## Q65 receive parameters

| Option | Accepted values | Default |
|---|---|---:|
| `q65Period` | `30`, `60`, `120`, `300` | `60` |
| `q65Submode` | `'A'..'E'` or `0..4` | `'A'` / `0` |
| `q65MaxDrift` | non-negative integer | `50` |
| `q65ClearAveraging` | boolean | `false` |
| `q65SingleDecode` | boolean | `false` |
| `q65Averaging` | boolean | `false` |

Existing decode options continue to map onto the WSJT-X decoder: `frequency`, `txFrequency`, `lowFreq`, `highFreq`, `tolerance`, station/grid context, AP decode toggle, decode depth, and QSO progress.

## Native mapping

The Q65 receive path sets or derives:

- `nmode = 66`
- `ntrperiod = q65Period`
- `nsubmode = q65Submode`
- `ntxmode = 66`
- `nzhsym = 85`
- `max_drift = q65MaxDrift`
- `nclearave = q65ClearAveraging`
- `nexp_decode |= 32` when `q65SingleDecode` is true
- `ndepth |= 16` when `q65Averaging` is true

## Regression coverage

The smoke test suite covers Q65 capability reporting, enum stability, legacy thread-count encode calls, object encode options for period/submode, validation of invalid Q65 options, and decode option plumbing for drift and averaging controls.

Run:

```bash
npm run build
npm test
```
