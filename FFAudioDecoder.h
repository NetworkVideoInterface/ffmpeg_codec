#pragma once

#include <memory>
#include <functional>
#include <NVI/Codec.h>

struct AVCodecContext;

class FFAudioDecoder final
{
public:
    typedef std::function<int32_t(const NVIAudioWaveFrame* wave)> Output;

public:
    FFAudioDecoder();
    virtual ~FFAudioDecoder();

public:
    bool Config(const NVIAudioCodecParam& param);
    bool Decoding(const NVIAudioEncodedPacket& packet, const Output& output);

private:
    void Release();

private:
    AVCodecContext* m_pDecoderContext;
    std::unique_ptr<uint8_t[]> m_pWaveBuffer;
    size_t m_szWaveBuffer;
    NVIAudioWaveFrame m_wave;
};
