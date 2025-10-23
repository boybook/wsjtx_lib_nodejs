#include "wsjtx_bridge.h"
#include "../wsjtx_lib/wsjtx_lib.h"  // 引用子模块的C++头文件
#include <new>
#include <cstring>
#include <stdexcept>

// ==================== 内部辅助函数 ====================

namespace {
    // 将C枚举转换为C++枚举
    wsjtxMode convert_mode(wsjtx_mode_t mode) {
        switch (mode) {
            case WSJTX_MODE_FT8:  return wsjtxMode::FT8;
            case WSJTX_MODE_FT4:  return wsjtxMode::FT4;
            case WSJTX_MODE_JT65: return wsjtxMode::JT65;
            case WSJTX_MODE_WSPR: return wsjtxMode::WSPR;
            default:              return wsjtxMode::FT8;
        }
    }

    // 获取采样率
    int get_sample_rate_internal(wsjtx_mode_t mode) {
        switch (mode) {
            case WSJTX_MODE_FT8:  return 12000;
            case WSJTX_MODE_FT4:  return 12000;
            case WSJTX_MODE_JT65: return 12000;
            case WSJTX_MODE_WSPR: return 12000;
            default:              return 12000;
        }
    }

    // 获取最大采样数
    int get_max_samples_internal(wsjtx_mode_t mode) {
        int sample_rate = get_sample_rate_internal(mode);
        switch (mode) {
            case WSJTX_MODE_FT8:  return sample_rate * 15;  // 15秒
            case WSJTX_MODE_FT4:  return sample_rate * 7;   // 7.5秒
            case WSJTX_MODE_JT65: return sample_rate * 60;  // 60秒
            case WSJTX_MODE_WSPR: return sample_rate * 120; // 120秒
            default:              return sample_rate * 15;
        }
    }
}

// ==================== 核心API实现 ====================

extern "C" {

wsjtx_handle_t wsjtx_create() {
    try {
        return new wsjtx_lib();  // 在MinGW堆上分配
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

void wsjtx_destroy(wsjtx_handle_t handle) {
    if (handle) {
        try {
            delete static_cast<wsjtx_lib*>(handle);  // 在MinGW堆上释放
        } catch (...) {
            // 忽略析构函数异常
        }
    }
}

int wsjtx_decode(
    wsjtx_handle_t handle,
    wsjtx_mode_t mode,
    const float* audio_samples,
    int sample_count,
    int frequency,
    int num_threads
) {
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    if (!audio_samples || sample_count <= 0) return WSJTX_ERR_INVALID_PARAM;
    if (num_threads < 1) num_threads = 1;

    try {
        auto* lib = static_cast<wsjtx_lib*>(handle);

        // 转换模式枚举
        wsjtxMode cpp_mode = convert_mode(mode);

        // 复制数据到MinGW侧（避免跨CRT指针依赖）
        WsjTxVector samples(audio_samples, audio_samples + sample_count);

        // 调用内部C++ API
        lib->decode(cpp_mode, samples, frequency, num_threads);

        return WSJTX_OK;
    } catch (const std::invalid_argument&) {
        return WSJTX_ERR_INVALID_PARAM;
    } catch (const std::bad_alloc&) {
        return WSJTX_ERR_OUT_OF_MEMORY;
    } catch (const std::exception&) {
        return WSJTX_ERR_DECODE_FAILED;
    } catch (...) {
        return WSJTX_ERR_INTERNAL;
    }
}

int wsjtx_pull_message(wsjtx_handle_t handle, wsjtx_message_t* message) {
    if (!handle || !message) return WSJTX_ERR_INVALID_HANDLE;

    try {
        auto* lib = static_cast<wsjtx_lib*>(handle);
        WsjtxMessage msg;

        if (!lib->pullMessage(msg)) {
            return 0;  // 队列为空
        }

        // 填充C结构体（调用方提供的结构体）
        message->hh = msg.hh;
        message->min = msg.min;
        message->sec = msg.sec;
        message->snr = msg.snr;
        message->sync = msg.sync;
        message->dt = msg.dt;
        message->freq = msg.freq;

        // 安全复制字符串
        strncpy(message->message, msg.msg.c_str(), sizeof(message->message) - 1);
        message->message[sizeof(message->message) - 1] = '\0';

        return 1;  // 成功获取一条消息
    } catch (const std::exception&) {
        return WSJTX_ERR_INVALID_HANDLE;
    } catch (...) {
        return WSJTX_ERR_INTERNAL;
    }
}

int wsjtx_encode(
    wsjtx_handle_t handle,
    wsjtx_mode_t mode,
    const char* message,
    int frequency,
    float* output_samples,
    int* output_sample_count
) {
    if (!handle) return WSJTX_ERR_INVALID_HANDLE;
    if (!message || !output_samples || !output_sample_count) return WSJTX_ERR_NULL_POINTER;
    if (*output_sample_count <= 0) return WSJTX_ERR_INVALID_PARAM;

    try {
        auto* lib = static_cast<wsjtx_lib*>(handle);

        // 转换模式枚举
        wsjtxMode cpp_mode = convert_mode(mode);

        // 在MinGW堆上创建临时对象
        std::string msg_str(message);
        std::string actual_msg;

        // 调用内部API（返回MinGW的std::vector）
        std::vector<float> result = lib->encode(cpp_mode, frequency, msg_str, actual_msg);

        // 检查缓冲区大小
        if (result.size() > static_cast<size_t>(*output_sample_count)) {
            return WSJTX_ERR_BUFFER_TOO_SMALL;
        }

        // 复制数据到调用方的缓冲区
        std::copy(result.begin(), result.end(), output_samples);
        *output_sample_count = static_cast<int>(result.size());

        // result、msg_str等MinGW对象在此处析构（MinGW堆释放）
        return WSJTX_OK;
    } catch (const std::invalid_argument&) {
        return WSJTX_ERR_INVALID_PARAM;
    } catch (const std::bad_alloc&) {
        return WSJTX_ERR_OUT_OF_MEMORY;
    } catch (const std::exception&) {
        return WSJTX_ERR_ENCODE_FAILED;
    } catch (...) {
        return WSJTX_ERR_INTERNAL;
    }
}

// ==================== 辅助函数实现 ====================

int wsjtx_get_sample_rate(wsjtx_mode_t mode) {
    return get_sample_rate_internal(mode);
}

int wsjtx_get_max_samples(wsjtx_mode_t mode) {
    return get_max_samples_internal(mode);
}

const char* wsjtx_error_string(int error_code) {
    switch (error_code) {
        case WSJTX_OK:                          return "Success";
        case WSJTX_ERR_INVALID_HANDLE:          return "Invalid handle";
        case WSJTX_ERR_INVALID_MODE:            return "Invalid mode";
        case WSJTX_ERR_INVALID_PARAM:           return "Invalid parameter";
        case WSJTX_ERR_NULL_POINTER:            return "Null pointer";
        case WSJTX_ERR_BUFFER_TOO_SMALL:        return "Buffer too small";
        case WSJTX_ERR_DECODE_FAILED:           return "Decode failed";
        case WSJTX_ERR_ENCODE_FAILED:           return "Encode failed";
        case WSJTX_ERR_OUT_OF_MEMORY:           return "Out of memory";
        case WSJTX_ERR_THREAD_ERROR:            return "Thread error";
        case WSJTX_ERR_NOT_INITIALIZED:         return "Not initialized";
        case WSJTX_ERR_ALREADY_INITIALIZED:     return "Already initialized";
        case WSJTX_ERR_INTERNAL:                return "Internal error";
        default:                                return "Unknown error";
    }
}

} // extern "C"
