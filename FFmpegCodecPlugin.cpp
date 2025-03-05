#include "FFmpegCodecPlugin.h"
#include "FFAudioDecoder.h"
#include "FFVideoDecoder.h"
#include "adaption/Logging.h"

#define DEC_SUCCESS (0)
#define DEC_ERROR(x) (-1024 - x)
#define DEC_ERROR_INVALID_ARGS DEC_ERROR(1)
#define DEC_ERROR_NOT_SUPPORT DEC_ERROR(2)
#define DEC_ERROR_DECODING DEC_ERROR(3)

class FFmpegVideoDecodeDelegate final
{
public:
    static FFVideoDecoder* Alloc(uint32_t codec)
    {
        if (codec == NVICodec_AVC || codec == NVICodec_HEVC)
        {
            return new FFVideoDecoder();
        }
        return nullptr;
    }
    static int32_t Config(void* decoder, const NVIVideoCodecParam* param)
    {
        if (decoder && param)
        {
            auto pDecoder = reinterpret_cast<FFVideoDecoder*>(decoder);
            return pDecoder->Config(*param) ? DEC_SUCCESS : DEC_ERROR_NOT_SUPPORT;
        }
        return DEC_ERROR_INVALID_ARGS;
    }
    static int32_t Decoding(void* decoder, const NVIVideoEncodedPacket* in, NVIVideoDecode::OnFrame out, void* user)
    {
        if (decoder && in)
        {
            auto pDecoder = reinterpret_cast<FFVideoDecoder*>(decoder);
            if (out == nullptr)
            {
                return pDecoder->Decoding(*in, FFVideoDecoder::Output(nullptr)) ? DEC_SUCCESS : DEC_ERROR_DECODING;
            }
            else
            {
                return pDecoder->Decoding(*in,
                                          [out, user](const NVIVideoImageFrame* frame) -> int32_t
                                          {
                                              return out(frame, user);
                                          })
                           ? DEC_SUCCESS
                           : DEC_ERROR_DECODING;
            }
        }
        return DEC_ERROR_INVALID_ARGS;
    }
    static int32_t Release(void* decoder)
    {
        if (decoder)
        {
            auto pDecoder = reinterpret_cast<FFVideoDecoder*>(decoder);
            delete pDecoder;
            return DEC_SUCCESS;
        }
        return DEC_ERROR_INVALID_ARGS;
    }
};

class FFmpegAudioDecodeDelegate final
{
public:
    static FFAudioDecoder* Alloc(uint32_t codec)
    {
        if (codec == NVICodec_AAC || codec == NVICodec_OPUS)
        {
            return new FFAudioDecoder();
        }
        return nullptr;
    }
    static int32_t Config(void* decoder, const NVIAudioCodecParam* param)
    {
        if (decoder && param)
        {
            auto pDecoder = reinterpret_cast<FFAudioDecoder*>(decoder);
            return pDecoder->Config(*param) ? DEC_SUCCESS : DEC_ERROR_NOT_SUPPORT;
        }
        return DEC_ERROR_INVALID_ARGS;
    }
    static int32_t Decoding(void* decoder, const NVIAudioEncodedPacket* in, NVIAudioDecode::OnFrame out, void* user)
    {
        if (decoder && in)
        {
            auto pDecoder = reinterpret_cast<FFAudioDecoder*>(decoder);
            if (out == nullptr)
            {
                return pDecoder->Decoding(*in, FFAudioDecoder::Output(nullptr)) ? DEC_SUCCESS : DEC_ERROR_DECODING;
            }
            else
            {
                return pDecoder->Decoding(*in,
                                          [out, user](const NVIAudioWaveFrame* frame) -> int32_t
                                          {
                                              return out(frame, user);
                                          })
                           ? DEC_SUCCESS
                           : DEC_ERROR_DECODING;
            }
        }
        return DEC_ERROR_INVALID_ARGS;
    }
    static int32_t Release(void* decoder)
    {
        if (decoder)
        {
            auto pDecoder = reinterpret_cast<FFAudioDecoder*>(decoder);
            delete pDecoder;
            return DEC_SUCCESS;
        }
        return DEC_ERROR_INVALID_ARGS;
    }
};

//////////////////////////////////////////////////////////////////////////
NVIVideoDecode VideoDecodeAlloc(uint32_t codec)
{
    NVIVideoDecode vd{};
    vd.decoder = FFmpegVideoDecodeDelegate::Alloc(codec);
    if (vd.decoder)
    {
        vd.Config = &FFmpegVideoDecodeDelegate::Config;
        vd.Decoding = &FFmpegVideoDecodeDelegate::Decoding;
        vd.Release = &FFmpegVideoDecodeDelegate::Release;
    }
    return vd;
}

NVIAudioDecode AudioDecodeAlloc(uint32_t codec)
{
    NVIAudioDecode ad{};
    ad.decoder = FFmpegAudioDecodeDelegate::Alloc(codec);
    if (ad.decoder)
    {
        ad.Config = &FFmpegAudioDecodeDelegate::Config;
        ad.Decoding = &FFmpegAudioDecodeDelegate::Decoding;
        ad.Release = &FFmpegAudioDecodeDelegate::Release;
    }
    return ad;
}

void SetLogging(void (*logging)(int level, const char* message, unsigned int length))
{
    SetLoggingFunc(logging);
}
