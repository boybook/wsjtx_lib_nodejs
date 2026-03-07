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

/* Mode metadata table (mirrors wsjtx_wrapper.cpp MODE_INFO) */
struct ModeMetadata {
    int sampleRate;
    double duration;
    int encodingSupported;
    int decodingSupported;
};

static const ModeMetadata MODE_TABLE[] = {
    /* FT8     */ { 48000, 12.64, 1, 1 },
    /* FT4     */ { 48000,  6.0,  1, 1 },
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

/* ---- Decode ---- */

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

/* ---- Encode ---- */

WSJTX_API int wsjtx_encode(wsjtx_handle_t handle, int mode, int freq,
    const char* message,
    float* out_samples, int* out_num_samples, int out_buf_size,
    char* out_message_sent, int out_msg_buf_size)
{
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    if (!valid_mode(mode)) return WSJTX_ERR_INVALID_MODE;

    try {
        std::string messageSent;
        std::vector<float> audio = to_lib(handle)->encode(
            static_cast<wsjtxMode>(mode), freq, std::string(message), messageSent);

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

WSJTX_API int wsjtx_pull_message(wsjtx_handle_t handle, wsjtx_message_t* out_msg) {
    if (!handle || !out_msg) return 0;

    try {
        WsjtxMessage msg;
        if (!to_lib(handle)->pullMessage(msg)) return 0;

        out_msg->hh   = msg.hh;
        out_msg->min  = msg.min;
        out_msg->sec  = msg.sec;
        out_msg->snr  = msg.snr;
        out_msg->freq = msg.freq;
        out_msg->sync = msg.sync;
        out_msg->dt   = msg.dt;

        memset(out_msg->msg, 0, sizeof(out_msg->msg));
        strncpy(out_msg->msg, msg.msg.c_str(), sizeof(out_msg->msg) - 1);

        return 1;
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
