//
// Created by andrea on 10/11/21.
//

#ifndef SCREENRECORDER_PROVA_SCREENRECORDER_H
#define SCREENRECORDER_PROVA_SCREENRECORDER_H

#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <math.h>
#include <string.h>
#include <thread>
#include <atomic>

#define __STDC_CONSTANT_MACROS

//FFMPEG LIBRARIES
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"

#include "libavdevice/avdevice.h"

#include "libavfilter/avfilter.h"
//#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"

#include "libavformat/avformat.h"
#include "libavformat/avio.h"

// libav resample

#include "libavutil/opt.h"
#include "libavutil/common.h"
#include "libavutil/channel_layout.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libavutil/file.h"
#include "libavutil/audio_fifo.h"

// lib swresample

#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

}

typedef struct StreamingContext {
    AVFormatContext *avfc;
    AVCodec *avc;
    AVStream *avs;
    AVCodecContext *avcc;
    int stream_index;
    char *filename;
} StreamingContext;

class ScreenRecorder {
private:
    std::atomic_bool isRunning;

    StreamingContext *video_decoder;
    StreamingContext *video_encoder;
    std::thread *video_thread;

    StreamingContext *audio_decoder;
    StreamingContext *audio_encoder;
    SwrContext      *audioConverter;
    AVAudioFifo     *audioFifo;
    std::thread *audio_thread;

    AVFormatContext *out_avfc;

    char *output_filename;

public:
    ScreenRecorder();
    ~ScreenRecorder();

    int capture();
    int capture_video();
    int capture_audio();
    int transcode_audio(AVPacket *input_packet, AVFrame *input_frame);
    int open_video_media();
    int open_audio_media();
    int prepare_video_decoder();
    int prepare_audio_decoder();
    int prepare_video_encoder();
    int prepare_audio_encoder();
    int encode_video(AVFrame *input_frame);
    int transcode_video(AVPacket *input_packet, AVFrame *input_frame);
};


#endif //SCREENRECORDER_PROVA_SCREENRECORDER_H
