#include "FFmpegAccel.h"
#include "FFmpegWrapper.hpp"
#include "adaption/Logging.h"

#ifdef _NVCODEC
#include <dynlink_cuda.h>
#include <libavutil/hwcontext_cuda.h>
#endif

#ifdef _WIN32
#include <libavutil/hwcontext_dxva2.h>
#include <libavutil/hwcontext_d3d11va.h>
#endif

#ifdef __APPLE__
#include <libavutil/hwcontext_videotoolbox.h>
#endif

#if defined(ANDROID) || defined(__ANDROID__)
#define _MEDIA_CODEC
#include <libavutil/hwcontext_mediacodec.h>
#endif

#ifdef _VAAPI
#include <libavutil/hwcontext_vaapi.h>
#endif

namespace ffmpeg
{
class FFmpegLogHacker
{
private:
    static void Log(void*, int level, const char* fmt, va_list args)
    {
        if (level > AV_LOG_INFO)
        {
            return;
        }
        else
        {
            std::array<char, 1024> formatted;
            int length = vsnprintf(formatted.data(), formatted.size(), fmt, args);
            if (length < 0)
            {
                LoggingMessage(LogLevel::WARN, "FFmepg Log parse error: {}", fmt);
            }
            else if (length < 1)
            {
                return;
            }
            else if (length < 1024)
            {
                bool line = formatted[length - 1] == '\n';
                switch (level)
                {
                case AV_LOG_PANIC:
                    LoggingMessage(LogLevel::FATAL, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_FATAL:
                    LoggingMessage(LogLevel::FAULT, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_ERROR:
                    LoggingMessage(LogLevel::FAULT, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_WARNING:
                    LoggingMessage(LogLevel::WARN, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_INFO:
                    LoggingMessage(LogLevel::INFO, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_VERBOSE:
                    LoggingMessage(LogLevel::DEBUG, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_DEBUG:
                    LoggingMessage(LogLevel::DEBUG, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                case AV_LOG_TRACE:
                    LoggingMessage(LogLevel::DEBUG, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                default:
                    LoggingMessage(LogLevel::DEBUG, line ? "#FFmpeg {}" : "#FFmpeg {}\n", fmt::string_view(formatted.data(), static_cast<size_t>(length)));
                    break;
                }
            }
            else
            {
                LoggingMessage(LogLevel::WARN, "FFmepg Log parse no resource: {}", fmt);
            }
        }
    }

private:
    FFmpegLogHacker()
    {
        av_log_set_level(AV_LOG_INFO);
        av_log_set_callback(&FFmpegLogHacker::Log);
    }

private:
    static FFmpegLogHacker s_log;
};
FFmpegLogHacker FFmpegLogHacker::s_log = FFmpegLogHacker();

//////////////////////////////////////////////////////////////////////////
AVBufferRef* CreateCUDAContext(const decltype(NVIVideoAccelerate::context)& context)
{
    AVBufferRef* pHWRef = nullptr;
    if (context.cuda.context == nullptr)
    {
        std::string strDevice = context.cuda.device >= 0 ? fmt::format("{}", context.cuda.device) : "";
        int nCreate = av_hwdevice_ctx_create(&pHWRef, AV_HWDEVICE_TYPE_CUDA, strDevice.c_str(), nullptr,
                                             context.cuda.use_primary_context ? AV_CUDA_USE_PRIMARY_CONTEXT : 0);
        if (nCreate < 0)
        {
            LOG_WARNING("av_hwdevice_ctx_create AV_HWDEVICE_TYPE_CUDA failed {}, {}", nCreate, av_errstr(nCreate));
        }
    }
    else
    {
#ifdef CUDA_VERSION
        pHWRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA);
        if (pHWRef && pHWRef->data)
        {
            AVHWDeviceContext* pHWContext = (AVHWDeviceContext*)pHWRef->data;
            AVCUDADeviceContext* pDeviceContext = (AVCUDADeviceContext*)pHWContext->hwctx;
            if (pDeviceContext)
            {
                pDeviceContext->cuda_ctx = (CUcontext)context.cuda.context;
                int nInit = av_hwdevice_ctx_init(pHWRef);
                if (nInit < 0)
                {
                    av_buffer_unref(&pHWRef);
                    LOG_WARNING("av_hwdevice_ctx_init failed {}, {}.", nInit, av_errstr(nInit));
                }
            }
            else
            {
                av_buffer_unref(&pHWRef);
                LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) failed, has not valid data.");
            }
        }
        else
        {
            LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_CUDA) failed.");
        }
#endif
    }
    return pHWRef;
}

AVBufferRef* CreateDXVA2Context(const decltype(NVIVideoAccelerate::context)& context)
{
    AVBufferRef* pHWRef = nullptr;
    if (context.dxva2.manager == nullptr)
    {
        int nCreate = av_hwdevice_ctx_create(&pHWRef, AV_HWDEVICE_TYPE_DXVA2, nullptr, nullptr, 0);
        if (nCreate < 0)
        {
            LOG_WARNING("av_hwdevice_ctx_create AV_HWDEVICE_TYPE_DXVA2 failed {}, {}", nCreate, av_errstr(nCreate));
        }
    }
    else
    {
#ifdef _WIN32
        pHWRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DXVA2);
        if (pHWRef && pHWRef->data)
        {
            AVHWDeviceContext* pHWContext = (AVHWDeviceContext*)pHWRef->data;
            AVDXVA2DeviceContext* pDeviceContext = (AVDXVA2DeviceContext*)pHWContext->hwctx;
            if (pDeviceContext)
            {
                pDeviceContext->devmgr = (IDirect3DDeviceManager9*)context.cuda.context;
                int nInit = av_hwdevice_ctx_init(pHWRef);
                if (nInit < 0)
                {
                    av_buffer_unref(&pHWRef);
                    LOG_WARNING("av_hwdevice_ctx_init failed {}, {}.", nInit, av_errstr(nInit));
                }
            }
            else
            {
                av_buffer_unref(&pHWRef);
                LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DXVA2) failed, has not valid data.");
            }
        }
        else
        {
            LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_DXVA2) failed.");
        }
#endif
    }
    return pHWRef;
}

AVBufferRef* CreateD3D11VAContext(const decltype(NVIVideoAccelerate::context)& context)
{
    AVBufferRef* pHWRef = nullptr;
    if (context.d3d11va.d3d11_device == nullptr && context.d3d11va.video_device == nullptr)
    {
        int nCreate = av_hwdevice_ctx_create(&pHWRef, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0);
        if (nCreate < 0)
        {
            LOG_WARNING("av_hwdevice_ctx_create AV_HWDEVICE_TYPE_D3D11VA failed {}, {}", nCreate, av_errstr(nCreate));
        }
    }
    else
    {
#ifdef _WIN32
        pHWRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
        if (pHWRef && pHWRef->data)
        {
            AVHWDeviceContext* pHWContext = (AVHWDeviceContext*)pHWRef->data;
            AVD3D11VADeviceContext* pD3D11Context = (AVD3D11VADeviceContext*)pHWContext->hwctx;
            pD3D11Context->device = (ID3D11Device*)context.d3d11va.d3d11_device;
            pD3D11Context->device_context = (ID3D11DeviceContext*)context.d3d11va.d3d11_context;
            if (context.d3d11va.video_device == nullptr)
            {
                pD3D11Context->device->QueryInterface(__uuidof(ID3D11VideoDevice), (void**)&pD3D11Context->video_device);
                pD3D11Context->device_context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pD3D11Context->video_context);
            }
            else
            {
                pD3D11Context->video_device = (ID3D11VideoDevice*)context.d3d11va.video_device;
                if (context.d3d11va.video_context == nullptr)
                {
                    pD3D11Context->device_context->QueryInterface(__uuidof(ID3D11VideoContext), (void**)&pD3D11Context->video_context);
                }
                else
                {
                    pD3D11Context->video_context = (ID3D11VideoContext*)context.d3d11va.video_context;
                }
            }
            pD3D11Context->lock_ctx = context.d3d11va.mutex;
            pD3D11Context->lock = context.d3d11va.lock;
            pD3D11Context->unlock = context.d3d11va.unlock;
            if (!pD3D11Context->video_device || !pD3D11Context->video_context)
            {
                av_buffer_unref(&pHWRef);
                LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA) failed, has not valid data.");
            }
            else
            {
                pD3D11Context->device->AddRef();
                pD3D11Context->device_context->AddRef();
            }
        }
        else
        {
            LOG_WARNING("av_hwdevice_ctx_alloc(CreateD3D11VAContext) failed.");
        }
#endif
    }
    return pHWRef;
}

AVBufferRef* CreateVideoToolboxContext(const decltype(NVIVideoAccelerate::context)& context)
{
    (void)context;
    AVBufferRef* pHWRef = nullptr;
    int nCreate = av_hwdevice_ctx_create(&pHWRef, AV_HWDEVICE_TYPE_VIDEOTOOLBOX, nullptr, nullptr, 0);
    if (nCreate < 0)
    {
        LOG_WARNING("av_hwdevice_ctx_create AV_HWDEVICE_TYPE_VIDEOTOOLBOX failed {}, {}", nCreate, av_errstr(nCreate));
    }
    return pHWRef;
}

AVBufferRef* CreateMediaCodecContext(const decltype(NVIVideoAccelerate::context)& context)
{
    AVBufferRef* pHWRef = nullptr;
#ifdef _MEDIA_CODEC
    pHWRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC);
    if (pHWRef && pHWRef->data)
    {
        AVHWDeviceContext* pHWContext = (AVHWDeviceContext*)pHWRef->data;
        AVMediaCodecDeviceContext* pMediaCodecDeviceContext = (AVMediaCodecDeviceContext*)pHWContext->hwctx;
        if (pMediaCodecDeviceContext)
        {
            pMediaCodecDeviceContext->surface = context.media_codec.surface;
        }
        else
        {
            av_buffer_unref(&pHWRef);
            LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC) failed, has not valid data.");
        }
    }
    else
    {
        LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_MEDIACODEC) failed.");
    }
#else
    (void)context;
#endif
    return pHWRef;
}

AVBufferRef* CreateVAAPIContext(const decltype(NVIVideoAccelerate::context)& context)
{
    AVBufferRef* pHWRef = nullptr;
    if (context.vaapi.display == nullptr)
    {
        const char* device = nullptr;
        AVDictionary* opts = nullptr;
#ifdef _WIN32
        av_dict_set(&opts, "connection_type", "win32", 0);
        std::string strDevice;
        if (context.vaapi.adapter >= 0)
        {
            strDevice = fmt::format("{}", context.vaapi.adapter);
            device = strDevice.c_str();
        }
#else
        if (context.vaapi.drm)
        {
            device = context.vaapi.x11;
            av_dict_set(&opts, "connection_type", "x11", 0);
        }
        else
        {
            device = context.vaapi.drm;
            av_dict_set(&opts, "connection_type", "drm", 0);
        }
#endif
        int nCreate = av_hwdevice_ctx_create(&pHWRef, AV_HWDEVICE_TYPE_VAAPI, device, opts, 0);
        if (nCreate < 0)
        {
            LOG_WARNING("av_hwdevice_ctx_create AV_HWDEVICE_TYPE_VAAPI failed {}, {}", nCreate, av_errstr(nCreate));
        }
        av_dict_free(&opts);
    }
    else
    {
#ifdef _VAAPI
        pHWRef = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI);
        if (pHWRef && pHWRef->data)
        {
            AVHWDeviceContext* pHWContext = (AVHWDeviceContext*)pHWRef->data;
            AVVAAPIDeviceContext* pVAAPIContext = (AVVAAPIDeviceContext*)pHWContext->hwctx;
            if (pVAAPIContext)
            {
                pVAAPIContext->display = (VADisplay)context.vaapi.display;
                int nInit = av_hwdevice_ctx_init(pHWRef);
                if (nInit < 0)
                {
                    av_buffer_unref(&pHWRef);
                    LOG_WARNING("av_hwdevice_ctx_init failed {}, {}.", nInit, av_errstr(nInit));
                }
            }
            else
            {
                av_buffer_unref(&pHWRef);
                LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI) failed, has not valid data.");
            }
        }
        else
        {
            LOG_WARNING("av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_VAAPI) failed.");
        }
#endif
    }
    return pHWRef;
}

AVBufferRef* CreateHWContext(const NVIVideoAccelerate* pAccel)
{
    if (pAccel == nullptr)
    {
        return nullptr;
    }
    auto eAVHWDeviceType = ToAVHWDeviceType(static_cast<NVIAccelType>(pAccel->type));
    switch (eAVHWDeviceType)
    {
    case AV_HWDEVICE_TYPE_CUDA: return CreateCUDAContext(pAccel->context);
    case AV_HWDEVICE_TYPE_DXVA2: return CreateDXVA2Context(pAccel->context);
    case AV_HWDEVICE_TYPE_D3D11VA: return CreateD3D11VAContext(pAccel->context);
    case AV_HWDEVICE_TYPE_VIDEOTOOLBOX: return CreateVideoToolboxContext(pAccel->context);
    case AV_HWDEVICE_TYPE_MEDIACODEC: return CreateMediaCodecContext(pAccel->context);
    case AV_HWDEVICE_TYPE_VAAPI: return CreateVAAPIContext(pAccel->context);
    default: break;
    }
    return nullptr;
}

#ifdef CUDA_VERSION
void* CudaContext(AVHWFramesContext* pHWFramesContext)
{
    if (pHWFramesContext)
    {
        AVHWDeviceContext* pDeviceContext = reinterpret_cast<AVHWDeviceContext*>(pHWFramesContext->device_ctx);
        if (pDeviceContext && pDeviceContext->type == AV_HWDEVICE_TYPE_CUDA)
        {
            AVCUDADeviceContext* pCudaContext = reinterpret_cast<AVCUDADeviceContext*>(pDeviceContext->hwctx);
            return pCudaContext->cuda_ctx;
        }
    }
    return nullptr;
}
#endif  //CUDA_VERSION

}  //namespace ffmpeg
