//
// Created by andrea on 10/11/21.
//

#include "../include/ScreenRecorder.h"

using namespace std;

ScreenRecorder::ScreenRecorder()
{
    //avdevice_register_all();
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder(){
}

int ScreenRecorder::capture() {

    avdevice_register_all();

    output_filename = (char *)malloc(50*sizeof(char));
    strcpy(output_filename, "../media/output.mp4");

    video_input_format = (char *)malloc(50*sizeof(char));
    strcpy(video_input_format, "x11grab");

    audio_input_format = (char *)malloc(50*sizeof(char));
    strcpy(audio_input_format, "alsa");


    if (open_video_media()){
        cout << "open_video_media crash!" << endl;
        return -1;
    }

    if (open_audio_media()){
        cout << "open_audio_media crash!" << endl;
        return -1;
    }

    if (prepare_video_decoder()) {
        cout << "prepare_video_decoder crash!" << endl;
        return -1;
    }

    if (prepare_audio_decoder()){
        cout << "prepare_audio_decoder crash!" << endl;
        return -1;
    }

    avformat_alloc_output_context2(&(out_avfc), NULL, NULL, output_filename);
    if (!out_avfc) {
        cout << "could not allocate memory for output format" << endl;
        return -1;
    }

    if (prepare_video_encoder()) {
        cout << "prepare_video_encoder crash!" << endl;
        return -1;
    }
    if (prepare_audio_encoder()) {
        cout << "prepare_audio_encoder crash!" << endl;
        return -1;
    }

    if (out_avfc->oformat->flags & AVFMT_GLOBALHEADER)
        out_avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(out_avfc->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&out_avfc->pb, output_filename, AVIO_FLAG_WRITE) < 0) {
            cout << "could not open the output file" << endl;
            return -1;
        }
    }

    AVDictionary* muxer_opts = NULL;

    if (avformat_write_header(out_avfc, &muxer_opts) < 0) {
        cout << "an error occurred when opening output file" << endl;
        return -1;
    }

    //TODO: qui sganciare i threads
    isRunning = true;
    video_thread = new thread([this](){
        this->capture_video();
    });

   audio_thread = new thread([this](){
       this->capture_audio();
   });
    this_thread::sleep_for(20s);
    isRunning.exchange(false);
    video_thread->join();
    audio_thread->join();

    av_write_trailer(out_avfc);

    if (muxer_opts != NULL) {
        av_dict_free(&muxer_opts);
        muxer_opts = NULL;
    }

    avformat_close_input(&video_decoder->avfc);
    avformat_free_context(video_decoder->avfc);
    video_decoder->avfc = NULL;

    avformat_close_input(&audio_decoder->avfc);
    avformat_free_context(audio_decoder->avfc);
    audio_decoder->avfc = NULL;

    avformat_free_context(out_avfc);
    out_avfc = NULL;

    avcodec_free_context(&video_decoder->avcc);
    video_decoder->avcc = NULL;
    avcodec_free_context(&audio_decoder->avcc);
    audio_decoder->avcc = NULL;

    free(video_decoder);
    video_decoder = NULL;
    free(video_encoder);
    video_encoder = NULL;

    return 0;

}

int ScreenRecorder::capture_video(){
    int numFrames = 100;
    AVFrame *input_frame = av_frame_alloc();
    if (!input_frame) {
        cout << "failed to allocated memory for AVFrame" << endl;
        return -1;
    }

    AVPacket *input_packet = av_packet_alloc();
    if (!input_packet) {
        cout << "failed to allocated memory for AVPacket" << endl;
        return -1;
    }

    int i=0;

    while (isRunning)
    {
        if(av_read_frame(video_decoder->avfc, input_packet) < 0)
            cout << "Error reading video frame" << endl;

        if (video_decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            // TODO: refactor to be generic for audio and video (receiving a function pointer to the differences)
            if (transcode_video(input_packet, input_frame, i))
                return -1;
            av_packet_unref(input_packet);

        }
        else {
            cout << "ignoring all non video or audio packets" << endl;
        }

        i++;
    }
    // TODO: should I also flush the audio video_encoder?
    //if (encode_video(NULL))
      //  return -1;

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

int ScreenRecorder::capture_audio() {
    int numFrames = 100;
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
    
    AVFrame *input_frame = av_frame_alloc();
    if (!input_frame) {
        cout << "failed to allocated memory for AVFrame" << endl;
        return -1;
    }

    AVPacket *input_packet = av_packet_alloc();
    if (!input_packet) {
        cout << "failed to allocated memory for AVPacket" << endl;
        return -1;
    }

    AVPacket *outputPacket = av_packet_alloc();
    if (!outputPacket) {
        cout << "failed to allocated memory for AVPacket" << endl;
        return -1;
    }

    int i = 0;

    while (isRunning)
    {
        if(av_read_frame(audio_decoder->avfc, input_packet) < 0){
            cout << "Error reading audio frame" << endl;
            return -1;
        }

        if (audio_decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)  {

            if (transcode_audio(input_packet, input_frame))
                return -1;

            int ret;

            while (av_audio_fifo_size(audioFifo) >= audio_encoder->avcc->frame_size) {
                AVFrame* outputFrame = av_frame_alloc();
                outputFrame->nb_samples = audio_encoder->avcc->frame_size;
                outputFrame->channels = audio_decoder->avcc->channels;
                outputFrame->channel_layout = av_get_default_channel_layout(audio_decoder->avcc->channels);
                outputFrame->format = AV_SAMPLE_FMT_FLTP;
                outputFrame->sample_rate = audio_encoder->avcc->sample_rate;

                ret = av_frame_get_buffer(outputFrame, 0);
                if(ret < 0){
                    cout << "errore" << endl;
                }
                ret = av_audio_fifo_read(audioFifo, (void**)outputFrame->data, audio_encoder->avcc->frame_size);
                if(ret < 0){
                    cout << "errore" << endl;
                }

                //outputFrame->pts = i * audio_encoder->avs->time_base.den * 1024 / audio_encoder->avcc->sample_rate;
                outputFrame->pts = i * 1024;

                ret = avcodec_send_frame(audio_encoder->avcc, outputFrame);
                if (ret < 0) {
                    cout << "Fail to send frame in encoding" << endl;
                }
                av_frame_free(&outputFrame);
                ret = avcodec_receive_packet(audio_encoder->avcc, outputPacket);
                if (ret == AVERROR(EAGAIN)) {
                    continue;
                }
                else if (ret < 0) {
                    cout << "Fail to receive packet in encoding" << endl;
                }


                outputPacket->stream_index = audio_encoder->avs->index;
                outputPacket->duration = audio_encoder->avs->time_base.den * 1024 / audio_encoder->avcc->sample_rate;
                //outputPacket->dts = outputPacket->pts = i * audio_encoder->avs->time_base.den * 1024 / audio_encoder->avcc->sample_rate;
                outputPacket->dts = outputPacket->pts = i * 1024;

                i++;

                ret = av_write_frame(out_avfc, outputPacket);
                av_packet_unref(outputPacket);

            }
        }
        else {
            cout << "ignoring all non video or audio packets" << endl;
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

int ScreenRecorder::transcode_audio(AVPacket *input_packet, AVFrame *input_frame) {

    uint8_t **cSamples = nullptr;

    int response = avcodec_send_packet(audio_decoder->avcc, input_packet);
    if (response < 0) {
        cout << "Error while sending packet to audio_decoder" << endl;
        return response;
    }

    response = avcodec_receive_frame(audio_decoder->avcc, input_frame);
    if (response < 0) {
        cout << "Error while receiving frame from audio_decoder" << endl;
        return response;
    }

    response = av_samples_alloc_array_and_samples(&cSamples, NULL, audio_encoder->avcc->channels, input_frame->nb_samples, AV_SAMPLE_FMT_FLTP, 0);
    if (response < 0) {
        cout << "Fail to alloc samples by av_samples_alloc_array_and_samples." << endl;
        return response;
    }

    response = swr_convert(audioConverter, cSamples, input_frame->nb_samples, (const uint8_t**)input_frame->extended_data, input_frame->nb_samples);
    if (response < 0) {
        cout << "Fail to swr_convert." << endl;
        return response;
    }
    if (av_audio_fifo_space(audioFifo) < input_frame->nb_samples){
        cout << "audio buffer is too small." << endl;
        return response;
    }

    response = av_audio_fifo_write(audioFifo, (void**)cSamples, input_frame->nb_samples);
    if (response < 0) {
        cout << "Fail to write fifo" << endl;
        return response;
    }

    av_freep(&cSamples[0]);

    av_frame_unref(input_frame);
    av_packet_unref(input_packet);

    return 0;
}

int ScreenRecorder::open_video_media() {


    video_decoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    video_decoder->filename = (char *) malloc(50*sizeof(char));
    strcpy(video_decoder->filename, ":0.0+0,0");

    video_encoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));

    video_decoder->avfc = avformat_alloc_context();

    if (!video_decoder->avfc) {
        cout << "failed to alloc memory for format" << endl;
        return -1;
    }

    AVInputFormat *pAVInputFormat = av_find_input_format(video_input_format);

    AVDictionary *options = NULL;
    int value = av_dict_set(&options, "video_size", "1920x950", 0);
    if (value < 0) {
        cout << "no option" << endl;
        return -1;
    }

    if (avformat_open_input(&video_decoder->avfc, video_decoder->filename, pAVInputFormat, &options) != 0) {
        cout << "failed to open input file " << video_decoder->filename << endl;
        return -1;
    }

    if (avformat_find_stream_info(video_decoder->avfc, NULL) < 0) {
        cout << "failed to get stream info" << endl;
        return -1;
    }

    return 0;
}

int ScreenRecorder::open_audio_media() {

    audio_decoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    audio_decoder->filename = (char *) malloc(50*sizeof(char));
    strcpy(audio_decoder->filename, "sysdefault:CARD=I82801AAICH");

    audio_encoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));

    audio_decoder->avfc = avformat_alloc_context();

    if (!audio_decoder->avfc) {
        cout << "failed to alloc memory for format" << endl;
        return -1;
    }

    AVInputFormat *pAVInputFormat = av_find_input_format(audio_input_format);

    if (avformat_open_input(&audio_decoder->avfc, audio_decoder->filename, pAVInputFormat, NULL) != 0) {
        cout << "failed to open input file " << audio_decoder->filename << endl;
        return -1;
    }

    if (avformat_find_stream_info(audio_decoder->avfc, NULL) < 0) {
        cout << "failed to get stream info" << endl;
        return -1;
    }

    return 0;
}

int ScreenRecorder::prepare_video_decoder() {
    for (int i = 0; i < video_decoder->avfc->nb_streams; i++) {
        if (video_decoder->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_decoder->avs = video_decoder->avfc->streams[i];
            video_decoder->stream_index = i;

            video_decoder->avc = avcodec_find_decoder(video_decoder->avs->codecpar->codec_id);
            if (!video_decoder->avc) {
                cout << "failed to find the codec" << endl;
                return -1;
            }

            video_decoder->avcc = avcodec_alloc_context3(video_decoder->avc);
            if (!video_decoder->avcc) {
                cout << "failed to alloc memory for codec context" << endl;
                return -1;
            }


            if (avcodec_parameters_to_context(video_decoder->avcc, video_decoder->avs->codecpar) < 0) {
                cout << "failed to fill codec context" << endl;
                return -1;
            }

            if (avcodec_open2(video_decoder->avcc, video_decoder->avc, NULL) < 0) {
                cout << "failed to open codec" << endl;
                return -1;
            }
        }
        else {
            cout << "skipping streams other than audio and video" << endl;
        }
    }

    return 0;
}

int ScreenRecorder::prepare_audio_decoder() {
    for (int i = 0; i < audio_decoder->avfc->nb_streams; i++) {
        if (audio_decoder->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_decoder->avs = audio_decoder->avfc->streams[i];
            audio_decoder->stream_index = i;

            audio_decoder->avc = avcodec_find_decoder(audio_decoder->avs->codecpar->codec_id);
            if (!audio_decoder->avc) {
                cout << "failed to find the codec" << endl;
                return -1;
            }

            audio_decoder->avcc = avcodec_alloc_context3(audio_decoder->avc);
            if (!audio_decoder->avcc) {
                cout << "failed to alloc memory for codec context" << endl;
                return -1;
            }


            if (avcodec_parameters_to_context(audio_decoder->avcc, audio_decoder->avs->codecpar) < 0) {
                cout << "failed to fill codec context" << endl;
                return -1;
            }

            if (avcodec_open2(audio_decoder->avcc, audio_decoder->avc, NULL) < 0) {
                cout << "failed to open codec" << endl;
                return -1;
            }
        }
        else {
            cout << "skipping streams other than audio and audio" << endl;
        }
    }

    return 0;
}

int ScreenRecorder::prepare_video_encoder() {
    AVRational input_framerate = av_guess_frame_rate(video_decoder->avfc, video_decoder->avs, NULL);
    video_encoder->avs = avformat_new_stream(out_avfc, NULL);

    video_encoder->avc = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_encoder->avc) {
        cout << "could not find the proper codec" << endl;
        return -1;
    }

    video_encoder->avcc = avcodec_alloc_context3(video_encoder->avc);
    if (!video_encoder->avcc) {
        cout << "could not allocated memory for codec context" << endl;
        return -1;
    }

    av_opt_set(video_encoder->avcc->priv_data, "preset", "slow", 0);

    //avcodec_parameters_from_context(video_encoder->avs->codecpar, video_encoder->avcc);
    video_encoder->avcc->height = video_decoder->avcc->height;
    video_encoder->avcc->width = video_decoder->avcc->width;
    video_encoder->avcc->sample_aspect_ratio = video_decoder->avcc->sample_aspect_ratio;
    if (video_encoder->avc->pix_fmts)
        video_encoder->avcc->pix_fmt = video_encoder->avc->pix_fmts[0];
    else
        video_encoder->avcc->pix_fmt = video_decoder->avcc->pix_fmt;

    video_encoder->avcc->bit_rate = 400000;
    //video_encoder->avcc->rc_buffer_size = 4 * 1000 * 1000;
    //video_encoder->avcc->rc_max_rate = 2 * 1000 * 1000;
    //video_encoder->avcc->rc_min_rate = 2.5 * 1000 * 1000;

    //video_encoder->avcc->time_base = av_inv_q(input_framerate);
    video_encoder->avcc->time_base.num = 1;
    video_encoder->avcc->time_base.den = 15;

    video_encoder->avs->time_base = video_encoder->avcc->time_base;

    if (avcodec_open2(video_encoder->avcc, video_encoder->avc, NULL) < 0) {
        cout << "could not open the codec" << endl;
        return -1;
    }
    avcodec_parameters_from_context(video_encoder->avs->codecpar, video_encoder->avcc);
    return 0;
}

int ScreenRecorder::prepare_audio_encoder(){
    audio_encoder->avs = avformat_new_stream(out_avfc, NULL);

    audio_encoder->avc = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (!audio_encoder->avc) {
        cout << "could not find the proper codec" << endl;
        return -1;
    }

    audio_encoder->avcc = avcodec_alloc_context3(audio_encoder->avc);
    if (!audio_encoder->avcc) {
        cout << "could not allocated memory for codec context" << endl;
        return -1;
    }

    //int OUTPUT_CHANNELS = 2;
    //int OUTPUT_BIT_RATE = 196000;
    audio_encoder->avcc->channels       = audio_decoder->avs->codecpar->channels;
    audio_encoder->avcc->channel_layout = av_get_default_channel_layout(audio_decoder->avs->codecpar->channels);
    audio_encoder->avcc->sample_rate    = audio_decoder->avs->codecpar->sample_rate;
    audio_encoder->avcc->sample_fmt     = audio_encoder->avc->sample_fmts[0];
    audio_encoder->avcc->bit_rate       = 32000;
    //audio_encoder->avcc->time_base      = (AVRational){1, audio_encoder->avcc->sample_rate};
    audio_encoder->avcc->time_base.num = 1;
    audio_encoder->avcc->time_base.den = audio_encoder->avcc->sample_rate;

    audio_encoder->avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

    audio_encoder->avs->time_base = audio_encoder->avcc->time_base;

    if (avcodec_open2(audio_encoder->avcc, audio_encoder->avc, NULL) < 0) {
        cout << "could not open the codec" << endl;
        return -1;
    }
    avcodec_parameters_from_context(audio_encoder->avs->codecpar, audio_encoder->avcc);
    return 0;
}

int ScreenRecorder::encode_video(AVFrame *input_frame, int i) {
    if (input_frame)
        input_frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
        cout <<"could not allocate memory for output packet" << endl;
        return -1;
    }

    int response = avcodec_send_frame(video_encoder->avcc, input_frame);

    while (response >= 0) {
        response = avcodec_receive_packet(video_encoder->avcc, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            cout << "Error while receiving packet from video_encoder" << endl;
            return -1;
        }

        output_packet->stream_index = video_decoder->stream_index;
        output_packet->duration = video_encoder->avs->time_base.den / video_encoder->avs->time_base.num / video_decoder->avs->avg_frame_rate.num * video_decoder->avs->avg_frame_rate.den;
        output_packet->dts = output_packet->pts = i * 1024;
        //av_packet_rescale_ts(output_packet, video_decoder->avs->time_base, video_encoder->avs->time_base);

        response = av_interleaved_write_frame(out_avfc, output_packet);
        if (response != 0) {
            cout << "Error "<< response <<" while receiving packet from video_decoder" << endl;
            return -1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

int ScreenRecorder::transcode_video(AVPacket *input_packet, AVFrame *input_frame, int i) {
    int response = avcodec_send_packet(video_decoder->avcc, input_packet);
    if (response < 0) {
        cout << "Error while sending packet to video_decoder" << endl;
        return response;
    }

    while (response >= 0) {
        response = avcodec_receive_frame(video_decoder->avcc, input_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            cout << "Error while receiving frame from video_decoder" << endl;
            return response;
        }

        int nbytes = av_image_get_buffer_size(video_encoder->avcc->pix_fmt, video_encoder->avcc->width, video_encoder->avcc->height, 32);
        uint8_t *video_outbuf = (uint8_t*)av_malloc(nbytes);
        if( video_outbuf == NULL )
        {
            cout<<"\nunable to allocate memory";
            exit(1);
        }

        AVFrame *output_frame = av_frame_alloc();
        int value = av_image_fill_arrays(output_frame->data, output_frame->linesize, video_outbuf , AV_PIX_FMT_YUV420P, video_encoder->avcc->width, video_encoder->avcc->height, 1 ); // returns : the size in bytes required for src
        if(value < 0)
        {
            cout<<"error in filling image array" <<endl;
        }
        SwsContext* swsCtx_ ;
        swsCtx_ = sws_getContext(video_decoder->avcc->width, video_decoder->avcc->height, video_decoder->avcc->pix_fmt, video_encoder->avcc->width, video_encoder->avcc->height, video_encoder->avcc->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

        sws_scale(swsCtx_, input_frame->data, input_frame->linesize, 0, video_decoder->avcc->height, output_frame->data, output_frame->linesize);

        output_frame->width = video_encoder->avcc->width;
        output_frame->height = video_encoder->avcc->height;
        output_frame->format = video_encoder->avcc->pix_fmt;
        output_frame->pts = input_frame->pts;

        if (response >= 0) {
            if (encode_video(output_frame, i)) return -1;
        }
        av_frame_unref(input_frame);
    }
    return 0;
}