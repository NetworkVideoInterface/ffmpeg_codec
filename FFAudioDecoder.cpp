#include "FFAudioDecoder.h"
#include "FFmpegWrapper.hpp"
#include "adaption/Logging.h"
#include <cstring>
#include <fstream>

using namespace ffmpeg;

FFAudioDecoder::FFAudioDecoder()
    : m_pDecoderContext(nullptr)
    , m_pWaveBuffer(nullptr)
    , m_szWaveBuffer(0)
    , m_wave({})
{
}

FFAudioDecoder::~FFAudioDecoder()
{
    Release();
}

bool FFAudioDecoder::Config(const NVIAudioCodecParam& param)
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
        if (m_pDecoderContext->codec)
        {
            LOG_NOTICE("FFAudioDecoder init {}, {}.", m_pDecoderContext->codec->name, m_pDecoderContext->codec->long_name);
        }
        int nOpen = avcodec_open2(m_pDecoderContext, nullptr, nullptr);
        if (nOpen == 0)
        {
            return true;
        }
        else
        {
            LOG_ERROR("FFAudioDecoder avcodec_open2 failed {}, {}.", nOpen, av_errstr(nOpen));
            avcodec_free_context(&m_pDecoderContext);
        }
    }
    return false;
}

bool FFAudioDecoder::Decoding(const NVIAudioEncodedPacket& packet, const Output& output)
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
                    if (output)
                    {
                        const size_t szBytesPerSample = static_cast<size_t>(av_get_bytes_per_sample(m_pDecoderContext->sample_fmt));
                        const size_t szFrameBuffer = static_cast<size_t>(
                            av_samples_get_buffer_size(nullptr, pFrame->ch_layout.nb_channels, pFrame->nb_samples, m_pDecoderContext->sample_fmt, 0));
                        if (uOut == 0u)
                        {
                            m_wave.info = packet.info;
                            m_wave.info.tick.value = pFrame->pts;
                            m_wave.info.sample_rate = static_cast<uint32_t>(pFrame->sample_rate);
                            m_wave.info.depth = static_cast<uint16_t>(szBytesPerSample << 3);
                            m_wave.info.channels = static_cast<uint16_t>(pFrame->ch_layout.nb_channels);
                            m_wave.buffer.align =
                                static_cast<uint16_t>(szFrameBuffer / pFrame->ch_layout.nb_channels / pFrame->nb_samples);  // bytes per sample
                        }
                        m_wave.buffer.samples += static_cast<uint16_t>(pFrame->nb_samples);
                        const size_t szWaveBuffer = m_wave.buffer.size + szFrameBuffer;
                        if (m_pWaveBuffer == nullptr || m_szWaveBuffer < szWaveBuffer)
                        {
                            uint8_t* pSwapBuffer = new uint8_t[szWaveBuffer];
                            if (m_pWaveBuffer && m_wave.buffer.size > 0)
                            {
                                memcpy(pSwapBuffer, m_wave.buffer.data, m_wave.buffer.size);
                            }
                            m_pWaveBuffer.reset(pSwapBuffer);
                            m_szWaveBuffer = szWaveBuffer;
                            m_wave.buffer.data = m_pWaveBuffer.get();
                        }
                        uint8_t* pBuffer = m_pWaveBuffer.get() + m_wave.buffer.size;
                        if (av_sample_fmt_is_planar(m_pDecoderContext->sample_fmt) == 1)
                        {
                            const size_t szBytesPerBlock = static_cast<size_t>(m_wave.buffer.align) * m_wave.info.channels;
                            for (int i = 0; i < pFrame->ch_layout.nb_channels; ++i)
                            {
                                const uint8_t* pPlane = pFrame->data[i];
                                if (pPlane == nullptr)
                                {
                                    LOG_ERROR("FFAudioDecoder audio wave buffer plane{} is null.", i);
                                    return false;
                                }
                                const size_t szOffsetOfChannel = static_cast<size_t>(i) * m_wave.buffer.align;
                                for (int j = 0; j < pFrame->nb_samples; ++j)
                                {
                                    memcpy(pBuffer + (szBytesPerBlock * j + szOffsetOfChannel), pPlane, szBytesPerSample);
                                    pPlane += m_wave.buffer.align;
                                }
                            }
                        }
                        else
                        {
                            memcpy(pBuffer, pFrame->data[0], szFrameBuffer);
                        }
                        m_wave.buffer.size += szFrameBuffer;
                        ++uOut;
                    }
                }
                else
                {
                    if (uOut > 0u)
                    {
                        output(&m_wave);
                        m_wave.buffer.size = 0;
                        m_wave.buffer.samples = 0;
                    }
                    if (nRecv == AVERROR(EAGAIN))
                    {
                        break;
                    }
                    else
                    {
                        LOG_ERROR("FFAudioDecoder avcodec_receive_frame failed {}, {}.", nRecv, av_errstr(nRecv));
                        return false;
                    }
                }
            }
        }
        else
        {
            LOG_ERROR("FFAudioDecoder avcodec_send_packet failed {}, {}.", nSend, av_errstr(nSend));
            return false;
        }
        return true;
    }
    return false;
}

void FFAudioDecoder::Release()
{
    if (m_pDecoderContext)
    {
        avcodec_free_context(&m_pDecoderContext);
    }
}
