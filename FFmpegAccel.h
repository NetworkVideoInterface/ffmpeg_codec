#pragma once

extern "C"
{
#include <libavutil/cpu.h>
#include <libavutil/buffer.h>
#include <libavutil/hwcontext.h>
#include <libavcodec/avcodec.h>
}
#include "NVI/Codec.h"

namespace ffmpeg
{
AVBufferRef* CreateHWContext(const NVIVideoAccelerate* pAccel);
void* CudaContext(AVHWFramesContext* pHWFramesContext);
}  //namespace ffmpeg
