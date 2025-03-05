#pragma once

#include <memory>
#include <functional>
#include <NVI/Codec.h>

struct AVCodecContext;
struct AVFrame;

class FFVideoDecoder final
{
public:
    typedef std::function<int32_t(const NVIVideoImageFrame* image)> Output;
    //typedef NVIVideoDecode::OnFrame Output;

public:
    FFVideoDecoder();
    virtual ~FFVideoDecoder();

public:
    bool Config(const NVIVideoCodecParam& param);
    bool Decoding(const NVIVideoEncodedPacket& packet, const Output& output);
    int32_t HWPixelFormat() const
    {
        return m_nHWPixelFormat;
    }

private:
    bool OutputLastFrame(const NVIImageInfo& info, const Output& output);
    bool HWAccelContextInit(const NVIVideoAccelerate* accel);
    void Release();

private:
    Output m_output;
    AVCodecContext* m_pDecoderContext;
    int32_t m_nHWPixelFormat;
    NVIBufferType m_eOutBufferType;
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> m_pLastFrame;
    std::unique_ptr<AVFrame, void (*)(AVFrame*)> m_pHostFrame;
};
