#include "FFVideoDecoder.h"
#include "FFmpegAccel.h"
#include "FFmpegWrapper.hpp"
#include "adaption/Logging.h"

using namespace ffmpeg;

inline NVIBufferType GetHWBufferType(NVIAccelType eAccelType)
{
    switch (eAccelType)
    {
    case NVIAccel_NVCodec: return NVIBuffer_CUDA;
    case NVIAccel_DXVA2: return NVIBuffer_D3DSurface9;
    case NVIAccel_D3D11VA: return NVIBuffer_D3D11Texture2D;
    case NVIAccel_VideoToolbox: return NVIBuffer_CVPixelBufferRef;
    case NVIAccel_MediaCodec: return NVIBuffer_MediaCodecBuffer;
    case NVIAccel_VAAPI: return NVIBuffer_VASurfaceID;
    default: return NVIBuffer_HOST;
    }
}

AVPixelFormat GetHWFormat(AVCodecContext* ctx, const enum AVPixelFormat* fmts)
{
    FFVideoDecoder* pDelegate = (FFVideoDecoder*)ctx->opaque;
    if (pDelegate)
    {
        AVPixelFormat fmt = (AVPixelFormat)pDelegate->HWPixelFormat();
        for (auto p = fmts; *p != -1; p++)
        {
            if (*p == fmt)
            {
                return fmt;
            }
        }
    }
    return fmts ? *fmts : AV_PIX_FMT_NONE;
}

AVPixelFormat GetHWDownloadFormat(AVBufferRef* ctx)
{
    AVPixelFormat format = AV_PIX_FMT_NONE;
    AVPixelFormat* arrFormats = nullptr;
    int nGetFormats = av_hwframe_transfer_get_formats(ctx, AVHWFrameTransferDirection::AV_HWFRAME_TRANSFER_DIRECTION_FROM, &arrFormats, 0);
    if (nGetFormats >= 0 && arrFormats)
    {
        format = arrFormats[0];
        av_freep(&arrFormats);
    }
    else
    {
        LOG_ERROR("av_hwframe_transfer_get_formats failed {}, {}.", nGetFormats, av_errstr(nGetFormats));
    }
    return format;
}

#ifdef _NVCODEC
#define FFNV_LOG_FUNC(ctx, fmt, ...)
#define FFNV_DEBUG_LOG_FUNC(ctx, fmt, ...)
#include <dynlink_loader.h>
#define ALIGN(x, a) (((x) + (a)-1) & ~((a)-1))

class CudaHostModule
{
public:
    static CudaHostFunctions* Functions()
    {
        static CudaHostModule m;
        return m.m_pFunctions;
    }

private:
    CudaHostModule()
    {
        cuda_host_load_functions(&m_pFunctions, nullptr);
        LOG_INFO("Load cuda host functions {}.", (void*)m_pFunctions);
    }
    ~CudaHostModule()
    {
        cuda_host_free_functions(&m_pFunctions);
    }

private:
    CudaHostFunctions* m_pFunctions;
};

static void FreeCudaHost(void* opaque, uint8_t* data)
{
    if (opaque && data)
    {
        CudaHostFunctions* cuda = reinterpret_cast<CudaHostFunctions*>(opaque);
        cuda->cuMemFreeHost(data);
        LOG_INFO("Free cuda host {}.", (void*)data);
    }
}

static AVFrame* AllocHostAVFrame(AVBufferRef* ref, int width, int height)
{
    if (ref == nullptr)
    {
        return nullptr;
    }
    CUcontext context = reinterpret_cast<CUcontext>(CudaContext(reinterpret_cast<AVHWFramesContext*>(ref->data)));
    if (context == nullptr)
    {
        return nullptr;
    }
    // code reference get_video_buffer
    if (av_image_check_size(width, height, 0, nullptr) < 0)
    {
        LOG_ERROR("Alloc host av_image_check_size({},{}) failed.", width, height);
        return nullptr;
    }
    AVFrame* pFrame = av_frame_alloc();
    if (pFrame == nullptr)
    {
        LOG_ERROR("Alloc host av_frame_alloc failed.");
        return nullptr;
    }
    const AVPixelFormat format = GetHWDownloadFormat(ref);
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format);
    if (desc == nullptr)
    {
        LOG_ERROR("Alloc host not found pixel format[{}] descriptor.", (int32_t)format);
        return pFrame;
    }
    CudaHostFunctions* cuda = CudaHostModule::Functions();
    if (cuda)
    {
        // for page-locked host memory.
        pFrame->format = format;
        pFrame->width = width;
        pFrame->height = height;
        int nFill = av_image_fill_linesizes(pFrame->linesize, format, FFALIGN(width, 32));
        if (nFill < 0)
        {
            LOG_ERROR("Alloc host call av_image_fill_linesizes failed {}.", nFill);
            return pFrame;
        }
        ptrdiff_t linesizes[4]{};
        for (int i = 0; i < 4; ++i)
        {
            linesizes[i] = pFrame->linesize[i];
        }
        const int padding = FFALIGN(height, 32);
        size_t sizes[4]{};
        nFill = av_image_fill_plane_sizes(sizes, format, padding, linesizes);
        if (nFill < 0)
        {
            LOG_ERROR("Alloc host call av_image_fill_plane_sizes failed {}.", nFill);
            return pFrame;
        }
        size_t bytes = 0ull;
        for (int i = 0; i < 4; ++i)
        {
            bytes += sizes[i];
        }
        uint8_t* pMem = nullptr;
        cuda->cuCtxPushCurrent(context);
        CUresult result = cuda->cuMemHostAlloc(reinterpret_cast<void**>(&pMem), bytes, 0);
        if (result == CUDA_SUCCESS && pMem)
        {
            LOG_INFO("Cuda host alloc {}@{}.", (void*)pMem, bytes);
            pFrame->buf[0] = av_buffer_create(pMem, bytes, &FreeCudaHost, cuda, 0);
            nFill = av_image_fill_pointers(pFrame->data, format, padding, pMem, pFrame->linesize);
            if (nFill < 0)
            {
                LOG_ERROR("Alloc host call av_image_fill_pointers failed {}.", nFill);
            }
        }
        else
        {
            LOG_ERROR("Cuda host alloc failed {}.", (int32_t)result);
        }
        cuda->cuCtxPopCurrent(nullptr);
    }
    return pFrame;
}
#else
#define AllocHostAVFrame(...) (nullptr)
#endif

FFVideoDecoder::FFVideoDecoder()
    : m_pDecoderContext(nullptr)
    , m_nHWPixelFormat(-1)
    , m_eOutBufferType(NVIBuffer_HOST)
    , m_pLastFrame(nullptr, &FreeAVFrame)
    , m_pHostFrame(nullptr, &FreeAVFrame)
{
}

FFVideoDecoder::~FFVideoDecoder()
{
    Release();
}

bool FFVideoDecoder::Config(const NVIVideoCodecParam& param)
{
    Release();
    auto pDecoder = avcodec_find_decoder(ToAVCodecID(param.codec));
    if (pDecoder == nullptr)
    {
        return false;
    }
    m_pDecoderContext = avcodec_alloc_context3(pDecoder);
    if (m_pDecoderContext)
    {
        if (!HWAccelContextInit(param.accel))
        {
            avcodec_free_context(&m_pDecoderContext);
            return false;
        }
        if (m_pDecoderContext->codec)
        {
            LOG_NOTICE("FFVideoDecoder init {}, {}.", m_pDecoderContext->codec->name, m_pDecoderContext->codec->long_name);
        }
        int nOpen = avcodec_open2(m_pDecoderContext, nullptr, nullptr);
        if (nOpen == 0)
        {
            return true;
        }
        else
        {
            LOG_ERROR("FFVideoDecoder avcodec_open2 failed {}, {}.", nOpen, av_errstr(nOpen));
            avcodec_free_context(&m_pDecoderContext);
        }
    }
    return false;
}

bool FFVideoDecoder::Decoding(const NVIVideoEncodedPacket& packet, const Output& output)
{
    if (m_pDecoderContext == nullptr)
    {
        return false;
    }
    if (avcodec_is_open(m_pDecoderContext))
    {
        auto pPacket(AllocAVPacket());
        if (pPacket == nullptr)
        {
            return false;
        }
        pPacket->data = (uint8_t*)packet.buffer.bytes;
        pPacket->size = (int)packet.buffer.size;
        pPacket->pts = packet.info.tick.value;
        pPacket->dts = pPacket->pts;
        av_frame_unref(m_pLastFrame.get());
        int nSend = avcodec_send_packet(m_pDecoderContext, pPacket.get());
        if (nSend == 0)
        {
            int nRecv = 0;
            uint32_t uOut = 0U;
            while (nRecv >= 0)
            {
                auto pFrame = AllocAVFrame();
                if (pFrame == nullptr)
                {
                    LOG_ERROR("av_frame_alloc failed!");
                    return false;
                }
                nRecv = avcodec_receive_frame(m_pDecoderContext, pFrame.get());
                if (nRecv >= 0)
                {
                    ++uOut;
                    m_pLastFrame = std::move(pFrame);
                    OutputLastFrame(packet.info, output);
                }
                else
                {
                    if (nRecv == AVERROR(EAGAIN))
                    {
                        if (uOut == 0u)
                        {
                            LOG_WARNING("FFVideoDecoder avcodec_receive_frame delay!!!!");
                        }
                        break;
                    }
                    else
                    {
                        LOG_ERROR("FFVideoDecoder avcodec_receive_frame failed {}, {}.", nRecv, av_errstr(nRecv));
                        return false;
                    }
                }
            }
        }
        else
        {
            LOG_ERROR("FFVideoDecoder avcodec_send_packet failed {}, {}.", nSend, av_errstr(nSend));
            return false;
        }
        return true;
    }
    return false;
}

bool FFVideoDecoder::OutputLastFrame(const NVIImageInfo& info, const Output& output)
{
    if (m_pLastFrame && output)
    {
        AVFrame* pOutFrame = nullptr;
        if (m_nHWPixelFormat == m_pLastFrame->format)
        {
            if (m_eOutBufferType == NVIBuffer_HOST)
            {
                // download
                if (m_pHostFrame == nullptr || (m_pLastFrame->width != m_pHostFrame->width || m_pLastFrame->height != m_pHostFrame->height))
                {
                    if (m_pLastFrame->format == AV_PIX_FMT_CUDA)
                    {
                        m_pHostFrame.reset(AllocHostAVFrame(m_pLastFrame->hw_frames_ctx, m_pLastFrame->width, m_pLastFrame->height));
                    }
                    if (m_pHostFrame == nullptr)
                    {
                        m_pHostFrame = AllocAVFrame();
                    }
                }
                int nTransfer = av_hwframe_transfer_data(m_pHostFrame.get(), m_pLastFrame.get(), 0);
                if ((nTransfer) < 0)
                {
                    LOG_ERROR("av_hwframe_transfer_data failed {}, {}.", nTransfer, av_errstr(nTransfer));
                    return false;
                }
                m_pHostFrame->pts = m_pLastFrame->pts;
                m_pHostFrame->pict_type = m_pLastFrame->pict_type;
                m_pHostFrame->color_range = m_pLastFrame->color_range;
                m_pHostFrame->color_primaries = m_pLastFrame->color_primaries;
                m_pHostFrame->color_trc = m_pLastFrame->color_trc;
                m_pHostFrame->colorspace = m_pLastFrame->colorspace;
                pOutFrame = m_pHostFrame.get();
                m_pLastFrame = nullptr;
            }
            else
            {
                // or direct device buffer
                m_pLastFrame->format = GetHWDownloadFormat(m_pLastFrame->hw_frames_ctx);
                if (m_pLastFrame->format == AV_PIX_FMT_NONE)
                {
                    return false;
                }
                pOutFrame = m_pLastFrame.get();
            }
        }
        else
        {
            pOutFrame = m_pLastFrame.get();
        }
        NVIVideoImageFrame image{};
        if (ConvertPixelFormat((AVPixelFormat)pOutFrame->format, image.buffer.format))
        {
            image.info = info;
            image.info.width = static_cast<uint32_t>(pOutFrame->width);
            image.info.height = static_cast<uint32_t>(pOutFrame->height);
            image.info.tick.value = pOutFrame->pts;
            if (pOutFrame->colorspace != AVCOL_SPC_UNSPECIFIED && pOutFrame->color_range != AVCOL_RANGE_UNSPECIFIED)
            {
                image.info.colorspace = ConvertColorSpace(pOutFrame->color_primaries, pOutFrame->color_trc, pOutFrame->colorspace, pOutFrame->color_range);
            }
            image.buffer.type = m_eOutBufferType;
            if (m_eOutBufferType == NVIBuffer_D3DSurface9)
            {
                image.buffer.planes[0] = pOutFrame->data[0];
            }
            else if (m_eOutBufferType == NVIBuffer_D3D11Texture2D)
            {
                image.buffer.planes[0] = pOutFrame->data[0];
                image.buffer.planes[1] = pOutFrame->data[1];
            }  //todo! else device buffer
            else
            {
                for (int i = 0; i < 4; ++i)
                {
                    image.buffer.planes[i] = pOutFrame->data[i];
                    image.buffer.strides[i] = static_cast<uint32_t>(pOutFrame->linesize[i]);
                }
            }
            output(&image);
        }
        else
        {
            LOG_ERROR("Not match our pixel format: {}.", pOutFrame->format);
            return false;
        }
    }
    return true;
}

bool FFVideoDecoder::HWAccelContextInit(const NVIVideoAccelerate* accel)
{
    if (m_pDecoderContext == nullptr)
    {
        return false;
    }
    m_nHWPixelFormat = -1;
    m_eOutBufferType = NVIBuffer_HOST;
    if (accel == nullptr || accel->type <= NVIAccel_Auto)
    {
        return true;
    }
    auto typDevice = ToAVHWDeviceType(static_cast<NVIAccelType>(accel->type));
    if (typDevice == AV_HWDEVICE_TYPE_NONE)
    {
        return false;
    }
    else
    {
        for (int i = 0;; i++)
        {
            const AVCodecHWConfig* config = avcodec_get_hw_config(m_pDecoderContext->codec, i);
            if (!config)
            {
                break;
            }
            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == typDevice)
            {
                m_nHWPixelFormat = (int32_t)config->pix_fmt;
                break;
            }
        }
        if (m_nHWPixelFormat == -1)
        {
            LOG_WARNING("FFVideoDecoder not found valid hw pixel format.");
            return false;
        }
        else
        {
            AVBufferRef* pHWContext = CreateHWContext(accel);
            if (pHWContext)
            {
                if ((accel->flags & NVIAccelFlag_UseDeviceBuffer) == NVIAccelFlag_UseDeviceBuffer)
                {
                    m_eOutBufferType = GetHWBufferType(static_cast<NVIAccelType>(accel->type));
                }
                else
                {
                    m_eOutBufferType = NVIBuffer_HOST;
                }
                m_pDecoderContext->hw_device_ctx = pHWContext;
                m_pDecoderContext->opaque = this;
                m_pDecoderContext->get_format = GetHWFormat;
            }
            else
            {
                return false;
            }
        }
        return true;
    }
}

void FFVideoDecoder::Release()
{
    if (m_pDecoderContext)
    {
        avcodec_free_context(&m_pDecoderContext);
    }
}
