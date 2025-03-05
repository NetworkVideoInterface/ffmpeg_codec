#pragma once

#include <NVI/Codec.h>

#define EXTERN_C extern "C"

#ifdef _MSC_VER
#define API EXTERN_C __declspec(dllexport)  // 只使用动态加载，不需要import
#else
#define API EXTERN_C __attribute((visibility("default")))
#endif

API NVIVideoDecode VideoDecodeAlloc(uint32_t codec);

API NVIAudioDecode AudioDecodeAlloc(uint32_t codec);

API void SetLogging(void (*logging)(int level, const char* message, unsigned int length));
