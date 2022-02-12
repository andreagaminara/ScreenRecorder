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
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <exception>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <dshow.h>
#elif __linux__
#include <X11/Xlib.h>
#endif

#define __STDC_CONSTANT_MACROS

//FFMPEG LIBRARIES
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavcodec/avfft.h"

#include "libavdevice/avdevice.h"

#include "libavfilter/avfilter.h"
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


class ScreenRecorderException : std::exception {
private:
    const char* desc;
public:
    ScreenRecorderException(const char* description) : desc(description) {}
    const char* what() const noexcept {
        return desc;
    }
};


typedef struct StreamingContext {
    AVFormatContext* avfc;
    AVCodec* avc;
    AVStream* avs;
    AVCodecContext* avcc;
    int stream_index;
    char* filename;
} StreamingContext;

class ScreenRecorder {
private:

    bool isAudio;
    std::atomic_bool isRunning;
    std::atomic_bool isPause;
    std::mutex m;
    std::condition_variable cv;

    char* video_input_format;
    char* audio_input_format;

    StreamingContext* video_decoder;
    StreamingContext* video_encoder;
    std::thread* video_thread;

    StreamingContext* audio_decoder;
    StreamingContext* audio_encoder;
    SwrContext* audioConverter;
    AVAudioFifo* audioFifo;
    std::thread* audio_thread;

    AVFormatContext* out_avfc;

    char* output_filename;

    int screen_width;
    int screen_height;
    int video_width;
    int video_height;
    int offset_x;
    int offset_y;

public:
    ScreenRecorder();
    ~ScreenRecorder();

    int start() throw();
    int stop();
    int pause();
    int resume() throw();
    int capture() throw();
    int capture_video() throw();
    int capture_audio() throw();
    int transcode_audio(AVPacket* input_packet, AVFrame* input_frame) throw();
    int open_video_media() throw();
    int open_audio_media() throw();
    int prepare_video_decoder() throw();
    int prepare_audio_decoder() throw();
    int prepare_video_encoder() throw();
    int prepare_audio_encoder() throw();
    int encode_video(AVFrame* input_frame, int i) throw();
    int transcode_video(AVPacket* input_packet, AVFrame* input_frame, int i) throw();
    void controller() throw();

};


#endif //SCREENRECORDER_PROVA_SCREENRECORDER_H
