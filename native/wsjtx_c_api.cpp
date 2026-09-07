/**
 * wsjtx_c_api.cpp - C API implementation for wsjtx_lib
 *
 * This file is compiled into the wsjtx_core shared library.
 * It wraps the C++ wsjtx_lib class with pure C functions.
 * All C++ exceptions are caught at this boundary.
 */

#include "wsjtx_c_api.h"
#include <wsjtx_lib.h>
#include <cstring>
#include <vector>
#include <complex>
#include <string>
#include <utility>
#include <algorithm>

/* Mode metadata table (mirrors wsjtx_wrapper.cpp MODE_INFO) */
struct ModeMetadata {
    int sampleRate;
    double duration;
    int encodingSupported;
    int decodingSupported;
};

static const ModeMetadata MODE_TABLE[] = {
    /* FT8     */ { 12000, 12.64, 1, 1 },
    /* FT4     */ { 12000,  6.0,  1, 1 },
    /* JT4     */ { 11025, 47.1,  0, 1 },
    /* JT65    */ { 11025, 46.8,  0, 1 },
    /* JT9     */ { 12000, 49.0,  0, 1 },
    /* FST4    */ { 12000, 60.0,  0, 1 },
    /* Q65     */ { 12000, 60.0,  0, 1 },
    /* FST4W   */ { 12000, 120.0, 0, 1 },
    /* JT65JT9 */ { 11025, 46.8,  0, 1 },
    /* WSPR    */ { 12000, 110.6, 0, 1 },
};

static const int MODE_COUNT = sizeof(MODE_TABLE) / sizeof(MODE_TABLE[0]);

static inline int valid_mode(int mode) {
    return mode >= 0 && mode < MODE_COUNT;
}

static inline wsjtx_lib* to_lib(wsjtx_handle_t h) {
    return static_cast<wsjtx_lib*>(h);
}

extern "C" void wsjtx_decode_stats_(int *stage, int *candidates, int *decoded, int *average);

/* Convert the C ABI options into one atomic C++ invocation configuration.
 * Keeping this as a value object lets wsjtx_lib apply the complete request
 * while holding its decoder mutex, so concurrent N-API workers cannot mix
 * station/range/depth/session fields between calls. */
static WsjtxDecodeConfig make_decode_config(const wsjtx_decode_options_t* opts) {
    WsjtxDecodeConfig config;
    config.low_freq = opts->low_freq;
    config.high_freq = opts->high_freq;
    config.tolerance = opts->tolerance;
    config.ap_decode = opts->ap_decode != 0;
    config.decode_depth = opts->decode_depth;
    config.tx_frequency = opts->tx_frequency;
    config.qso_progress = opts->qso_progress;
    config.my_call = std::string(opts->mycall);
    config.my_grid = std::string(opts->mygrid);
    config.dx_call = std::string(opts->hiscall);
    config.dx_grid = std::string(opts->hisgrid);
    wsjtx_decode_stage_t stage{};
    stage.stage_symbols = opts->stage_symbols > 0 ? opts->stage_symbols : 50;
    stage.slot_utc = opts->slot_utc;
    stage.reset_state = opts->reset_session != 0;
    stage.nagain = opts->nagain != 0;
    stage.eme_delay_ms = opts->eme_delay_ms;
    stage.session_id = opts->session_id;
    config.stage = std::move(stage);
    return config;
}

static void copy_message(wsjtx_message_t* dst, const WsjtxMessage& src);

static void copy_stats(wsjtx_decode_stats_t* output) {
    if (!output) return;
    const auto stats = wsjtx_last_stats();
    output->stage_symbols = stats.stage_symbols;
    output->candidate_count = stats.candidate_count;
    output->decoded_count = stats.decoded_count;
    output->average_count = stats.average_count;
}

static int copy_messages(const std::vector<WsjtxMessage>& messages,
    wsjtx_message_t* out_messages, int max_messages, int* out_num_messages) {
    if (!out_num_messages || max_messages < 0 || (max_messages > 0 && !out_messages)) {
        return WSJTX_ERR_BUFFER_TOO_SMALL;
    }
    const int count = std::min(static_cast<int>(messages.size()), max_messages);
    for (int i = 0; i < count; i++) copy_message(&out_messages[i], messages[i]);
    *out_num_messages = count;
    return WSJTX_OK;
}

/* ---- Lifecycle ---- */

WSJTX_API wsjtx_handle_t wsjtx_create(void) {
    try {
        return static_cast<wsjtx_handle_t>(new wsjtx_lib());
    } catch (...) {
        return nullptr;
    }
}

WSJTX_API void wsjtx_destroy(wsjtx_handle_t handle) {
    delete to_lib(handle);
}

/* ---- Decode (legacy) ---- */

WSJTX_API int wsjtx_decode_float(wsjtx_handle_t handle, int mode,
    float* samples, int num_samples, int freq, int threads)
{
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;

    try {
        std::vector<float> data(samples, samples + num_samples);
        to_lib(handle)->decode(static_cast<wsjtxMode>(mode), data, freq, threads);
        return WSJTX_OK;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

WSJTX_API int wsjtx_decode_int16(wsjtx_handle_t handle, int mode,
    int16_t* samples, int num_samples, int freq, int threads)
{
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;

    try {
        std::vector<short int> data(samples, samples + num_samples);
        to_lib(handle)->decode(static_cast<wsjtxMode>(mode), data, freq, threads);
        return WSJTX_OK;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

/* ---- Decode (v2 with options) ---- */

WSJTX_API int wsjtx_decode_float_v2(wsjtx_handle_t handle, int mode,
    const float* samples, int num_samples,
    const wsjtx_decode_options_t* options)
{
    if (!handle || !options) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;

    try {
        wsjtx_lib* lib = to_lib(handle);
        auto config = make_decode_config(options);
        std::vector<float> data(samples, samples + num_samples);
        lib->decodeWithOptions(static_cast<wsjtxMode>(mode), data, options->frequency, options->threads, config);
        return WSJTX_OK;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

WSJTX_API int wsjtx_decode_int16_v2(wsjtx_handle_t handle, int mode,
    const int16_t* samples, int num_samples,
    const wsjtx_decode_options_t* options)
{
    if (!handle || !options) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;

    try {
        wsjtx_lib* lib = to_lib(handle);
        auto config = make_decode_config(options);
        std::vector<short int> data(samples, samples + num_samples);
        lib->decodeWithOptions(static_cast<wsjtxMode>(mode), data, options->frequency, options->threads, config);
        return WSJTX_OK;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

WSJTX_API int wsjtx_decode_float_v3(wsjtx_handle_t handle, int mode,
    const float* samples, int num_samples, const wsjtx_decode_options_t* options,
    wsjtx_message_t* out_messages, int max_messages, int* out_num_messages,
    wsjtx_decode_stats_t* out_stats)
{
    if (!handle || !options) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;
    try {
        auto config = make_decode_config(options);
        std::vector<float> data(samples, samples + num_samples);
        std::vector<WsjtxMessage> messages;
        to_lib(handle)->decodeWithOptionsAndPull(static_cast<wsjtxMode>(mode), data, options->frequency, options->threads, config, messages);
        copy_stats(out_stats);
        return copy_messages(messages, out_messages, max_messages, out_num_messages);
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

WSJTX_API int wsjtx_decode_int16_v3(wsjtx_handle_t handle, int mode,
    const int16_t* samples, int num_samples, const wsjtx_decode_options_t* options,
    wsjtx_message_t* out_messages, int max_messages, int* out_num_messages,
    wsjtx_decode_stats_t* out_stats)
{
    if (!handle || !options) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;
    try {
        auto config = make_decode_config(options);
        std::vector<short int> data(samples, samples + num_samples);
        std::vector<WsjtxMessage> messages;
        to_lib(handle)->decodeWithOptionsAndPull(static_cast<wsjtxMode>(mode), data, options->frequency, options->threads, config, messages);
        copy_stats(out_stats);
        return copy_messages(messages, out_messages, max_messages, out_num_messages);
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

WSJTX_API int wsjtx_end_decode_session(wsjtx_handle_t handle, const char* session_id) {
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    try {
        to_lib(handle)->endDecodeSession(session_id ? std::string(session_id) : std::string{});
        return WSJTX_OK;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

WSJTX_API int wsjtx_get_last_decode_stats(wsjtx_handle_t handle, wsjtx_decode_stats_t* stats) {
    if (!handle || !stats) return WSJTX_ERR_INVALID_HANDLE;
    const auto last = wsjtx_last_stats();
    stats->stage_symbols = last.stage_symbols;
    stats->candidate_count = last.candidate_count;
    stats->decoded_count = last.decoded_count;
    stats->average_count = last.average_count;
    return WSJTX_OK;
}

/* ---- Encode ---- */

WSJTX_API int wsjtx_encode(wsjtx_handle_t handle, int mode, int freq, int sample_rate,
    const char* message,
    float* out_samples, int* out_num_samples, int out_buf_size,
    char* out_message_sent, int out_msg_buf_size)
{
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;
    if (sample_rate != 12000 && sample_rate != 48000) return WSJTX_ERR_INVALID_SAMPLE_RATE;

    try {
        std::string messageSent;
        std::vector<float> audio = to_lib(handle)->encode(
            static_cast<wsjtxMode>(mode), freq, std::string(message), messageSent, sample_rate);

        if (audio.empty()) return WSJTX_ERR_ENCODE_FAILED;

        int n = static_cast<int>(audio.size());
        if (n > out_buf_size) return WSJTX_ERR_BUFFER_TOO_SMALL;

        memcpy(out_samples, audio.data(), n * sizeof(float));
        *out_num_samples = n;

        if (out_message_sent && out_msg_buf_size > 0) {
            strncpy(out_message_sent, messageSent.c_str(), out_msg_buf_size - 1);
            out_message_sent[out_msg_buf_size - 1] = '\0';
        }

        return WSJTX_OK;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

/* ---- Message queue ---- */

static void copy_message(wsjtx_message_t* dst, const WsjtxMessage& src) {
    dst->hh   = src.hh;
    dst->min  = src.min;
    dst->sec  = src.sec;
    dst->snr  = src.snr;
    dst->freq = src.freq;
    dst->sync = src.sync;
    dst->dt   = src.dt;
    memset(dst->msg, 0, sizeof(dst->msg));
    strncpy(dst->msg, src.msg.c_str(), sizeof(dst->msg) - 1);
}

WSJTX_API int wsjtx_pull_message(wsjtx_handle_t handle, wsjtx_message_t* out_msg) {
    if (!handle || !out_msg) return 0;

    try {
        WsjtxMessage msg;
        if (!to_lib(handle)->pullMessage(msg)) return 0;
        copy_message(out_msg, msg);
        return 1;
    } catch (...) {
        return 0;
    }
}

WSJTX_API int wsjtx_pull_messages(wsjtx_handle_t handle,
    wsjtx_message_t* out_messages, int max_messages)
{
    if (!handle || !out_messages || max_messages <= 0) return 0;

    try {
        wsjtx_lib* lib = to_lib(handle);
        WsjtxMessage msg;
        int count = 0;
        while (count < max_messages && lib->pullMessage(msg)) {
            copy_message(&out_messages[count], msg);
            count++;
        }
        return count;
    } catch (...) {
        return 0;
    }
}

/* ---- WSPR ---- */

WSJTX_API int wsjtx_wspr_decode(wsjtx_handle_t handle,
    float* iq_interleaved, int num_iq_samples,
    wsjtx_decoder_options_t* options,
    wsjtx_decoder_result_t* out_results, int max_results)
{
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;

    try {
        /* Reconstruct complex vector from interleaved floats */
        std::vector<std::complex<float>> iqData;
        iqData.reserve(num_iq_samples);
        for (int i = 0; i < num_iq_samples; i++) {
            iqData.emplace_back(iq_interleaved[i * 2], iq_interleaved[i * 2 + 1]);
        }

        /* Convert C options to C++ decoder_options */
        decoder_options opts;
        opts.freq         = options->freq;
        opts.quickmode    = options->quickmode;
        opts.usehashtable = options->usehashtable;
        opts.npasses      = options->npasses;
        opts.subtraction  = options->subtraction;
        strncpy(opts.rcall, options->rcall, sizeof(opts.rcall) - 1);
        opts.rcall[sizeof(opts.rcall) - 1] = '\0';
        strncpy(opts.rloc, options->rloc, sizeof(opts.rloc) - 1);
        opts.rloc[sizeof(opts.rloc) - 1] = '\0';

        std::vector<decoder_results> results = to_lib(handle)->wspr_decode(iqData, opts);

        int count = static_cast<int>(results.size());
        if (count > max_results) count = max_results;

        for (int i = 0; i < count; i++) {
            out_results[i].freq   = results[i].freq;
            out_results[i].sync   = results[i].sync;
            out_results[i].snr    = results[i].snr;
            out_results[i].dt     = results[i].dt;
            out_results[i].drift  = results[i].drift;
            out_results[i].jitter = results[i].jitter;
            out_results[i].cycles = results[i].cycles;

            memcpy(out_results[i].message, results[i].message, sizeof(results[i].message));
            memcpy(out_results[i].call, results[i].call, sizeof(results[i].call));
            memcpy(out_results[i].loc, results[i].loc, sizeof(results[i].loc));
            memcpy(out_results[i].pwr, results[i].pwr, sizeof(results[i].pwr));
        }

        return count;
    } catch (...) {
        return WSJTX_ERR_EXCEPTION;
    }
}

/* ---- Stateless queries ---- */

WSJTX_API int wsjtx_is_encoding_supported(int mode) {
    if (!valid_mode(mode)) return 0;
    return MODE_TABLE[mode].encodingSupported;
}

WSJTX_API int wsjtx_is_decoding_supported(int mode) {
    if (!valid_mode(mode)) return 0;
    return MODE_TABLE[mode].decodingSupported;
}

WSJTX_API int wsjtx_get_sample_rate(int mode) {
    if (!valid_mode(mode)) return 12000;
    return MODE_TABLE[mode].sampleRate;
}

WSJTX_API double wsjtx_get_transmission_duration(int mode) {
    if (!valid_mode(mode)) return 60.0;
    return MODE_TABLE[mode].duration;
}
