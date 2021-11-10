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

// lib swresample

#include "libswscale/swscale.h"

}

typedef struct StreamingContext {
    AVFormatContext *avfc;
    AVCodec *video_avc;
    //AVCodec *audio_avc;
    AVStream *video_avs;
    //AVStream *audio_avs;
    AVCodecContext *video_avcc;
    //AVCodecContext *audio_avcc;
    int video_index;
    //int audio_index;
    char *filename;
} StreamingContext;

class ScreenRecorder {
private:
    StreamingContext *decoder;
    StreamingContext *encoder;

public:
    ScreenRecorder();
    ~ScreenRecorder();

    int capture(int numFrames);
    int open_media();
    int prepare_decoder();
    int prepare_video_encoder();
    int encode_video(AVFrame *input_frame);
    int transcode_video(AVPacket *input_packet, AVFrame *input_frame);
};


#endif //SCREENRECORDER_PROVA_SCREENRECORDER_H
