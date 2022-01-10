#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

typedef struct StreamParams {
    char copy_video;
    char copy_audio;
    char *output_extension;
    char *muxer_opt_key;
    char *muxer_opt_value;
    char *video_codec;
    char *audio_codec;
    char *codec_priv_key;
    char *codec_priv_value;
} StreamParams;

typedef struct StreamContext {
    AVFormatContext *avfmt_ctx;
    AVCodec *vcodec;
    AVCodec *acodec;
    AVStream *vstream;
    AVStream *astream;
    AVCodecContext *vcodec_ctx;
    AVCodecContext *acodec_ctx;
    int vindex;
    int aindex;
    char *filename;
} StreamContext;

static int fill_stream_info(AVStream *av_stream, AVCodec **av_codec, AVCodecContext **av_codec_ctx) {
    int ret = 0;
    *av_codec = avcodec_find_decoder(av_stream->codecpar->codec_id);
    if (!*av_codec) {
        av_log(NULL, AV_LOG_ERROR, "Not find this codec\n");
        return -1;
    }

    *av_codec_ctx = avcodec_alloc_context3(*av_codec);
    if (!*av_codec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to alloc memory for codec context\n");
        return -1;
    }

    if ((ret = avcodec_parameters_to_context(*av_codec_ctx, av_stream->codecpar)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to fill codec context\n");
        return ret;
    }

    if ((ret = avcodec_open2(*av_codec_ctx, *av_codec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open codec\n");
        return ret;
    }
    return ret;
}

static int load_input_file(const char *filename, AVFormatContext **avfmt_ctx) {
    int ret = 0;
    *avfmt_ctx = avformat_alloc_context();
    if (!*avfmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Alloc AVFormatContext failed\n");
        return -1;
    }

    if ((ret = avformat_open_input(avfmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(*avfmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }
    return ret;
}

static int load_input_decoder(StreamContext *sc) {
    unsigned int i;
    for (i = 0; i < sc->avfmt_ctx->nb_streams; i++) {
        if (sc->avfmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            sc->vstream = sc->avfmt_ctx->streams[i];
            sc->vindex = i;
            if (fill_stream_info(sc->vstream, &sc->vcodec, &sc->vcodec_ctx)) {
                return -1;
            }
        } else if (sc->avfmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            sc->astream = sc->avfmt_ctx->streams[i];
            sc->aindex = i;
            if (fill_stream_info(sc->astream, &sc->acodec, &sc->acodec_ctx)) {
                return -1;
            }
        } else {
            av_log(NULL, AV_LOG_ERROR, "Skipping streams other than audio and video\n");
        }
    }
    return 0;
}

static int open_input_file(StreamContext *dec_ctx) {
    int ret = 0;
    // 加载输入文件/流
    if ((ret = load_input_file(dec_ctx->filename, &dec_ctx->avfmt_ctx) < 0)) {
        av_log(NULL, AV_LOG_ERROR, "load input file failed!\n");
        return ret;
    }
    // 加载解码器
    if ((ret = load_input_decoder(dec_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "load input coder failed!\n");
        return ret;
    }
    return ret;
}

static int load_output_file(const char *filename, AVFormatContext **av_fmt_ctx) {
    int ret = 0;
    avformat_alloc_output_context2(av_fmt_ctx, NULL, NULL, filename);
    if (!*av_fmt_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate memory for output format!\n");
        return -1;
    }
    return ret;
}

static int load_video_encoder(StreamContext *dec_ctx, StreamContext *enc_ctx, StreamParams *sp) {
    AVRational input_framerate = av_guess_frame_rate(dec_ctx->avfmt_ctx, dec_ctx->vstream, NULL);
    enc_ctx->vstream = avformat_new_stream(enc_ctx->avfmt_ctx, NULL);

    enc_ctx->vcodec = avcodec_find_encoder_by_name(sp->video_codec);
    if (!enc_ctx->vcodec) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the proper codec!\n");
        return -1;
    }

    enc_ctx->vcodec_ctx = avcodec_alloc_context3(enc_ctx->vcodec);
    if (!enc_ctx->vcodec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocated memory for codec context!\n");
        return -1;
    }
    
    // 设置参数
    // av_opt_set(enc_ctx->vcodec_ctx->priv_data, "preset", "fast", 0);
    enc_ctx->vcodec_ctx->height = dec_ctx->vcodec_ctx->height;
    enc_ctx->vcodec_ctx->width = dec_ctx->vcodec_ctx->width;
    enc_ctx->vcodec_ctx->sample_aspect_ratio = dec_ctx->vcodec_ctx->sample_aspect_ratio;
    if (enc_ctx->vcodec->pix_fmts) {
        enc_ctx->vcodec_ctx->pix_fmt = enc_ctx->vcodec->pix_fmts[0];
    } else {
        enc_ctx->vcodec_ctx->pix_fmt = dec_ctx->vcodec_ctx->pix_fmt;
    }
    printf("%lld\n", dec_ctx->vcodec_ctx->bit_rate);
    enc_ctx->vcodec_ctx->bit_rate = dec_ctx->vcodec_ctx->bit_rate;
    enc_ctx->vcodec_ctx->rc_buffer_size = dec_ctx->vcodec_ctx->rc_buffer_size;
    enc_ctx->vcodec_ctx->rc_max_rate = dec_ctx->vcodec_ctx->rc_max_rate;
    enc_ctx->vcodec_ctx->rc_min_rate = dec_ctx->vcodec_ctx->rc_min_rate;
    enc_ctx->vcodec_ctx->time_base = av_inv_q(input_framerate);
    enc_ctx->vstream->time_base = enc_ctx->vcodec_ctx->time_base;
    
    if (avcodec_open2(enc_ctx->vcodec_ctx, enc_ctx->vcodec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open the codec!\n");
        return -1;
    }
    avcodec_parameters_from_context(enc_ctx->vstream->codecpar, enc_ctx->vcodec_ctx);
    return 0;
}

static int load_audio_encoder(StreamContext *enc_ctx, int sample_rate, StreamParams *sp) {
    enc_ctx->astream = avformat_new_stream(enc_ctx->avfmt_ctx, NULL);

    enc_ctx->acodec = avcodec_find_encoder_by_name(sp->audio_codec);
    if (!enc_ctx->acodec) {
        av_log(NULL, AV_LOG_ERROR, "Could not find the proper codec!\n");
        return -1;
    }

    enc_ctx->acodec_ctx = avcodec_alloc_context3(enc_ctx->acodec);
    if (!enc_ctx->acodec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Could not allocated memory for codec context!\n");
        return -1;
    }
    
    // 设置参数
    int OUTPUT_CHANNELS = 2;
    int OUTPUT_BIT_RATE = 196000;
    enc_ctx->acodec_ctx->channels = OUTPUT_CHANNELS;
    enc_ctx->acodec_ctx->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
    enc_ctx->acodec_ctx->sample_rate = sample_rate;
    enc_ctx->acodec_ctx->sample_fmt = enc_ctx->acodec->sample_fmts[0];
    enc_ctx->acodec_ctx->bit_rate = OUTPUT_BIT_RATE;
    enc_ctx->acodec_ctx->time_base = (AVRational){1, sample_rate};
    enc_ctx->acodec_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    enc_ctx->astream->time_base = enc_ctx->acodec_ctx->time_base;

    if (avcodec_open2(enc_ctx->acodec_ctx, enc_ctx->acodec, NULL) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Could not open the codec!\n");
        return -1;
    }
    avcodec_parameters_from_context(enc_ctx->astream->codecpar, enc_ctx->acodec_ctx);
    return 0;
}

static int reload_params(AVFormatContext *avfmt_ctx, AVStream **avstream, AVCodecParameters *avcodec_par) {
    *avstream = avformat_new_stream(avfmt_ctx, NULL);
    avcodec_parameters_copy((*avstream)->codecpar, avcodec_par);
    return 0;
}

static int open_output_file(StreamContext *dec_ctx, StreamContext *enc_ctx, StreamParams *sp) {
    int ret = 0;
    // 加载输入文件/流
    if ((ret = load_output_file(enc_ctx->filename, &enc_ctx->avfmt_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "load output file failed!\n");
        return ret;
    }
    
    // 加载视频编码器
    if (!sp->copy_video) {
        if ((ret = load_video_encoder(dec_ctx, enc_ctx, sp)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "load output video coder failed!\n");
            return ret;
        }
    } else {
        if (reload_params(enc_ctx->avfmt_ctx, &enc_ctx->vstream, dec_ctx->vstream->codecpar) < 0 ) {
            av_log(NULL, AV_LOG_ERROR, "prepare copy failed!\n");
            return -1;
        }
    }
    
    // 加载音频编码器
    if (!sp->copy_audio) {
        if ((ret = load_audio_encoder(enc_ctx, dec_ctx->acodec_ctx->sample_rate, sp)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "load output audio coder failed!\n");
            return ret;
        }
    } else {
        if (reload_params(enc_ctx->avfmt_ctx, &enc_ctx->astream, dec_ctx->astream->codecpar) < 0) {
            av_log(NULL, AV_LOG_ERROR, "prepare copy failed!\n");
            return -1;
        }
    } 
    
    if (enc_ctx->avfmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        enc_ctx->avfmt_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    av_dump_format(enc_ctx->avfmt_ctx, 0, enc_ctx->filename, 1);

    if (!(enc_ctx->avfmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&enc_ctx->avfmt_ctx->pb, enc_ctx->filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'\n", enc_ctx->filename);
            return ret;
        }
    }

    /* set muxer opts */
    AVDictionary* muxer_opts = NULL;
    // av_dict_set(&muxer_opts, sp.muxer_opt_key, sp.muxer_opt_value, 0);

    /* init muxer, write output file header */
    ret = avformat_write_header(enc_ctx->avfmt_ctx, &muxer_opts);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file\n");
        return ret;
    }
    return ret;
}

static int encode_video(StreamContext *dec_ctx, StreamContext *enc_ctx, AVFrame *input_frame) {
    if (input_frame) {
        input_frame->pict_type = AV_PICTURE_TYPE_NONE;
    }

    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
        av_log(NULL, AV_LOG_ERROR, "could not allocate memory for output packet\n");
        return -1;
    }

    int ret = avcodec_send_frame(enc_ctx->vcodec_ctx, input_frame);
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx->vcodec_ctx, output_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while receiving packet from encoder\n");
            return -1;
        }

        output_packet->stream_index = dec_ctx->vindex;
        output_packet->duration = enc_ctx->vstream->time_base.den / enc_ctx->vstream->time_base.num / dec_ctx->vstream->avg_frame_rate.num * dec_ctx->vstream->avg_frame_rate.den;

        av_packet_rescale_ts(output_packet, dec_ctx->vstream->time_base, enc_ctx->vstream->time_base);
        ret = av_interleaved_write_frame(enc_ctx->avfmt_ctx, output_packet);
        if (ret != 0) { 
            av_log(NULL, AV_LOG_ERROR, "Error %d while receiving packet from decoder: %s\n", ret, av_err2str(ret));
            return -1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

static int transcode_video(StreamContext *dec_ctx, StreamContext *enc_ctx, AVPacket *input_packet, AVFrame *input_frame) {
    int ret = avcodec_send_packet(dec_ctx->vcodec_ctx, input_packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while sending packet to decoder\n"); 
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx->vcodec_ctx, input_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while receiving frame from decoder\n");
            return ret;
        }
        if (ret >= 0) {
            if (encode_video(dec_ctx, enc_ctx, input_frame)) {
                return -1;
            }
        }
        av_frame_unref(input_frame);
    }
    return 0;
}

int encode_audio(StreamContext *dec_ctx, StreamContext *enc_ctx, AVFrame *input_frame) {
    AVPacket *output_packet = av_packet_alloc();
    if (!output_packet) {
        av_log(NULL, AV_LOG_ERROR, "could not allocate memory for output packet\n");
        return -1;
    }

    int ret = avcodec_send_frame(enc_ctx->acodec_ctx, input_frame);
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx->acodec_ctx, output_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while receiving packet from encoder: %s\n", av_err2str(ret));
            return -1;
        }

        output_packet->stream_index = dec_ctx->aindex;
        av_packet_rescale_ts(output_packet, dec_ctx->astream->time_base, enc_ctx->astream->time_base);
        ret = av_interleaved_write_frame(enc_ctx->avfmt_ctx, output_packet);
        if (ret != 0) {
            av_log(NULL, AV_LOG_ERROR, "Error %d while receiving packet from decoder: %s\n", ret, av_err2str(ret)); 
            return -1;
        }
    }
    av_packet_unref(output_packet);
    av_packet_free(&output_packet);
    return 0;
}

static int transcode_audio(StreamContext *dec_ctx, StreamContext *enc_ctx, AVPacket *input_packet, AVFrame *input_frame) {
    int ret = avcodec_send_packet(dec_ctx->acodec_ctx, input_packet);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error while sending packet to decoder: %s\n", av_err2str(ret)); 
        return ret;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx->acodec_ctx, input_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while receiving frame from decoder: %s\n", av_err2str(ret));
            return ret;
        }

        if (ret >= 0) {
            if (encode_audio(dec_ctx, enc_ctx, input_frame)) {
                return -1;
            }
        }
        av_frame_unref(input_frame);
    }
    return 0;
}

static int remux(AVPacket *pkt, AVFormatContext *avfc, AVRational dec_tb, AVRational enc_tb) {
    av_packet_rescale_ts(pkt, dec_tb, enc_tb);
    pkt->pos = -1;
    if (av_interleaved_write_frame(avfc, pkt) < 0) {
        av_log(NULL, AV_LOG_ERROR, "error while copying stream packet\n"); 
        return -1; 
    }
    return 0;
}

static int transcode(StreamContext *dec_ctx, StreamContext *enc_ctx, StreamParams *sp) {
    /* init input frame */
    AVFrame *input_frame = av_frame_alloc();
    if (!input_frame) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocated memory for AVFrame\n"); 
        return -1;
    }
    
    /* init input packet */
    AVPacket *input_packet = av_packet_alloc();
    if (!input_packet) {
        av_log(NULL, AV_LOG_ERROR, "failed to allocated memory for AVPacket\n");
        return -1;
    }
    
    while (av_read_frame(dec_ctx->avfmt_ctx, input_packet) >= 0) {
        if (dec_ctx->avfmt_ctx->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            if (!sp->copy_video) {
                if (transcode_video(dec_ctx, enc_ctx, input_packet, input_frame)) {
                    return -1;
                }
                av_packet_unref(input_packet);
            } else {
                if (avcodec_parameters_copy(enc_ctx->vstream->codecpar, dec_ctx->vstream->codecpar) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameters\n");
                }
                if (remux(input_packet, enc_ctx->avfmt_ctx, dec_ctx->astream->time_base, enc_ctx->astream->time_base) < 0) {
                    return -1;
                }
            }
        } else if (dec_ctx->avfmt_ctx->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)  {
            if (!sp->copy_audio) {
                if (transcode_audio(dec_ctx, enc_ctx, input_packet, input_frame)) {
                    return -1;
                }
                av_packet_unref(input_packet);
            } else {
                if (avcodec_parameters_copy(enc_ctx->astream->codecpar, dec_ctx->astream->codecpar) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameters\n");
                }
                if (remux(input_packet, enc_ctx->avfmt_ctx, dec_ctx->astream->time_base, enc_ctx->astream->time_base) < 0) {
                    return -1;
                }
            }
        } else {
            av_log(NULL, AV_LOG_ERROR, "ignoring all non video or audio packets\n");
        }
    }

    // flush encoder
    // if (encode_video(dec_ctx, enc_ctx, NULL)) {
    //     return -1;
    // }

    av_write_trailer(enc_ctx->avfmt_ctx);
    
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

int main(int argc, char *argv[]) {
    // 检查输入输出
    if (argc != 3) {
        av_log(NULL, AV_LOG_ERROR, "Usage: %s <input file> <output file>\n", argv[0]);
        return 0;
    }
    int ret = 0;
    StreamContext *dec_ctx = (StreamContext *)calloc(1, sizeof(StreamContext));
    dec_ctx->filename = argv[1];

    StreamContext *enc_ctx = (StreamContext *)calloc(1, sizeof(StreamContext));
    enc_ctx->filename = argv[2];
             
    // 设置编码参数
    StreamParams sp = {0};
    sp.copy_audio = 1;
    sp.copy_video = 1;
    sp.video_codec = "libx264";
    sp.audio_codec = "aac";

    // 加载输入文件/流
    if ((ret = open_input_file(dec_ctx)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "open input file failed!\n");
        return ret;
    }

    // 打开输出文件/流
    if ((ret = open_output_file(dec_ctx, enc_ctx, &sp)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "open output file failed!\n");
        return ret;
    }

    // 初始化filter
    // if ((ret = init_filters()) < 0) {
    //     return ret;
    // }

    // 进行编码
    if ((ret = transcode(dec_ctx, enc_ctx, &sp)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "transcode failed!\n");
        return ret;
    }

    avformat_close_input(&dec_ctx->avfmt_ctx);

    avformat_free_context(dec_ctx->avfmt_ctx); dec_ctx->avfmt_ctx = NULL;
    avformat_free_context(enc_ctx->avfmt_ctx); enc_ctx->avfmt_ctx = NULL;

    avcodec_free_context(&dec_ctx->vcodec_ctx); dec_ctx->vcodec_ctx = NULL;
    avcodec_free_context(&dec_ctx->acodec_ctx); dec_ctx->acodec_ctx = NULL;

    free(dec_ctx); dec_ctx = NULL;
    free(enc_ctx); enc_ctx = NULL;
    return ret;
}