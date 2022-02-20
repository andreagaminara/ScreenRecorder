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

    `Asks to the user to set some recording configuration parameters and calls the methods to initialize all the structure needed to perform the recording.`

- void controller() throw();

    `Shows the menu to the user and calls the method to execute the action chosen by the user.`

- int start() throw();

    `Starts the recording of video and eventually audio packets.`
  
- int stop();

    `Terminates the recording.`

- int pause();

    `Stops temporarily the recording (it can be resumed later).`

- int resume() throw();

    `Resumes a recording that was previously paused.`

- int open_video_media() throw();

    `Opens video input device`

- int prepare_video_decoder() throw();

    `Creates and initializes the structure used to decode video input packet`

- int prepare_video_encoder() throw();

    `Creates and initializes the structure used to encode video input packet`

- int capture_video() throw();

    `Captures video frames`

- int transcode_video(AVPacket* input_packet, AVFrame* input_frame, int i) throw();

    `Decodes input packets and calls the method to encode them in output packets`
    
- int encode_video(AVFrame* input_frame, int i) throw();

    `Encodes input frames into output packets`

- int open_audio_media() throw();

    `Opens audio input device`

- int prepare_audio_decoder() throw();

    `Creates and initializes the structure used to decode audio input packet`    

- int prepare_audio_encoder() throw();

    `Creates and initializes the structure used to encode audio input packet`

- int capture_audio() throw();

    `Captures audio frames`

- int transcode_audio(AVPacket* input_packet, AVFrame* input_frame) throw();

    `Decodes input packets and encodes them in output packets`








