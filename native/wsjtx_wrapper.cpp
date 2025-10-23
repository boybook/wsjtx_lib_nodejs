#include "wsjtx_wrapper.h"
#include <chrono>
#include <complex>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

namespace wsjtx_nodejs
{

    // Static mode information
    struct ModeInfo
    {
        int sampleRate;
        double duration;
        bool encodingSupported;
        bool decodingSupported;
    };

    static const std::map<wsjtxMode, ModeInfo> MODE_INFO = {
        {FT8, {48000, 12.64, true, true}},
        {FT4, {48000, 6.0, true, true}},
        {JT4, {11025, 47.1, false, true}},
        {JT65, {11025, 46.8, false, true}},
        {JT9, {12000, 49.0, false, true}},
        {FST4, {12000, 60.0, false, true}},
        {Q65, {12000, 60.0, false, true}},
        {FST4W, {12000, 120.0, false, true}},
        {WSPR, {12000, 110.6, false, true}}};

    // WSJTXLibWrapper implementation
    Napi::Object WSJTXLibWrapper::Init(Napi::Env env, Napi::Object exports)
    {
        Napi::Function func = DefineClass(env, "WSJTXLib", {
            InstanceMethod("decode", &WSJTXLibWrapper::Decode),
            InstanceMethod("encode", &WSJTXLibWrapper::Encode),
            InstanceMethod("decodeWSPR", &WSJTXLibWrapper::DecodeWSPR),
            InstanceMethod("pullMessages", &WSJTXLibWrapper::PullMessages),
            InstanceMethod("isEncodingSupported", &WSJTXLibWrapper::IsEncodingSupported),
            InstanceMethod("isDecodingSupported", &WSJTXLibWrapper::IsDecodingSupported),
            InstanceMethod("getSampleRate", &WSJTXLibWrapper::GetSampleRate),
            InstanceMethod("getTransmissionDuration", &WSJTXLibWrapper::GetTransmissionDuration),
            InstanceMethod("convertAudioFormat", &WSJTXLibWrapper::ConvertAudioFormat)
        });

        exports.Set("WSJTXLib", func);
        return exports;
    }

    // New method: convertAudioFormat(audioData, targetFormat, callback)
    Napi::Value WSJTXLibWrapper::ConvertAudioFormat(const Napi::CallbackInfo& info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 3)
        {
            Napi::TypeError::New(env, "Expected 3 arguments: audioData, targetFormat, callback").ThrowAsJavaScriptException();
            return env.Null();
        }

        if (!info[0].IsTypedArray() || !info[1].IsString() || !info[2].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        std::string target = info[1].As<Napi::String>().Utf8Value();
        AudioConvertWorker::Target tgt;
        if (target == "float32") tgt = AudioConvertWorker::Target::Float32;
        else if (target == "int16") tgt = AudioConvertWorker::Target::Int16;
        else {
            Napi::TypeError::New(env, "targetFormat must be 'float32' or 'int16'").ThrowAsJavaScriptException();
            return env.Null();
        }

        Napi::Function callback = info[2].As<Napi::Function>();

        Napi::TypedArray ta = info[0].As<Napi::TypedArray>();
        if (ta.TypedArrayType() == napi_float32_array)
        {
            auto input = ConvertToFloatArray(env, info[0]);
            auto* worker = new AudioConvertWorker(callback, input, tgt);
            worker->Queue();
        }
        else if (ta.TypedArrayType() == napi_int16_array)
        {
            auto input = ConvertToIntArray(env, info[0]);
            auto* worker = new AudioConvertWorker(callback, input, tgt);
            worker->Queue();
        }
        else
        {
            Napi::TypeError::New(env, "audioData must be Float32Array or Int16Array").ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    WSJTXLibWrapper::WSJTXLibWrapper(const Napi::CallbackInfo &info)
        : Napi::ObjectWrap<WSJTXLibWrapper>(info)
        , dll_handle_(nullptr)
        , lib_handle_(nullptr)
        , wsjtx_create_(nullptr)
        , wsjtx_destroy_(nullptr)
        , wsjtx_decode_(nullptr)
        , wsjtx_pull_message_(nullptr)
        , wsjtx_encode_(nullptr)
        , wsjtx_get_sample_rate_(nullptr)
        , wsjtx_get_max_samples_(nullptr)
    {
        LoadBridge();
    }

    // Decode method - supports Float32Array and Int16Array audio data
    Napi::Value WSJTXLibWrapper::Decode(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 5)
        {
            Napi::TypeError::New(env, "Expected 5 arguments: mode, audioData, frequency, threads, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Validate arguments
        if (!info[0].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber() || !info[4].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        int frequency = info[2].As<Napi::Number>().Int32Value();
        int threads = info[3].As<Napi::Number>().Int32Value();
        Napi::Function callback = info[4].As<Napi::Function>();

        // Validate parameters
        try
        {
            ValidateMode(env, mode);
            ValidateFrequency(env, frequency);
            ValidateThreads(env, threads);
        }
        catch (const std::exception &e)
        {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }

        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        // Check if audio data is Float32Array or Int16Array
        Napi::Value audioData = info[1];

        if (audioData.IsTypedArray())
        {
            Napi::TypedArray typedArray = audioData.As<Napi::TypedArray>();

            if (typedArray.TypedArrayType() == napi_float32_array)
            {
                // Float32Array
                auto floatData = ConvertToFloatArray(env, audioData);
                auto worker = new DecodeWorker(callback, lib_handle_, wsjtx_decode_, wsjtx_pull_message_,
                                               wsjtxModeVal, floatData, frequency, threads);
                worker->Queue();
            }
            else if (typedArray.TypedArrayType() == napi_int16_array)
            {
                // Int16Array
                auto intData = ConvertToIntArray(env, audioData);
                auto worker = new DecodeWorker(callback, lib_handle_, wsjtx_decode_, wsjtx_pull_message_,
                                               wsjtxModeVal, intData, frequency, threads);
                worker->Queue();
            }
            else
            {
                Napi::TypeError::New(env, "Audio data must be Float32Array or Int16Array")
                    .ThrowAsJavaScriptException();
                return env.Null();
            }
        }
        else
        {
            Napi::TypeError::New(env, "Audio data must be a typed array")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        return env.Undefined();
    }

    // Encode method - generates audio waveform for transmission
    Napi::Value WSJTXLibWrapper::Encode(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 5)
        {
            Napi::TypeError::New(env, "Expected 5 arguments: mode, message, frequency, threads, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Validate arguments
        if (!info[0].IsNumber() || !info[1].IsString() || !info[2].IsNumber() ||
            !info[3].IsNumber() || !info[4].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        std::string message = info[1].As<Napi::String>().Utf8Value();
        int frequency = info[2].As<Napi::Number>().Int32Value();
        int threads = info[3].As<Napi::Number>().Int32Value();
        Napi::Function callback = info[4].As<Napi::Function>();

        // Validate parameters
        try
        {
            ValidateMode(env, mode);
            ValidateFrequency(env, frequency);
            ValidateThreads(env, threads);
            ValidateMessage(env, message);
        }
        catch (const std::exception &e)
        {
            Napi::Error::New(env, e.what()).ThrowAsJavaScriptException();
            return env.Null();
        }

        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        // Check encoding support
        auto it = MODE_INFO.find(wsjtxModeVal);
        if (it == MODE_INFO.end() || !it->second.encodingSupported)
        {
            Napi::Error::New(env, "Encoding not supported for this mode")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        auto worker = new EncodeWorker(callback, lib_handle_, wsjtx_encode_, wsjtx_get_max_samples_,
                                       wsjtxModeVal, message, frequency, threads);
        worker->Queue();

        return env.Undefined();
    }

    // WSPR specific decode method with IQ data and options
    Napi::Value WSJTXLibWrapper::DecodeWSPR(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

#if WSJTX_WINDOWS_MSVC_MODE
        // WSPR is not supported in MSVC bridge mode
        Napi::Error::New(env, "WSPR decode is not currently supported in Windows MSVC mode")
            .ThrowAsJavaScriptException();
        return env.Null();
#else
        if (info.Length() < 3)
        {
            Napi::TypeError::New(env, "Expected 3 arguments: iqData, options, callback")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        // Validate arguments
        if (!info[0].IsTypedArray() || !info[1].IsObject() || !info[2].IsFunction())
        {
            Napi::TypeError::New(env, "Invalid argument types").ThrowAsJavaScriptException();
            return env.Null();
        }

        // Convert IQ data (interleaved I,Q samples)
        Napi::Float32Array iqArray = info[0].As<Napi::Float32Array>();
        size_t length = iqArray.ElementLength();

        if (length % 2 != 0)
        {
            Napi::Error::New(env, "IQ data length must be even (interleaved I,Q samples)")
                .ThrowAsJavaScriptException();
            return env.Null();
        }

        WsjtxIQSampleVector iqData;
        iqData.reserve(length / 2);

        float *data = iqArray.Data();
        for (size_t i = 0; i < length; i += 2)
        {
            iqData.emplace_back(data[i], data[i + 1]);
        }

        // Parse decoder options
        Napi::Object options = info[1].As<Napi::Object>();
        decoder_options decoderOptions;

        if (options.Has("dialFrequency"))
        {
            decoderOptions.freq = options.Get("dialFrequency").As<Napi::Number>().Int32Value();
        }

        if (options.Has("callsign"))
        {
            std::string callsign = options.Get("callsign").As<Napi::String>().Utf8Value();
            strncpy(decoderOptions.rcall, callsign.c_str(), sizeof(decoderOptions.rcall) - 1);
            decoderOptions.rcall[sizeof(decoderOptions.rcall) - 1] = '\0';
        }

        if (options.Has("locator"))
        {
            std::string locator = options.Get("locator").As<Napi::String>().Utf8Value();
            strncpy(decoderOptions.rloc, locator.c_str(), sizeof(decoderOptions.rloc) - 1);
            decoderOptions.rloc[sizeof(decoderOptions.rloc) - 1] = '\0';
        }

        if (options.Has("quickMode"))
        {
            decoderOptions.quickmode = options.Get("quickMode").As<Napi::Boolean>().Value() ? 1 : 0;
        }

        if (options.Has("useHashTable"))
        {
            decoderOptions.usehashtable = options.Get("useHashTable").As<Napi::Boolean>().Value() ? 1 : 0;
        }

        if (options.Has("passes"))
        {
            decoderOptions.npasses = options.Get("passes").As<Napi::Number>().Int32Value();
        }

        if (options.Has("subtraction"))
        {
            decoderOptions.subtraction = options.Get("subtraction").As<Napi::Boolean>().Value() ? 1 : 0;
        }

        Napi::Function callback = info[2].As<Napi::Function>();

        // WSPR功能暂不支持（C API未实现，需要future enhancement）
        Napi::Error::New(env, "WSPR decoding is not yet supported in the current bridge architecture. Use standard decode() for supported modes.")
            .ThrowAsJavaScriptException();
        return env.Null();
#endif
    }

    // Pull decoded messages from the queue
    Napi::Value WSJTXLibWrapper::PullMessages(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        Napi::Array results = Napi::Array::New(env);
        uint32_t count = 0;

        // 统一使用C API
        while (true)
        {
            wsjtx_message_t c_msg;
            int has_message = wsjtx_pull_message_(lib_handle_, &c_msg);

            if (has_message <= 0)
            {
                break; // No more messages
            }

            // Convert C message to JavaScript object
            // In MSVC mode, WsjtxMessage is just wsjtx_message_t (a C struct)
            results[count++] = CreateWSJTXMessage(env, c_msg);
        }

        return results;
    }

    // Check if encoding is supported for a mode
    Napi::Value WSJTXLibWrapper::IsEncodingSupported(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        bool supported = (it != MODE_INFO.end()) && it->second.encodingSupported;

        return Napi::Boolean::New(env, supported);
    }

    // Check if decoding is supported for a mode
    Napi::Value WSJTXLibWrapper::IsDecodingSupported(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        bool supported = (it != MODE_INFO.end()) && it->second.decodingSupported;

        return Napi::Boolean::New(env, supported);
    }

    // Get sample rate for a mode
    Napi::Value WSJTXLibWrapper::GetSampleRate(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

#if WSJTX_WINDOWS_MSVC_MODE
        // MSVC mode: Use C API
        wsjtx_mode_t c_mode = static_cast<wsjtx_mode_t>(wsjtxModeVal);
        int sampleRate = wsjtx_get_sample_rate_(c_mode);
        if (sampleRate <= 0)
        {
            // Fallback to MODE_INFO if C API fails
            auto it = MODE_INFO.find(wsjtxModeVal);
            sampleRate = (it != MODE_INFO.end()) ? it->second.sampleRate : 12000;
        }
#else
        // Non-MSVC mode: Use MODE_INFO lookup
        auto it = MODE_INFO.find(wsjtxModeVal);
        int sampleRate = (it != MODE_INFO.end()) ? it->second.sampleRate : 12000;
#endif

        return Napi::Number::New(env, sampleRate);
    }

    // Get transmission duration for a mode
    Napi::Value WSJTXLibWrapper::GetTransmissionDuration(const Napi::CallbackInfo &info)
    {
        Napi::Env env = info.Env();

        if (info.Length() < 1 || !info[0].IsNumber())
        {
            Napi::TypeError::New(env, "Expected mode number").ThrowAsJavaScriptException();
            return env.Null();
        }

        int mode = info[0].As<Napi::Number>().Int32Value();
        wsjtxMode wsjtxModeVal = ConvertToWSJTXMode(mode);

        auto it = MODE_INFO.find(wsjtxModeVal);
        double duration = (it != MODE_INFO.end()) ? it->second.duration : 60.0;

        return Napi::Number::New(env, duration);
    }

    // Helper functions
    wsjtxMode ConvertToWSJTXMode(int mode)
    {
        return static_cast<wsjtxMode>(mode);
    }

    void WSJTXLibWrapper::ValidateMode(Napi::Env env, int mode)
    {
        if (mode < 0 || mode > WSPR)
        {
            throw std::invalid_argument("Invalid mode value");
        }
    }

    void WSJTXLibWrapper::ValidateFrequency(Napi::Env env, int frequency)
    {
        if (frequency < 0 || frequency > 30000000)
        { // 30 MHz max
            throw std::invalid_argument("Invalid frequency value");
        }
    }

    void WSJTXLibWrapper::ValidateThreads(Napi::Env env, int threads)
    {
        if (threads < 1 || threads > 16)
        {
            throw std::invalid_argument("Thread count must be between 1 and 16");
        }
    }

    void WSJTXLibWrapper::ValidateMessage(Napi::Env env, const std::string &message)
    {
        if (message.empty() || message.length() > 22)
        {
            throw std::invalid_argument("Message must be 1-22 characters long");
        }
    }

    std::vector<float> WSJTXLibWrapper::ConvertToFloatArray(Napi::Env env, const Napi::Value& value)
    {
        Napi::Float32Array array = value.As<Napi::Float32Array>();
        size_t length = array.ElementLength();
        float *data = array.Data();

        return WsjTxVector(data, data + length);
    }

    std::vector<short int> WSJTXLibWrapper::ConvertToIntArray(Napi::Env env, const Napi::Value& value)
    {
        Napi::Int16Array array = value.As<Napi::Int16Array>();
        size_t length = array.ElementLength();
        int16_t *data = array.Data();

        return IntWsjTxVector(data, data + length);
    }

    Napi::Object WSJTXLibWrapper::CreateWSJTXMessage(Napi::Env env, const WsjtxMessage &msg)
    {
        Napi::Object result = Napi::Object::New(env);

        result.Set("text", Napi::String::New(env, msg.message));
        result.Set("snr", Napi::Number::New(env, msg.snr));
        result.Set("deltaTime", Napi::Number::New(env, msg.dt));
        result.Set("deltaFrequency", Napi::Number::New(env, msg.freq));
        result.Set("timestamp", Napi::Number::New(env,
                                                  msg.hh * 3600 + msg.min * 60 + msg.sec));
        result.Set("sync", Napi::Number::New(env, msg.sync));

        return result;
    }

    // Async Workers Implementation

    // Base async worker class
    AsyncWorkerBase::AsyncWorkerBase(Napi::Function &callback)
        : Napi::AsyncWorker(callback) {}

    // Decode Worker constructors
    DecodeWorker::DecodeWorker(Napi::Function &callback,
                               wsjtx_handle_t handle,
                               wsjtx_decode_fn decode_fn,
                               wsjtx_pull_message_fn pull_fn,
                               wsjtxMode mode,
                               const std::vector<float> &audioData,
                               int frequency,
                               int threads)
        : AsyncWorkerBase(callback),
          handle_(handle),
          decode_fn_(decode_fn),
          pull_fn_(pull_fn),
          mode_(mode),
          floatData_(audioData),
          frequency_(frequency),
          threads_(threads),
          useFloat_(true) {}

    DecodeWorker::DecodeWorker(Napi::Function &callback,
                               wsjtx_handle_t handle,
                               wsjtx_decode_fn decode_fn,
                               wsjtx_pull_message_fn pull_fn,
                               wsjtxMode mode,
                               const std::vector<short int> &audioData,
                               int frequency,
                               int threads)
        : AsyncWorkerBase(callback),
          handle_(handle),
          decode_fn_(decode_fn),
          pull_fn_(pull_fn),
          mode_(mode),
          intData_(audioData),
          frequency_(frequency),
          threads_(threads),
          useFloat_(false) {}

    EncodeWorker::EncodeWorker(Napi::Function &callback,
                                wsjtx_handle_t handle,
                                wsjtx_encode_fn encode_fn,
                                wsjtx_get_max_samples_fn get_max_samples_fn,
                                wsjtxMode mode,
                                const std::string &message,
                                int frequency,
                                int threads)
        : AsyncWorkerBase(callback),
          handle_(handle),
          encode_fn_(encode_fn),
          get_max_samples_fn_(get_max_samples_fn),
          mode_(mode),
          message_(message),
          frequency_(frequency),
          threads_(threads) {}

    void DecodeWorker::Execute()
    {
        try
        {
            // 统一使用C API
            wsjtx_mode_t c_mode = static_cast<wsjtx_mode_t>(mode_);

            // Prepare audio data (only float supported in C API)
            std::vector<float> audioData;
            if (useFloat_)
            {
                audioData = floatData_;
            }
            else
            {
                // Convert int16 to float
                audioData.resize(intData_.size());
                for (size_t i = 0; i < intData_.size(); ++i)
                {
                    audioData[i] = static_cast<float>(intData_[i]) / 32768.0f;
                }
            }

            // Call C API decode function
            int result = decode_fn_(
                handle_,
                c_mode,
                audioData.data(),
                static_cast<int>(audioData.size()),
                frequency_,
                threads_
            );

            if (result != WSJTX_OK)
            {
                SetError("Decode failed with error code: " + std::to_string(result));
                return;
            }

            // Note: Messages are stored in the bridge's internal queue
            // User will call pullMessages() later to retrieve them
        }
        catch (const std::exception &e)
        {
            SetError(e.what());
        }
    }

    void DecodeWorker::OnOK()
    {
        Napi::Env env = Env();
        Callback().Call({env.Null(), Napi::Boolean::New(env, true)});
    }

    void EncodeWorker::Execute()
    {
        try
        {
            // 统一使用C API
            wsjtx_mode_t c_mode = static_cast<wsjtx_mode_t>(mode_);

            // Get maximum sample count for this mode
            int max_samples = get_max_samples_fn_(c_mode);
            if (max_samples <= 0)
            {
                SetError("Failed to get maximum sample count for mode");
                return;
            }

            // Allocate output buffer
            std::vector<float> output_buffer(max_samples);
            int actual_samples = max_samples;

            // Call C API encode function
            int result = encode_fn_(
                handle_,
                c_mode,
                message_.c_str(),
                frequency_,
                output_buffer.data(),
                &actual_samples
            );

            if (result != WSJTX_OK)
            {
                SetError("Encode failed with error code: " + std::to_string(result));
                return;
            }

            // Resize and store audio data
            audioData_.resize(actual_samples);
            std::copy(output_buffer.begin(),
                     output_buffer.begin() + actual_samples,
                     audioData_.begin());

            // C API doesn't modify the message, so messageSent_ is same as input
            messageSent_ = message_;
        }
        catch (const std::exception &e)
        {
            SetError(e.what());
        }
    }

    void EncodeWorker::OnOK()
    {
        Napi::Env env = Env();

        // Create Float32Array for audio data
        Napi::Float32Array audioArray = Napi::Float32Array::New(env, audioData_.size());
        std::copy(audioData_.begin(), audioData_.end(), audioArray.Data());

        Napi::Object result = Napi::Object::New(env);
        result.Set("audioData", audioArray);
        result.Set("messageSent", Napi::String::New(env, messageSent_));

        Callback().Call({env.Null(), result});
    }

    // WSPR Decode Worker (currently disabled - not implemented in bridge API)
    WSPRDecodeWorker::WSPRDecodeWorker(Napi::Function &callback,
                                       const std::vector<std::complex<float>> &iqData,
                                       const decoder_options &options)
        : AsyncWorkerBase(callback), iqData_(iqData), options_(options) {}

    void WSPRDecodeWorker::Execute()
    {
        // WSPR is not implemented in the bridge API yet
        SetError("WSPR decoding is not supported in the current bridge architecture");
    }

    void WSPRDecodeWorker::OnOK()
    {
        Napi::Env env = Env();

        Napi::Array resultsArray = Napi::Array::New(env, results_.size());

        for (size_t i = 0; i < results_.size(); i++)
        {
            const auto &result = results_[i];
            Napi::Object obj = Napi::Object::New(env);

            obj.Set("frequency", Napi::Number::New(env, result.freq));
            obj.Set("sync", Napi::Number::New(env, result.sync));
            obj.Set("snr", Napi::Number::New(env, result.snr));
            obj.Set("deltaTime", Napi::Number::New(env, result.dt));
            obj.Set("drift", Napi::Number::New(env, result.drift));
            obj.Set("jitter", Napi::Number::New(env, result.jitter));
            obj.Set("message", Napi::String::New(env, result.message));
            obj.Set("callsign", Napi::String::New(env, result.call));
            obj.Set("locator", Napi::String::New(env, result.loc));
            obj.Set("power", Napi::String::New(env, result.pwr));
            obj.Set("cycles", Napi::Number::New(env, result.cycles));

            resultsArray[i] = obj;
        }

        Callback().Call({env.Null(), resultsArray});
    }

    // AudioConvertWorker implementation
    void AudioConvertWorker::Execute()
    {
        if (fromFloat_)
        {
            if (target_ == Target::Float32)
            {
                // No-op copy
                floatOut_ = floatInput_;
            }
            else
            {
                intOut_.resize(floatInput_.size());
                for (size_t i = 0; i < floatInput_.size(); ++i)
                {
                    float v = floatInput_[i];
                    // Clamp then scale
                    if (v > 1.0f) v = 1.0f;
                    else if (v < -1.0f) v = -1.0f;
                    intOut_[i] = static_cast<short int>((std::max)(-32768, (std::min)(32767, static_cast<int>(std::lround(v * 32768.0f)))));
                }
            }
        }
        else
        {
            if (target_ == Target::Int16)
            {
                // No-op copy
                intOut_ = intInput_;
            }
            else
            {
                floatOut_.resize(intInput_.size());
                for (size_t i = 0; i < intInput_.size(); ++i)
                {
                    floatOut_[i] = static_cast<float>(intInput_[i]) / 32768.0f;
                }
            }
        }
    }

    void AudioConvertWorker::OnOK()
    {
        Napi::Env env = Env();
        if (target_ == Target::Float32)
        {
            Napi::Float32Array out = Napi::Float32Array::New(env, floatOut_.size());
            std::copy(floatOut_.begin(), floatOut_.end(), out.Data());
            Callback().Call({ env.Null(), out });
        }
        else
        {
            Napi::Int16Array out = Napi::Int16Array::New(env, intOut_.size());
            std::copy(intOut_.begin(), intOut_.end(), out.Data());
            Callback().Call({ env.Null(), out });
        }
    }

    // ==================== 跨平台Bridge加载实现 ====================

    std::string WSJTXLibWrapper::GetBridgePath()
    {
#ifdef _WIN32
        // Windows: 获取.node模块路径
        wchar_t modulePath[MAX_PATH];
        HMODULE hModule = nullptr;

        // 获取当前模块句柄
        GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&WSJTXLibWrapper::Init),
            &hModule);

        if (!hModule || GetModuleFileNameW(hModule, modulePath, MAX_PATH) == 0)
        {
            throw std::runtime_error("Failed to get module path on Windows");
        }

        // 转换为窄字符串
        std::wstring wideDir(modulePath);
        size_t lastSlash = wideDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos)
        {
            wideDir = wideDir.substr(0, lastSlash);
        }

        // 转换为UTF-8
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, wideDir.c_str(), -1, NULL, 0, NULL, NULL);
        std::string dir(size_needed - 1, 0);
        WideCharToMultiByte(CP_UTF8, 0, wideDir.c_str(), -1, &dir[0], size_needed, NULL, NULL);

        return dir + "\\wsjtx_bridge.dll";
#else
        // Unix (Linux/macOS): 使用dladdr获取.node路径
        Dl_info info;
        if (dladdr((void*)&WSJTXLibWrapper::Init, &info) == 0)
        {
            throw std::runtime_error("Failed to get module path on Unix");
        }

        std::string modulePath(info.dli_fname);
        size_t lastSlash = modulePath.find_last_of('/');
        std::string dir = (lastSlash != std::string::npos) ? modulePath.substr(0, lastSlash) : ".";

#ifdef __APPLE__
        return dir + "/wsjtx_bridge.dylib";
#else
        return dir + "/wsjtx_bridge.so";
#endif
#endif
    }

    void WSJTXLibWrapper::LoadBridge()
    {
        try
        {
            // 获取bridge库路径
            std::string bridgePath = GetBridgePath();

#ifdef _WIN32
            // Windows: 转换为宽字符串
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, bridgePath.c_str(), -1, NULL, 0);
            std::wstring widePath(size_needed - 1, 0);
            MultiByteToWideChar(CP_UTF8, 0, bridgePath.c_str(), -1, &widePath[0], size_needed);

            // 设置DLL搜索目录
            size_t lastSlash = widePath.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos)
            {
                std::wstring dir = widePath.substr(0, lastSlash);
                SetDllDirectoryW(dir.c_str());
            }

            // 加载库
            dll_handle_ = LOAD_LIBRARY(widePath.c_str());
            if (!dll_handle_)
            {
                DWORD error = GetLastError();
                throw std::runtime_error("Failed to load wsjtx_bridge.dll (error: " + std::to_string(error) + ")");
            }

            // 恢复DLL搜索路径
            SetDllDirectoryW(nullptr);
#else
            // Unix: 直接加载
            dll_handle_ = LOAD_LIBRARY(bridgePath.c_str());
            if (!dll_handle_)
            {
                const char* error = dlerror();
                throw std::runtime_error(std::string("Failed to load bridge library: ") + (error ? error : "unknown error"));
            }
#endif

            // 加载函数符号（跨平台）
            wsjtx_create_ = reinterpret_cast<wsjtx_create_fn>(GET_SYMBOL(dll_handle_, "wsjtx_create"));
            wsjtx_destroy_ = reinterpret_cast<wsjtx_destroy_fn>(GET_SYMBOL(dll_handle_, "wsjtx_destroy"));
            wsjtx_decode_ = reinterpret_cast<wsjtx_decode_fn>(GET_SYMBOL(dll_handle_, "wsjtx_decode"));
            wsjtx_pull_message_ = reinterpret_cast<wsjtx_pull_message_fn>(GET_SYMBOL(dll_handle_, "wsjtx_pull_message"));
            wsjtx_encode_ = reinterpret_cast<wsjtx_encode_fn>(GET_SYMBOL(dll_handle_, "wsjtx_encode"));
            wsjtx_get_sample_rate_ = reinterpret_cast<wsjtx_get_sample_rate_fn>(GET_SYMBOL(dll_handle_, "wsjtx_get_sample_rate"));
            wsjtx_get_max_samples_ = reinterpret_cast<wsjtx_get_max_samples_fn>(GET_SYMBOL(dll_handle_, "wsjtx_get_max_samples"));

            // 验证所有函数都加载成功
            if (!wsjtx_create_ || !wsjtx_destroy_ || !wsjtx_decode_ ||
                !wsjtx_pull_message_ || !wsjtx_encode_ ||
                !wsjtx_get_sample_rate_ || !wsjtx_get_max_samples_)
            {
                UnloadBridge();
                throw std::runtime_error("Failed to load one or more required functions from bridge library");
            }

            // 创建库实例
            lib_handle_ = wsjtx_create_();
            if (!lib_handle_)
            {
                UnloadBridge();
                throw std::runtime_error("Failed to create WSJTX library instance");
            }
        }
        catch (const std::exception &e)
        {
#ifdef _WIN32
            SetDllDirectoryW(nullptr);
#endif
            throw;
        }
    }

    void WSJTXLibWrapper::UnloadBridge()
    {
        // 销毁库实例
        if (lib_handle_ && wsjtx_destroy_)
        {
            wsjtx_destroy_(lib_handle_);
            lib_handle_ = nullptr;
        }

        // 卸载动态库
        if (dll_handle_)
        {
            FREE_LIBRARY(dll_handle_);
            dll_handle_ = nullptr;
        }

        // 清空函数指针
        wsjtx_create_ = nullptr;
        wsjtx_destroy_ = nullptr;
        wsjtx_decode_ = nullptr;
        wsjtx_pull_message_ = nullptr;
        wsjtx_encode_ = nullptr;
        wsjtx_get_sample_rate_ = nullptr;
        wsjtx_get_max_samples_ = nullptr;
    }

    // 析构函数（跨平台统一）
    WSJTXLibWrapper::~WSJTXLibWrapper()
    {
        UnloadBridge();
    }

    // Module initialization
    Napi::Object Init(Napi::Env env, Napi::Object exports)
    {
        return WSJTXLibWrapper::Init(env, exports);
    }

    NODE_API_MODULE(wsjtx_lib, Init)

} // namespace wsjtx_nodejs
