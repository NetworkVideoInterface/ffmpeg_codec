#pragma once

#include <array>
#include <memory>
#include <functional>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
}
#include <NVI/Codec.h>

#define AV_CUDA_USE_PRIMARY_CONTEXT (1 << 0)

// 只能用在函数的参数中
#define av_errstr(err) av_make_error_string(std::array<char, AV_ERROR_MAX_STRING_SIZE>().data(), AV_ERROR_MAX_STRING_SIZE, err)

namespace ffmpeg
{

inline AVCodecID ToAVCodecID(uint32_t codec)
{
    switch (codec)
    {
    case NVICodec_AVC: return AV_CODEC_ID_H264;
    case NVICodec_HEVC: return AV_CODEC_ID_H265;
    case NVICodec_AAC: return AV_CODEC_ID_AAC;
    case NVICodec_OPUS: return AV_CODEC_ID_OPUS;
    default: return AV_CODEC_ID_NONE;
    }
}

inline AVHWDeviceType ToAVHWDeviceType(NVIAccelType type)
{
    switch (type)
    {
    case NVIAccel_NVCodec: return AV_HWDEVICE_TYPE_CUDA;
    case NVIAccel_DXVA2: return AV_HWDEVICE_TYPE_DXVA2;
    case NVIAccel_D3D11VA: return AV_HWDEVICE_TYPE_D3D11VA;
    case NVIAccel_VideoToolbox: return AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
    case NVIAccel_MediaCodec: return AV_HWDEVICE_TYPE_MEDIACODEC;
    case NVIAccel_VAAPI: return AV_HWDEVICE_TYPE_VAAPI;
    default: return AV_HWDEVICE_TYPE_NONE;
    }
}

inline NVIColorSpace ConvertColorSpace(AVColorPrimaries primary, AVColorTransferCharacteristic transfer, AVColorSpace space, AVColorRange range)
{
    NVIColorSpace out;
    out.primary = static_cast<uint8_t>(primary);
    out.transfer = static_cast<uint8_t>(transfer);
    out.matrix = static_cast<uint8_t>(space);
    out.range = static_cast<uint8_t>(range);
    return out;
}

inline void ConvertColorSpace(
    const NVIColorSpace& in, AVColorPrimaries& primary, AVColorTransferCharacteristic& transfer, AVColorSpace& space, AVColorRange& range)
{
    primary = static_cast<AVColorPrimaries>(in.primary);
    transfer = static_cast<AVColorTransferCharacteristic>(in.transfer);
    space = static_cast<AVColorSpace>(in.matrix);
    range = static_cast<AVColorRange>(in.range);
}

inline bool ConvertPixelFormat(const AVPixelFormat& in, uint32_t& out)
{
    switch (in)
    {
    case AV_PIX_FMT_YUVJ420P: out = NVIPixel_I420; return true;
    case AV_PIX_FMT_YUV420P: out = NVIPixel_I420; return true;
    case AV_PIX_FMT_NV12: out = NVIPixel_NV12; return true;
    case AV_PIX_FMT_NV21: out = NVIPixel_NV21; return true;
    case AV_PIX_FMT_P010BE: out = NVIPixel_P010BE; return true;
    case AV_PIX_FMT_P010LE: out = NVIPixel_P010LE; return true;
    default: out = NVIPixel_Unspecific; break;
    }
    return false;
}

inline bool ConvertPixelFormat(const uint32_t& in, AVPixelFormat& out)
{
    switch (in)
    {
    case NVIPixel_I420: out = AV_PIX_FMT_YUV420P; return true;
    case NVIPixel_NV12: out = AV_PIX_FMT_NV12; return true;
    case NVIPixel_NV21: out = AV_PIX_FMT_NV21; return true;
    case NVIPixel_P010BE: out = AV_PIX_FMT_P010BE; return true;
    case NVIPixel_P010LE: out = AV_PIX_FMT_P010LE; return true;
    default: out = AV_PIX_FMT_NONE; break;
    }
    return false;
}

typedef std::unique_ptr<AVFrame, void (*)(AVFrame*)> AVFramePtr;

inline void FreeAVFrame(AVFrame* pObject)
{
    av_frame_free(&pObject);
}

inline AVFramePtr AllocAVFrame()
{
    return AVFramePtr(av_frame_alloc(), &FreeAVFrame);
}

typedef std::unique_ptr<AVPacket, void (*)(AVPacket*)> AVPacketPtr;

inline void FreeAVPacket(AVPacket* pObject)
{
    av_packet_free(&pObject);
}

inline AVPacketPtr AllocAVPacket()
{
    return AVPacketPtr(av_packet_alloc(), &FreeAVPacket);
}

}  //namespace ffmpeg