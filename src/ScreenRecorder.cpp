//
// Created by andrea on 10/11/21.
//

#ifdef _WIN32
    #include "ScreenRecorder.h"
#elif __linux__
    #include "../include/ScreenRecorder.h"
#endif

using namespace std;

ScreenRecorder::ScreenRecorder()
{
    avdevice_register_all();

#ifdef _WIN32
    screen_width = (int)GetSystemMetrics(SM_CXSCREEN);
    screen_height = (int)GetSystemMetrics(SM_CYSCREEN);
#elif __linux__
    Display* disp = XOpenDisplay(NULL);
    Screen* scrn = DefaultScreenOfDisplay(disp);
    screen_width = scrn->width;
    screen_height = scrn->height;
#endif

    isRunning = false;
    isPause = false;
    isAudio = false;
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder() {

    video_thread->join();

    if (isAudio) {
        audio_thread->join();
    }

    av_write_trailer(out_avfc);

    avformat_free_context(out_avfc);
    out_avfc = NULL;

    swr_free(&audioConverter);
    av_audio_fifo_free(audioFifo);

    if (isAudio) {
        avformat_close_input(&audio_decoder->avfc);
        avformat_free_context(audio_decoder->avfc);
        audio_decoder->avfc = NULL;

        avcodec_free_context(&audio_decoder->avcc);
        audio_decoder->avcc = NULL;

        avcodec_free_context(&audio_encoder->avcc);
        audio_encoder->avcc = NULL;

        free(audio_decoder);
        audio_decoder = NULL;

        free(audio_encoder);
        audio_encoder = NULL;
    }

    avformat_close_input(&video_decoder->avfc);
    avformat_free_context(video_decoder->avfc);
    video_decoder->avfc = NULL;

    avcodec_free_context(&video_decoder->avcc);
    video_decoder->avcc = NULL;

    avcodec_free_context(&video_encoder->avcc);
    video_encoder->avcc = NULL;

    free(video_decoder);
    video_decoder = NULL;

    free(video_encoder);
    video_encoder = NULL;
}

/**
* Starts the recording of video and eventually audio packets.
*/

int ScreenRecorder::start() throw() {

    isRunning = true;
    isPause = false;

    try{
        if (isAudio) {
            open_audio_media();
            prepare_audio_decoder();
        }
        open_video_media();
        prepare_video_decoder();
    }
    catch (ScreenRecorderException e) {
        throw e;
    }

    
    avformat_alloc_output_context2(&(out_avfc), NULL, NULL, output_filename);
    if (!out_avfc) {
        throw ScreenRecorderException("Failed to allocate memory for output format");
    }

    try {
        prepare_video_encoder();
        if (isAudio) {
            prepare_audio_encoder();
        }
    }
    catch (ScreenRecorderException e) {
        throw e;
    }

    if (out_avfc->oformat->flags & AVFMT_GLOBALHEADER)
        out_avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(out_avfc->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_avfc->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
            throw ScreenRecorderException("Failed to open the output file");
        }
    }

    if (avformat_write_header(out_avfc, NULL) < 0) {
        throw ScreenRecorderException("Failed to open the output file");
    }

    try {
        video_thread = new thread([this]() {
            this->capture_video();
            });

        if (isAudio) {
            audio_thread = new thread([this]() {
                this->capture_audio();
                });
        }
    }
    catch (ScreenRecorderException e) {
        throw e;
    }

    return 0;
}
/**
* Terminates the recording.
*/
int ScreenRecorder::stop() {
    unique_lock<mutex> ul(m);
    isRunning.exchange(false);
    cv.notify_all();

    return 0;
}
/**
* Stops temporarily the recording (it can be resumed later).
*/

int ScreenRecorder::pause() {

    isPause.exchange(true);
    return 0;

}
/**
* Resumes a recording that was previously paused.
*/
int ScreenRecorder::resume() throw() {
    unique_lock<mutex> ul(m);
    if (isAudio) {
        AVInputFormat* pAVInputFormat = av_find_input_format(audio_input_format);

        if (avformat_open_input(&audio_decoder->avfc, audio_decoder->filename, pAVInputFormat, NULL) != 0) {
            throw ScreenRecorderException("Failed to open audio input file");
        }

    }
    isPause.exchange(false);
    cv.notify_all();

    return 0;

}

/**
* Asks to the user to set some recording configuration parameters and calls the methods to initialize all the structure needed to perform the recording.
*/

int ScreenRecorder::capture() throw() {

    output_filename = (char*)malloc(50 * sizeof(char));

    //Dimensions of area to be recorded setting
    cout << "The screen resolution is " << screen_width << "x" << screen_height << endl;

    cout << "Insert the dimensions of the area of screen that you want to record (not exceed screen resolution)" << endl;

    cout << "Width: ";
    cin >> video_width;
    video_width = (video_width/32)*32;

    cout << "Height: ";
    cin >> video_height;
    video_height = (video_height/2)*2;

    //Offset setting
    cout << "Insert the offset (position where you want your recorded area starts on the screen)" << endl;

    cout << "Horizontal offset: ";
    cin >> offset_x;

    cout << "Vertical offset: ";
    cin >> offset_y;

    if (video_width+offset_x > screen_width || video_height + offset_y > screen_height)
        throw ScreenRecorderException("Invalid dimensions for the capture area");

    //Output filename setting
    cout << "Insert the name of the output file" << endl;

    cout << "Filename (without extension): ";
    cin >> output_filename;
    strcat(output_filename, ".mp4");

    //Audio option setting
    string answer;
    cout << "Choose whether to record audio or not (yes/no): ";
    cin >> answer;

    if (answer.compare("yes") == 0) {
        isAudio = true;
    }

    else {
        isAudio = false;
    }

    video_input_format = (char*)malloc(50 * sizeof(char));

    audio_input_format = (char*)malloc(50 * sizeof(char));

#ifdef _WIN32
    strcpy(video_input_format, "gdigrab");
    strcpy(audio_input_format, "dshow");
#elif __linux__
    strcpy(video_input_format, "x11grab");
    strcpy(audio_input_format, "alsa");
#endif

    /*start();
    this_thread::sleep_for(15s);
    pause();
    cout << "pause" << endl;
    cout << "pause" << endl;
    this_thread::sleep_for(10s);
    cout << "resume" << endl;
    cout << "resume" << endl;
    resume();
    this_thread::sleep_for(10s);
    cout << "Before stop" << endl;
    stop();
    cout << "After stop" << endl;
    */

    try {
        controller();
    }
    catch (ScreenRecorderException e) {
        throw e;
    }

    return 0;

}
/**
* Shows the menu to the user and calls the method to execute the action chosen by the user.
*/

void ScreenRecorder::controller() throw() {
    int action;
    bool endRecording = false;
    while (!endRecording) {
        cout << "Choose one action:\n0 = start.\n1 = stop.\n2 = pause.\n3 = resume." << endl;
        cin >> action;
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        switch (action) {
        case 0:
            if(!isRunning){
                try {
                    this->start();
                    cout << "Video recording is now started." << endl;
                }
                catch (ScreenRecorderException e) {
                    throw e;
                }
            }
            else cout << "Video recording already started." << endl;
            break;
        case 1:
            if (!isRunning){
                cout << "Video recording hasn't started yet" << endl;
            } else {
                this->stop();
                endRecording = true;
                cout << "Video recording is terminated." << endl;
            }
            break;
        case 2:
            if (!isRunning){
                cout << "Video recording hasn't started yet" << endl;
            } else {
                if (!isPause) {
                    this->pause();
                    cout << "Video recording is now paused." << endl;
                } else cout << "Video recording already paused." << endl;
            }
            break;
        case 3:
            if (!isRunning){
                cout << "Video recording hasn't started yet" << endl;
            } else {
                if (isPause) {
                    this->resume();
                    cout << "Video recording is now resumed.\n" << endl;
                } else cout << "Video recording not paused." << endl;
            }
            break;
        default:
            cout << "Action not recognised." << endl;

        }
    }
}
/**
* aptures video frames.
*/
int ScreenRecorder::capture_video() throw() {

    AVFrame* input_frame = av_frame_alloc();
    if (!input_frame) {
        throw ScreenRecorderException("Failed to allocate memory for AVFrame");
    }

    AVPacket* input_packet = av_packet_alloc();
    if (!input_packet) {
        throw ScreenRecorderException("Failed to allocated memory for AVPacket");
    }

    int i = 0;

    while (isRunning)
    {
        unique_lock<mutex> ul(m);
        if (isPause) {
            cv.wait(ul, [this]() {return !isPause || !isRunning;});
        }
        ul.unlock();
        if (av_read_frame(video_decoder->avfc, input_packet) < 0)
            throw ScreenRecorderException("Error in reading video frame");

        if (video_decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            try {
                transcode_video(input_packet, input_frame, i);
            }
            catch (ScreenRecorderException e) {
                throw e;
            }
            av_packet_unref(input_packet);
        }
        else {
            cout << "Ignoring all non video or audio packets" << endl;
        }

        i++;
    }

    if (input_frame != NULL) {
        av_frame_free(&input_frame);
        input_frame = NULL;
    }

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    return 0;
}
/**
* Captures audio frames.
*/
int ScreenRecorder::capture_audio() throw() {

    audioConverter = swr_alloc_set_opts(nullptr,
        av_get_default_channel_layout(audio_decoder->avcc->channels),
        AV_SAMPLE_FMT_FLTP,
        audio_decoder->avcc->sample_rate,
        av_get_default_channel_layout(audio_decoder->avcc->channels),
        (AVSampleFormat)audio_decoder->avs->codecpar->format,
        audio_decoder->avs->codecpar->sample_rate,
        0, nullptr);
    swr_init(audioConverter);

    // 2 seconds FIFO buffer
    audioFifo = av_audio_fifo_alloc(AV_SAMPLE_FMT_FLTP, audio_decoder->avcc->channels,
        audio_decoder->avcc->sample_rate * 2);

    AVFrame* input_frame = av_frame_alloc();
    if (!input_frame) {
        throw ScreenRecorderException("Failed to allocate memory for AVFrame");
    }

    AVPacket* input_packet = av_packet_alloc();
    if (!input_packet) {
        throw ScreenRecorderException("Failed to allocate memory for AVPacket");
    }

    AVPacket* outputPacket = av_packet_alloc();
    if (!outputPacket) {
        throw ScreenRecorderException("Failed to allocate memory for AVPacket");
    }

    int i = 0;

    while (isRunning)
    {
        unique_lock<mutex> ul(m);
        if (isPause) {
            avformat_close_input(&audio_decoder->avfc);

            cv.wait(ul, [this]() {return !isPause || !isRunning;});

            /*AVInputFormat* pAVInputFormat = av_find_input_format(audio_input_format);

            if (avformat_open_input(&audio_decoder->avfc, audio_decoder->filename, pAVInputFormat, NULL) != 0) {
                throw ScreenRecorderException("Failed to open audio input file");
            }*/
        }
        ul.unlock();
        if (av_read_frame(audio_decoder->avfc, input_packet) < 0) {
            throw ScreenRecorderException("Error reading audio frame");
        }

        if (audio_decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {

            try {
                transcode_audio(input_packet, input_frame);
            }
            catch (ScreenRecorderException e) {
                throw e;
            }

            int ret;

            while (av_audio_fifo_size(audioFifo) >= audio_encoder->avcc->frame_size) {
                AVFrame* outputFrame = av_frame_alloc();
                outputFrame->nb_samples = audio_encoder->avcc->frame_size;
                outputFrame->channels = audio_decoder->avcc->channels;
                outputFrame->channel_layout = av_get_default_channel_layout(audio_decoder->avcc->channels);
                outputFrame->format = AV_SAMPLE_FMT_FLTP;
                outputFrame->sample_rate = audio_encoder->avcc->sample_rate;

                ret = av_frame_get_buffer(outputFrame, 0);
                if (ret < 0) {
                    throw ScreenRecorderException("Error while allocating audio buffer");
                }
                ret = av_audio_fifo_read(audioFifo, (void**)outputFrame->data, audio_encoder->avcc->frame_size);
                if (ret < 0) {
                    throw ScreenRecorderException("Error reading from audioFifo");
                }

                outputFrame->pts = i * 1024;

                ret = avcodec_send_frame(audio_encoder->avcc, outputFrame);
                if (ret < 0) {
                    throw ScreenRecorderException("Failed to send frame in encoding");
                }
                av_frame_free(&outputFrame);
                ret = avcodec_receive_packet(audio_encoder->avcc, outputPacket);
                if (ret == AVERROR(EAGAIN)) {
                    continue;
                }
                else if (ret < 0) {
                    throw ScreenRecorderException("Failed to receive packet in encoding");
                }


                outputPacket->stream_index = audio_encoder->avs->index;
                outputPacket->duration = audio_encoder->avs->time_base.den * 1024 / audio_encoder->avcc->sample_rate;
                outputPacket->dts = outputPacket->pts = i * 1024;

                i++;

                ret = av_interleaved_write_frame(out_avfc, outputPacket);
                av_packet_unref(outputPacket);

            }
        }
        else {
            cout << "Ignoring all non video or audio packets" << endl;
        }


    }

    if (input_frame != NULL) {
        av_frame_free(&input_frame);
        input_frame = NULL;
    }

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    return 0;
}
/**
* Decodes input packets and encodes them in output packets.
*/
int ScreenRecorder::transcode_audio(AVPacket* input_packet, AVFrame* input_frame) throw() {

    uint8_t** cSamples = nullptr;

    int response = avcodec_send_packet(audio_decoder->avcc, input_packet);
    if (response < 0) {
        throw ScreenRecorderException("Error while sending packet to audio_decoder");
    }

    response = avcodec_receive_frame(audio_decoder->avcc, input_frame);
    if (response < 0) {
        throw ScreenRecorderException("Error while receiving frame from audio_decoder");
    }

    response = av_samples_alloc_array_and_samples(&cSamples, NULL, audio_encoder->avcc->channels, input_frame->nb_samples, AV_SAMPLE_FMT_FLTP, 0);
    if (response < 0) {
        throw ScreenRecorderException("Failed to alloc samples by av_samples_alloc_array_and_samples");
    }

    response = swr_convert(audioConverter, cSamples, input_frame->nb_samples, (const uint8_t**)input_frame->extended_data, input_frame->nb_samples);
    if (response < 0) {
        throw ScreenRecorderException("Failed to convert with swr_convert");
    }
    if (av_audio_fifo_space(audioFifo) < input_frame->nb_samples) {
        throw ScreenRecorderException("Audio buffer is too small");
    }

    response = av_audio_fifo_write(audioFifo, (void**)cSamples, input_frame->nb_samples);
    if (response < 0) {
        throw ScreenRecorderException("Failed to write audioFifo");
    }

    av_freep(&cSamples[0]);

    av_frame_unref(input_frame);
    av_packet_unref(input_packet);

    return 0;
}
/**
* Opens video input device.
*/
int ScreenRecorder::open_video_media() throw() {

    video_decoder = (StreamingContext*)calloc(1, sizeof(StreamingContext));
    video_decoder->filename = (char*)malloc(50 * sizeof(char));
#ifdef _WIN32
    strcpy(video_decoder->filename, "desktop");
#elif __linux__
    char *display = getenv("DISPLAY");
    string str_filename = string(display) + ".0+" + to_string(offset_x) + "," + to_string(offset_y);
    strcpy(video_decoder->filename, str_filename.c_str());
#endif


    video_encoder = (StreamingContext*)calloc(1, sizeof(StreamingContext));

    video_decoder->avfc = avformat_alloc_context();

    if (!video_decoder->avfc) {
        throw ScreenRecorderException("Failed to alloc memory for video input format");
    }

    AVInputFormat* pAVInputFormat = av_find_input_format(video_input_format);

    AVDictionary* options = NULL;

    string resolution = to_string(video_width) + "x" + to_string(video_height);

    int value = av_dict_set(&options, "video_size", resolution.c_str(), 0);
    if (value < 0) {
        throw ScreenRecorderException("Failed to video size option");
    }
    
     value = av_dict_set(&options, "framerate", "15", 0);
    if (value < 0) {

        throw ScreenRecorderException("Error in setting dictionary value (setting framerate)");
    }

#ifdef _WIN32
    value = av_dict_set(&options, "offset_x", to_string(offset_x).c_str(), 0);
    if (value < 0) {
        throw ScreenRecorderException("Failed to set video offset_x option");
    }

    value = av_dict_set(&options, "offset_y", to_string(offset_y).c_str(), 0);
    if (value < 0) {
        throw ScreenRecorderException("Failed to set video offset_y option");
    }
#elif __linux__
    value = av_dict_set(&options, "show_region", "1", 0);
    if (value < 0) {
        throw ScreenRecorderException("Failed to set video show_region option");
    }
#endif

    if (avformat_open_input(&video_decoder->avfc, video_decoder->filename, pAVInputFormat, &options) != 0) {
        throw ScreenRecorderException("Failed to open video input file");
    }

    if (avformat_find_stream_info(video_decoder->avfc, NULL) < 0) {
        throw ScreenRecorderException("Failed to get video stream info");
    }

    return 0;
}

#ifdef _WIN32
static std::string unicode2utf8(const WCHAR* uni) {
    static char temp[500];// max length of friendly name by UTF-16 is 128, so 500 in enough by utf-8
    memset(temp, 0, 500);
    WideCharToMultiByte(CP_UTF8, 0, uni, -1, temp, 500, NULL, NULL);
    return std::string(temp);
}
#endif

/**
* Opens audio input device.
*/
int ScreenRecorder::open_audio_media() throw() {

    audio_decoder = (StreamingContext*)calloc(1, sizeof(StreamingContext));
    audio_decoder->filename = (char*)malloc(50 * sizeof(char));
    
#ifdef _WIN32

    std::vector<std::string> vectorDevices;

    GUID guidValue;

    guidValue = CLSID_AudioInputDeviceCategory;

    WCHAR FriendlyName[128];
    HRESULT hr;

    vectorDevices.clear();

    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        throw ScreenRecorderException("Error while retrieving audio input device names");
    }

    ICreateDevEnum* pSysDevEnum = NULL;
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void**)&pSysDevEnum);
    if (FAILED(hr))
    {
        CoUninitialize();
        throw ScreenRecorderException("Error while retrieving audio input device names");
    }

    IEnumMoniker* pEnumCat = NULL;
    hr = pSysDevEnum->CreateClassEnumerator(guidValue, &pEnumCat, 0);
    if (hr == S_OK)
    {
        IMoniker* pMoniker = NULL;
        ULONG cFetched;
        while (pEnumCat->Next(1, &pMoniker, &cFetched) == S_OK)
        {
            IPropertyBag* pPropBag;
            hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void**)&pPropBag);
            if (SUCCEEDED(hr))
            {
                VARIANT varName;
                VariantInit(&varName);

                hr = pPropBag->Read(L"FriendlyName", &varName, NULL);
                if (SUCCEEDED(hr))
                {
                    StringCchCopy(FriendlyName, 128, varName.bstrVal);
                    vectorDevices.push_back(unicode2utf8(FriendlyName));
                }

                VariantClear(&varName);
                pPropBag->Release();
            }

            pMoniker->Release();
        } // End for While

        pEnumCat->Release();
    }

    pSysDevEnum->Release();
    CoUninitialize();

    string stringa = "audio=" + vectorDevices[0];
    strcpy(audio_decoder->filename, stringa.c_str());
#elif __linux__
    //strcpy(audio_decoder->filename, "sysdefault:CARD=I82801AAICH");
    strcpy(audio_decoder->filename, "hw:0");
#endif

    audio_encoder = (StreamingContext*)calloc(1, sizeof(StreamingContext));

    audio_decoder->avfc = avformat_alloc_context();

    if (!audio_decoder->avfc) { 
        throw ScreenRecorderException("Failed to alloc memory for audio input format");
    }

    AVInputFormat* pAVInputFormat = av_find_input_format(audio_input_format);

    AVDictionary* options = NULL;

    if (avformat_open_input(&audio_decoder->avfc, audio_decoder->filename, pAVInputFormat, &options) != 0) {
        throw ScreenRecorderException("Failed to open audio input file");
    }

    if (avformat_find_stream_info(audio_decoder->avfc, NULL) < 0) {
        throw ScreenRecorderException("Failed to get audio input stream info");
    }

    return 0;
}

/**
* Creates and initializes the structure used to decode video input packet.
*/
int ScreenRecorder::prepare_video_decoder() throw() {
    for (int i = 0; i < video_decoder->avfc->nb_streams; i++) {
        if (video_decoder->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_decoder->avs = video_decoder->avfc->streams[i];
            video_decoder->stream_index = i;

            video_decoder->avc = avcodec_find_decoder(video_decoder->avs->codecpar->codec_id);
            if (!video_decoder->avc) {
                throw ScreenRecorderException("Failed to find video codec");
            }

            video_decoder->avcc = avcodec_alloc_context3(video_decoder->avc);
            if (!video_decoder->avcc) {
                throw ScreenRecorderException("Failed to alloc memory for video input codec context");
            }


            if (avcodec_parameters_to_context(video_decoder->avcc, video_decoder->avs->codecpar) < 0) {
                throw ScreenRecorderException("Failed to fill video input codec context");
            }

            if (avcodec_open2(video_decoder->avcc, video_decoder->avc, NULL) < 0) {
                throw ScreenRecorderException("Failed to open video input codec");
            }
        }
        else {
            cout << "Skipping streams other than audio and video" << endl;
        }
    }

    return 0;
}
/**
* Creates and initializes the structure used to decode audio input packet.
*/
int ScreenRecorder::prepare_audio_decoder() throw() {
    for (int i = 0; i < audio_decoder->avfc->nb_streams; i++) {
        if (audio_decoder->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_decoder->avs = audio_decoder->avfc->streams[i];
            audio_decoder->stream_index = i;

            audio_decoder->avc = avcodec_find_decoder(audio_decoder->avs->codecpar->codec_id);
            if (!audio_decoder->avc) {
                throw ScreenRecorderException("Failed to find audio codec");
            }

            audio_decoder->avcc = avcodec_alloc_context3(audio_decoder->avc);
            if (!audio_decoder->avcc) {
                throw ScreenRecorderException("Failed to alloc memory for audio input codec context");
            }


            if (avcodec_parameters_to_context(audio_decoder->avcc, audio_decoder->avs->codecpar) < 0) {
                throw ScreenRecorderException("Failed to fill audio input codec context");
            }

            if (avcodec_open2(audio_decoder->avcc, audio_decoder->avc, NULL) < 0) {
                throw ScreenRecorderException("Failed to open audio input codec");
            }
        }
        else {
            cout << "Skipping streams other than audio and audio" << endl;
        }
    }

    return 0;
}
/**
* Creates and initializes the structure used to encode video input packet.
*/
int ScreenRecorder::prepare_video_encoder() throw() {
    video_encoder->avs = avformat_new_stream(out_avfc, NULL);

    video_encoder->avc = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_encoder->avc) {
        throw ScreenRecorderException("Failed to find video output codec");
    }

    video_encoder->avcc = avcodec_alloc_context3(video_encoder->avc);
    if (!video_encoder->avcc) {
        throw ScreenRecorderException("Failed to allocate memory for video output codec context");
    }

    av_opt_set(video_encoder->avcc, "preset", "ultrafast", 0);
#ifdef _WIN32
    av_opt_set(video_encoder->avcc, "cabac", "1", 0);
    av_opt_set(video_encoder->avcc, "ref", "3", 0);
    av_opt_set(video_encoder->avcc, "deblock", "1:0:0", 0);
    av_opt_set(video_encoder->avcc, "analyse", "0x3:0x113", 0);
    av_opt_set(video_encoder->avcc, "subme", "7", 0);
    av_opt_set(video_encoder->avcc, "chroma_qp_offset", "4", 0);
    av_opt_set(video_encoder->avcc, "rc", "crf", 0);
    av_opt_set(video_encoder->avcc, "rc_lookahead", "40", 0);
    av_opt_set(video_encoder->avcc, "crf", "10.0", 0);
#endif

    video_encoder->avcc->height = video_decoder->avcc->height;
    video_encoder->avcc->width = video_decoder->avcc->width;
    video_encoder->avcc->sample_aspect_ratio = video_decoder->avcc->sample_aspect_ratio;
    if (video_encoder->avc->pix_fmts)
        video_encoder->avcc->pix_fmt = video_encoder->avc->pix_fmts[0];
    else
        video_encoder->avcc->pix_fmt = video_decoder->avcc->pix_fmt;

    
    video_encoder->avcc->bit_rate = 2500000;

    video_encoder->avcc->time_base.num = 1;
    video_encoder->avcc->time_base.den = 15;

    video_encoder->avcc->gop_size = 15;     // 3
    video_encoder->avcc->max_b_frames = 2;

    video_encoder->avs->time_base = video_encoder->avcc->time_base;


    if (avcodec_open2(video_encoder->avcc, video_encoder->avc, NULL) < 0) {
        throw ScreenRecorderException("Failed to open the video output codec");
    }
    avcodec_parameters_from_context(video_encoder->avs->codecpar, video_encoder->avcc);
    return 0;
}
/**
* Creates and initializes the structure used to encode audio input packet.
*/
int ScreenRecorder::prepare_audio_encoder() throw() {
    audio_encoder->avs = avformat_new_stream(out_avfc, NULL);

    audio_encoder->avc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audio_encoder->avc) {
        throw ScreenRecorderException("Failed to find audio output codec");
    }

    audio_encoder->avcc = avcodec_alloc_context3(audio_encoder->avc);
    if (!audio_encoder->avcc) {
        throw ScreenRecorderException("Failed to allocate memory for audio output codec context");
    }

    audio_encoder->avcc->channels = audio_decoder->avs->codecpar->channels;
    audio_encoder->avcc->channel_layout = av_get_default_channel_layout(audio_decoder->avs->codecpar->channels);
    audio_encoder->avcc->sample_rate = audio_decoder->avs->codecpar->sample_rate;
    audio_encoder->avcc->sample_fmt = audio_encoder->avc->sample_fmts[0];
    audio_encoder->avcc->bit_rate = 32000;
    audio_encoder->avcc->time_base.num = 1;
    audio_encoder->avcc->time_base.den = audio_encoder->avcc->sample_rate;

    audio_encoder->avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    audio_encoder->avs->time_base = audio_encoder->avcc->time_base;

    if (avcodec_open2(audio_encoder->avcc, audio_encoder->avc, NULL) < 0) {
        throw ScreenRecorderException("Failed to open audio output codec");
    }
    avcodec_parameters_from_context(audio_encoder->avs->codecpar, audio_encoder->avcc);
    return 0;
}
/**
* Encodes input frames into output packets.
*/
int ScreenRecorder::encode_video(AVFrame* input_frame, int i) throw() {
    if (input_frame)
        input_frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket* output_packet = av_packet_alloc();
    if (!output_packet) {
        throw ScreenRecorderException("Failed to allocate memory for output packet");
    }

    int response = avcodec_send_frame(video_encoder->avcc, input_frame);

    while (response >= 0) {
        response = avcodec_receive_packet(video_encoder->avcc, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            throw ScreenRecorderException("Error while receiving packet from video_encoder");
        }

        output_packet->stream_index = video_decoder->stream_index;
        output_packet->duration = video_encoder->avs->time_base.den / video_encoder->avs->time_base.num / video_decoder->avs->avg_frame_rate.num * video_decoder->avs->avg_frame_rate.den;
        output_packet->dts = output_packet->pts = i * 1024;

        response = av_interleaved_write_frame(out_avfc, output_packet);
        if (response != 0) {
            throw ScreenRecorderException("Error while receiving packet from video_decoder");
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}
/**
* Decodes input packets and calls the method to encode them in output packets.
*/
int ScreenRecorder::transcode_video(AVPacket* input_packet, AVFrame* input_frame, int i) throw() {
    int response = avcodec_send_packet(video_decoder->avcc, input_packet);
    if (response < 0) {
        throw ScreenRecorderException("Error while sending packet to video_decoder");
    }

    while (response >= 0) {
        response = avcodec_receive_frame(video_decoder->avcc, input_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            throw ScreenRecorderException("Error while receiving frame from video_decoder");
        }

        int nbytes = av_image_get_buffer_size(video_encoder->avcc->pix_fmt, video_encoder->avcc->width, video_encoder->avcc->height, 32);
        uint8_t* video_outbuf = (uint8_t*)av_malloc(nbytes);
        if (video_outbuf == NULL)
        {
            throw ScreenRecorderException("Failed to allocate memory");
        }

        AVFrame* output_frame = av_frame_alloc();
        int value = av_image_fill_arrays(output_frame->data, output_frame->linesize, video_outbuf, AV_PIX_FMT_YUV420P, video_encoder->avcc->width, video_encoder->avcc->height, 1); // returns : the size in bytes required for src
        if (value < 0)
        {
            throw ScreenRecorderException("Error in filling image array");
        }
        SwsContext* swsCtx_;
        swsCtx_ = sws_getContext(video_decoder->avcc->width, video_decoder->avcc->height, video_decoder->avcc->pix_fmt, video_encoder->avcc->width, video_encoder->avcc->height, video_encoder->avcc->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

        sws_scale(swsCtx_, input_frame->data, input_frame->linesize, 0, video_decoder->avcc->height, output_frame->data, output_frame->linesize);

        output_frame->width = video_encoder->avcc->width;
        output_frame->height = video_encoder->avcc->height;
        output_frame->format = video_encoder->avcc->pix_fmt;
        output_frame->pts = input_frame->pts;

        if (response >= 0) {
            try {
                encode_video(output_frame, i);
            }
            catch (ScreenRecorderException e) {
                throw e;
            }
        }
        av_frame_unref(input_frame);
    }
    return 0;
}
