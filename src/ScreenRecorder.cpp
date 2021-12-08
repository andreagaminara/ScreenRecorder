//
// Created by andrea on 10/11/21.
//

#include "../include/ScreenRecorder.h"

using namespace std;

ScreenRecorder::ScreenRecorder()
{

    //av_register_all();
    //avcodec_register_all();
    //avdevice_register_all();
}

/* uninitialize the resources */
ScreenRecorder::~ScreenRecorder(){
}

int ScreenRecorder::capture(int numFrames) {

    if (open_media())
        return -1;
    if (prepare_decoder())
        return -1;

    avformat_alloc_output_context2(&(encoder->avfc), NULL, NULL, encoder->filename);
    if (!encoder->avfc) {
        cout << "could not allocate memory for output format" << endl;
        return -1;
    }

    if (prepare_video_encoder()) {
        return -1;
    }
    /*if (prepare_audio_encoder()) {
        return -1;
    }*/

    if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
        encoder->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (!(encoder->avfc->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&encoder->avfc->pb, encoder->filename, AVIO_FLAG_WRITE) < 0) {
            cout << "could not open the output file" << endl;
            return -1;
        }
    }

    AVDictionary* muxer_opts = NULL;

    if (avformat_write_header(encoder->avfc, &muxer_opts) < 0) {
        cout << "an error occurred when opening output file" << endl;
        return -1;
    }

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

    int i = 0;

    while (av_read_frame(decoder->avfc, input_packet) >= 0)
    {
        if(i++ == numFrames)
            break;

        if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            // TODO: refactor to be generic for audio and video (receiving a function pointer to the differences)
            if (transcode_video(input_packet, input_frame))
                return -1;
            av_packet_unref(input_packet);

        }
        /*
        else if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)  {

            if (transcode_audio(decoder, encoder, input_packet, input_frame)) return -1;
            av_packet_unref(input_packet);
        }*/
        else {
            cout << "ignoring all non video or audio packets" << endl;
        }
    }
    // TODO: should I also flush the audio encoder?
    if (encode_video(NULL))
        return -1;

    av_write_trailer(encoder->avfc);

    if (muxer_opts != NULL) {
        av_dict_free(&muxer_opts);
        muxer_opts = NULL;
    }

    if (input_frame != NULL) {
        av_frame_free(&input_frame);
        input_frame = NULL;
    }

    if (input_packet != NULL) {
        av_packet_free(&input_packet);
        input_packet = NULL;
    }

    avformat_close_input(&decoder->avfc);

    avformat_free_context(decoder->avfc); decoder->avfc = NULL;
    avformat_free_context(encoder->avfc); encoder->avfc = NULL;

    avcodec_free_context(&decoder->video_avcc);
    decoder->video_avcc = NULL;
    //avcodec_free_context(&decoder->audio_avcc);
    //decoder->audio_avcc = NULL;

    free(decoder);
    decoder = NULL;
    free(encoder);
    encoder = NULL;
    return 0;

}

int ScreenRecorder::open_media() {

    av_register_all();
    avcodec_register_all();
    avdevice_register_all();

    decoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    decoder->filename = ":0.0+0,0";

    encoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
    encoder->filename = "../media/output.mp4";

    decoder->avfc = avformat_alloc_context();

    if (!decoder->avfc) {
        cout << "failed to alloc memory for format" << endl;
        return -1;
    }

    AVInputFormat *pAVInputFormat = av_find_input_format("x11grab");

    AVDictionary *options = NULL;
    int value = av_dict_set(&options, "video_size", "1920x960", 0);
    if (value < 0) {
        cout << "no option" << endl;
        return -1;
    }

    if (avformat_open_input(&decoder->avfc, ":0.0+0,0", pAVInputFormat, &options) != 0) {
        cout << "failed to open input file " << decoder->filename << endl;
        return -1;
    }

    if (avformat_find_stream_info(decoder->avfc, NULL) < 0) {
        cout << "failed to get stream info" << endl;
        return -1;
    }

    return 0;
}

int ScreenRecorder::prepare_decoder() {
    for (int i = 0; i < decoder->avfc->nb_streams; i++) {
        if (decoder->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            decoder->video_avs = decoder->avfc->streams[i];
            decoder->video_index = i;

            decoder->video_avc = avcodec_find_decoder(decoder->video_avs->codecpar->codec_id);
            if (!decoder->video_avc) {
                cout << "failed to find the codec" << endl;
                return -1;
            }

            decoder->video_avcc = avcodec_alloc_context3(decoder->video_avc);
            if (!decoder->video_avcc) {
                cout << "failed to alloc memory for codec context" << endl;
                return -1;
            }


            if (avcodec_parameters_to_context(decoder->video_avcc, decoder->video_avs->codecpar) < 0) {
                cout << "failed to fill codec context" << endl;
                return -1;
            }

            if (avcodec_open2(decoder->video_avcc, decoder->video_avc, NULL) < 0) {
                cout << "failed to open codec" << endl;
                return -1;
            }
        }
        /*
        else if (decoder->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            decoder->audio_avs = decoder->avfc->streams[i];
            decoder->audio_index = i;

            *avc = avcodec_find_decoder(avs->codecpar->codec_id);
            if (!*avc) {logging("failed to find the codec"); return -1;}

            *avcc = avcodec_alloc_context3(*avc);
            if (!*avcc) {logging("failed to alloc memory for codec context"); return -1;}

            if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0) {logging("failed to fill codec context"); return -1;}

            if (avcodec_open2(*avcc, *avc, NULL) < 0) {logging("failed to open codec"); return -1;}
        } */
        else {
            cout << "skipping streams other than audio and video" << endl;
        }
    }

    return 0;
}

int ScreenRecorder::prepare_video_encoder() {
    AVRational input_framerate = av_guess_frame_rate(decoder->avfc, decoder->video_avs, NULL);
    encoder->video_avs = avformat_new_stream(encoder->avfc, NULL);

    encoder->video_avc = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder->video_avc) {
        cout << "could not find the proper codec" << endl;
        return -1;
    }

    encoder->video_avcc = avcodec_alloc_context3(encoder->video_avc);
    if (!encoder->video_avcc) {
        cout << "could not allocated memory for codec context" << endl;
        return -1;
    }

    av_opt_set(encoder->video_avcc->priv_data, "preset", "slow", 0);

    //avcodec_parameters_from_context(encoder->video_avs->codecpar, encoder->video_avcc);
    encoder->video_avcc->height = decoder->video_avcc->height;
    encoder->video_avcc->width = decoder->video_avcc->width;
    encoder->video_avcc->sample_aspect_ratio = decoder->video_avcc->sample_aspect_ratio;
    if (encoder->video_avc->pix_fmts)
        encoder->video_avcc->pix_fmt = encoder->video_avc->pix_fmts[0];
    else
        encoder->video_avcc->pix_fmt = decoder->video_avcc->pix_fmt;

    encoder->video_avcc->bit_rate = 400000;
    //encoder->video_avcc->rc_buffer_size = 4 * 1000 * 1000;
    //encoder->video_avcc->rc_max_rate = 2 * 1000 * 1000;
    //encoder->video_avcc->rc_min_rate = 2.5 * 1000 * 1000;

    //encoder->video_avcc->time_base = av_inv_q(input_framerate);
    encoder->video_avcc->time_base.num = 1;
    encoder->video_avcc->time_base.den = 15;

    encoder->video_avs->time_base = encoder->video_avcc->time_base;

    if (avcodec_open2(encoder->video_avcc, encoder->video_avc, NULL) < 0) {
        cout << "could not open the codec" << endl;
        return -1;
    }
    avcodec_parameters_from_context(encoder->video_avs->codecpar, encoder->video_avcc);
    return 0;
}

int ScreenRecorder::encode_video(AVFrame *input_frame) {
    if (input_frame)
        input_frame->pict_type = AV_PICTURE_TYPE_NONE;

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
        cout <<"could not allocate memory for output packet" << endl;
        return -1;
    }

    int response = avcodec_send_frame(encoder->video_avcc, input_frame);

    while (response >= 0) {
        response = avcodec_receive_packet(encoder->video_avcc, output_packet);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            cout << "Error while receiving packet from encoder" << endl;
            return -1;
        }

        output_packet->stream_index = decoder->video_index;
        output_packet->duration = encoder->video_avs->time_base.den / encoder->video_avs->time_base.num / decoder->video_avs->avg_frame_rate.num * decoder->video_avs->avg_frame_rate.den;

        av_packet_rescale_ts(output_packet, decoder->video_avs->time_base, encoder->video_avs->time_base);
        response = av_interleaved_write_frame(encoder->avfc, output_packet);
        if (response != 0) {
            cout << "Error "<< response <<" while receiving packet from decoder" << endl;
            return -1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

int ScreenRecorder::transcode_video(AVPacket *input_packet, AVFrame *input_frame) {
    int response = avcodec_send_packet(decoder->video_avcc, input_packet);
    if (response < 0) {
        cout << "Error while sending packet to decoder" << endl;
        return response;
    }

    while (response >= 0) {
        response = avcodec_receive_frame(decoder->video_avcc, input_frame);
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
            break;
        }
        else if (response < 0) {
            cout << "Error while receiving frame from decoder" << endl;
            return response;
        }

        int nbytes = av_image_get_buffer_size(encoder->video_avcc->pix_fmt,encoder->video_avcc->width,encoder->video_avcc->height,32);
        uint8_t *video_outbuf = (uint8_t*)av_malloc(nbytes);
        if( video_outbuf == NULL )
        {
            cout<<"\nunable to allocate memory";
            exit(1);
        }

        AVFrame *output_frame = av_frame_alloc();
        int value = av_image_fill_arrays( output_frame->data, output_frame->linesize, video_outbuf , AV_PIX_FMT_YUV420P, encoder->video_avcc->width,encoder->video_avcc->height,1 ); // returns : the size in bytes required for src
        if(value < 0)
        {
            cout<<"error in filling image array" <<endl;
        }
        SwsContext* swsCtx_ ;
        swsCtx_ = sws_getContext(decoder->video_avcc->width, decoder->video_avcc->height, decoder->video_avcc->pix_fmt, encoder->video_avcc->width, encoder->video_avcc->height, encoder->video_avcc->pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);

        sws_scale(swsCtx_, input_frame->data, input_frame->linesize,0, decoder->video_avcc->height, output_frame->data, output_frame->linesize);

        output_frame->width = encoder->video_avcc->width;
        output_frame->height = encoder->video_avcc->height;
        output_frame->format = encoder->video_avcc->pix_fmt;
        output_frame->pts = input_frame->pts;

        if (response >= 0) {
            if (encode_video(output_frame)) return -1;
        }
        av_frame_unref(input_frame);
    }
    return 0;
}