#pragma once

#ifdef _WIN32
  #ifdef WSJTX_BRIDGE_EXPORTS
    #define WSJTX_BRIDGE_API __declspec(dllexport)
  #else
    #define WSJTX_BRIDGE_API __declspec(dllimport)
  #endif
#else
  // Unix (Linux/macOS): 使用 GCC/Clang visibility 属性
  #ifdef WSJTX_BRIDGE_EXPORTS
    #define WSJTX_BRIDGE_API __attribute__((visibility("default")))
  #else
    #define WSJTX_BRIDGE_API
  #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 不透明句柄（隐藏C++实现）
typedef void* wsjtx_handle_t;

// 错误码定义
typedef enum {
    WSJTX_OK = 0,
    WSJTX_ERR_INVALID_HANDLE = -1,
    WSJTX_ERR_INVALID_MODE = -2,
    WSJTX_ERR_INVALID_PARAM = -3,
    WSJTX_ERR_NULL_POINTER = -4,
    WSJTX_ERR_BUFFER_TOO_SMALL = -5,
    WSJTX_ERR_DECODE_FAILED = -10,
    WSJTX_ERR_ENCODE_FAILED = -11,
    WSJTX_ERR_OUT_OF_MEMORY = -12,
    WSJTX_ERR_THREAD_ERROR = -13,
    WSJTX_ERR_NOT_INITIALIZED = -20,
    WSJTX_ERR_ALREADY_INITIALIZED = -21,
    WSJTX_ERR_INTERNAL = -99
} wsjtx_error_t;

// 模式枚举
typedef enum {
    WSJTX_MODE_FT8 = 0,
    WSJTX_MODE_FT4 = 1,
    WSJTX_MODE_JT65 = 2,
    WSJTX_MODE_WSPR = 3
} wsjtx_mode_t;

// 解码结果结构体（固定大小，避免动态分配）
typedef struct {
    int hh;
    int min;
    int sec;
    int snr;
    float sync;
    float dt;
    int freq;
    char message[80];  // 固定大小，避免动态分配
} wsjtx_message_t;

// ==================== 核心API函数 ====================

/**
 * 创建WSJTX库实例
 * @return 库句柄，失败返回NULL
 */
WSJTX_BRIDGE_API wsjtx_handle_t wsjtx_create(void);

/**
 * 销毁WSJTX库实例
 * @param handle 库句柄
 */
WSJTX_BRIDGE_API void wsjtx_destroy(wsjtx_handle_t handle);

/**
 * 解码音频数据
 * @param handle 库句柄
 * @param mode 解码模式
 * @param audio_samples 音频采样数据（调用方分配）
 * @param sample_count 采样数量
 * @param frequency 频率（Hz）
 * @param num_threads 线程数
 * @return WSJTX_OK成功，其他值表示错误
 */
WSJTX_BRIDGE_API int wsjtx_decode(
    wsjtx_handle_t handle,
    wsjtx_mode_t mode,
    const float* audio_samples,
    int sample_count,
    int frequency,
    int num_threads
);

/**
 * 从队列中拉取一条解码消息
 * @param handle 库句柄
 * @param message 消息结构体指针（调用方分配）
 * @return 1表示成功获取消息，0表示队列为空，负值表示错误
 */
WSJTX_BRIDGE_API int wsjtx_pull_message(
    wsjtx_handle_t handle,
    wsjtx_message_t* message
);

/**
 * 编码消息为音频数据
 * @param handle 库句柄
 * @param mode 编码模式
 * @param message 要编码的消息文本
 * @param frequency 频率（Hz）
 * @param output_samples 输出音频缓冲区（调用方分配）
 * @param output_sample_count 输入：缓冲区大小，输出：实际样本数
 * @return WSJTX_OK成功，其他值表示错误
 */
WSJTX_BRIDGE_API int wsjtx_encode(
    wsjtx_handle_t handle,
    wsjtx_mode_t mode,
    const char* message,
    int frequency,
    float* output_samples,
    int* output_sample_count
);

// ==================== 辅助函数 ====================

/**
 * 获取指定模式的采样率
 * @param mode 模式
 * @return 采样率（Hz）
 */
WSJTX_BRIDGE_API int wsjtx_get_sample_rate(wsjtx_mode_t mode);

/**
 * 获取指定模式的最大采样数
 * @param mode 模式
 * @return 最大采样数
 */
WSJTX_BRIDGE_API int wsjtx_get_max_samples(wsjtx_mode_t mode);

/**
 * 获取错误码对应的描述字符串
 * @param error_code 错误码
 * @return 错误描述字符串（DLL管理，不应free）
 */
WSJTX_BRIDGE_API const char* wsjtx_error_string(int error_code);

#ifdef __cplusplus
}
#endif
