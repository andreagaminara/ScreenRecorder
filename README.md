# ScreenRecorder
Screen recorder application built using FFmpeg 4.1 library.

### Dependencies
List of libraries needed on your system to run the application:
- libavcodec
- libavformat
- libavfilter
- libavutil
- libavdevice
- libswscale
- libswresample
- libx11

### Documentation
The class ScreenRecorder is used as an API to call the functions of the FFmpeg library. The methods of the class ScreenRecorder are the following:
- int capture() throw();
- void controller() throw();
- int start() throw();
  Start the recording of video and eventually audio packets.
- int stop();
- int pause();
- int resume() throw();
- int open_video_media() throw();
- int prepare_video_decoder() throw();
- int prepare_video_encoder() throw();
- int capture_video() throw();
- int transcode_video(AVPacket* input_packet, AVFrame* input_frame, int i) throw();
- int encode_video(AVFrame* input_frame, int i) throw();
- int open_audio_media() throw();
- int prepare_audio_decoder() throw();
- int prepare_audio_encoder() throw();
- int capture_audio() throw();
- int transcode_audio(AVPacket* input_packet, AVFrame* input_frame) throw();







