
/**
 * @file
 * API example for demuxing, decoding, filtering, encoding and muxing
 * @example transcoding.c
 */

#include <stdbool.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/time.h>
#include "av_filter.h"
#include "av_codec.h"

typedef struct {
    AVFormatContext* i_fmt_ctx;
    AVCodecContext* i_codec_ctx;
    AVFormatContext* o_fmt_ctx;
    AVCodecContext* o_codec_ctx;
    filter_ctx_t* flt_ctx;
    AVAudioFifo* aud_fifo;
    AVStream* i_stream;
    AVStream* o_stream;
    int stream_idx;
}   stream_ctx_t;

// 为每个音频流/视频流使用空滤镜，滤镜图中将buffer滤镜和buffersink滤镜直接相连
// 目的是：通过视频buffersink滤镜将视频流输出像素格式转换为编码器采用的像素格式
//         通过音频abuffersink滤镜将音频流输出声道布局转换为编码器采用的声道布局
//         为下一步的编码操作作好准备
static int init_filters(const inout_ctx_t *ictx, const inout_ctx_t *octx, filter_ctx_t **pp_fctxs) {
    int nb_streams = ictx->fmt_ctx->nb_streams;
    filter_ctx_t *p_fctxs = av_malloc_array(nb_streams, sizeof(*p_fctxs));
    if (!p_fctxs) {
        return AVERROR(ENOMEM);
    }

    filter_ivfmt_t ivfmt;
    filter_ovfmt_t ovfmt;
    filter_iafmt_t iafmt;
    filter_oafmt_t oafmt;
    enum AVMediaType codec_type;
    AVCodecContext *p_codec_ctx;
    for (int i = 0; i < nb_streams; i++) {
        codec_type = ictx->fmt_ctx->streams[i]->codecpar->codec_type;
        p_codec_ctx = octx->codec_ctx[i];
        
        int ret = 0;
        if (codec_type == AVMEDIA_TYPE_VIDEO) {
            get_filter_ivfmt(ictx, i, &ivfmt);
            #if 0
            get_filter_ovfmt(octx, i, &ovfmt);
            #else
            enum AVPixelFormat pix_fmts[] = { octx->codec_ctx[i]->pix_fmt, AV_PIX_FMT_NONE};
            ovfmt.pix_fmts = pix_fmts;
            #endif
            ret = init_video_filters("null", &ivfmt, &ovfmt, &p_fctxs[i]);
        } else if (codec_type == AVMEDIA_TYPE_AUDIO) {
            get_filter_iafmt(ictx, i, &iafmt);
            #if 0
            get_filter_oafmt(octx, i, &oafmt);
            #else
            enum AVSampleFormat sample_fmts[] = { octx->codec_ctx[i]->sample_fmt, -1 };
            int sample_rates[] = { octx->codec_ctx[i]->sample_rate, -1 };
            uint64_t channel_layouts[] = { octx->codec_ctx[i]->channel_layout, -1 };
            oafmt.sample_fmts = sample_fmts;
            oafmt.sample_rates = sample_rates;
            oafmt.channel_layouts = channel_layouts;
            #endif
            ret = init_audio_filters("anull", &iafmt, &oafmt, &p_fctxs[i]);
        } else {
            p_fctxs[i].bufsink_ctx = NULL;
            p_fctxs[i].bufsrc_ctx = NULL;
            p_fctxs[i].filter_graph = NULL;
        }

        if (ret < 0) {
            return ret;
        }
    }

    *pp_fctxs = p_fctxs;

    return 0;
}

/**
 * Initialize one input frame for writing to the output file.
 * The frame will be exactly frame_size samples large.
 * @param[out] frame                Frame to be initialized
 * @param      output_codec_context Codec context of the output file
 * @param      frame_size           Size of the frame
 * @return Error code (0 if successful)
 */
static int init_audio_output_frame(AVFrame **frame,
                                   AVCodecContext *occtx,
                                   int frame_size) {
    int error;

    /* Create a new frame to store the audio samples. */
    if (!(*frame = av_frame_alloc())) {
        fprintf(stderr, "Could not allocate output frame\n");
        return AVERROR_EXIT;
    }

    /* Set the frame's parameters, especially its size and format.
     * av_frame_get_buffer needs this to allocate memory for the
     * audio samples of the frame.
     * Default channel layouts based on the number of channels
     * are assumed for simplicity. */
    (*frame)->nb_samples     = frame_size;
    (*frame)->channel_layout = occtx->channel_layout;
    (*frame)->format         = occtx->sample_fmt;
    (*frame)->sample_rate    = occtx->sample_rate;

    /* Allocate the samples of the created frame. This call will make
     * sure that the audio frame can hold as many samples as specified. */
    // 为AVFrame分配缓冲区，此函数会填充AVFrame.data和AVFrame.buf，若有需要，也会填充
    // AVFrame.extended_data和AVFrame.extended_buf，对于planar格式音频，会为每个plane
    // 分配一个缓冲区
    if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
        fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
                av_err2str(error));
        av_frame_free(frame);
        return error;
    }

    return 0;
}

// FIFO中可读数据小于编码器帧尺寸，则继续往FIFO中写数据
static int write_frame_to_audio_fifo(AVAudioFifo *fifo,
                                     uint8_t **new_data,
                                     int new_size) {
    int ret = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + new_size);
    if (ret < 0) {
        fprintf(stderr, "Could not reallocate FIFO\n");
        return ret;
    }
    
    /* Store the new samples in the FIFO buffer. */
    ret = av_audio_fifo_write(fifo, (void **)new_data, new_size);
    if (ret < new_size) {
        fprintf(stderr, "Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }

    return 0;
}

static int read_frame_from_audio_fifo(AVAudioFifo *fifo,
                                      AVCodecContext *occtx,
                                      AVFrame **frame) {
    AVFrame *output_frame;
    // 如果FIFO中可读数据多于编码器帧大小，则只读取编码器帧大小的数据出来
    // 否则将FIFO中数据读完。frame_size是帧中单个声道的采样点数
    const int frame_size = FFMIN(av_audio_fifo_size(fifo), occtx->frame_size);

    /* Initialize temporary storage for one output frame. */
    // 分配AVFrame及AVFrame数据缓冲区
    int ret = init_audio_output_frame(&output_frame, occtx, frame_size);
    if (ret < 0) {
        return AVERROR_EXIT;
    }

    // 从FIFO从读取数据填充到output_frame->data中
    ret = av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size);
    if (ret < frame_size) {
        fprintf(stderr, "Could not read data from FIFO\n");
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    *frame = output_frame;

    return ret;
}

static int transcode_audio(const stream_ctx_t *sctx, AVPacket *ipacket) {
    AVFrame *frame_dec = av_frame_alloc();
    AVFrame *frame_flt = av_frame_alloc();
    if (!frame_dec || !frame_flt) {
        perror("Could not allocate frame");
        exit(1);
    }

    AVPacket opacket;
    av_new_packet(&opacket, 0);
    /* Set the packet data and size so that it is recognized as being empty. */

    int enc_frame_size = sctx->o_codec_ctx->frame_size;
    AVAudioFifo* p_fifo = sctx->aud_fifo;
    int ret = 0;
    bool dec_finished = false;
    bool enc_finished = false;
    bool flt_finished = false;
    bool new_packet = true;
    
    while (1) {  // 处理一个packet，一个音频packet可能包含多个音频frame，循环每次处理一个frame
        dec_finished = false;
        enc_finished = false;

        // 1. 时间基转换，解码
        av_packet_rescale_ts(ipacket, sctx->i_stream->time_base, sctx->o_codec_ctx->time_base);
        ret = av_decode_frame(sctx->i_codec_ctx, ipacket, &new_packet, frame_dec);
        if (ret == AVERROR(EAGAIN)) {    // 需要读取新的packet喂给解码器
            //av_log(NULL, AV_LOG_INFO, "decode aframe need more packet\n");
            goto end;
        } else if (ret == AVERROR_EOF) {   // 解码器已冲洗
            av_log(NULL, AV_LOG_INFO, "decode aframe EOF\n");
            if (frame_dec != NULL) {
                av_frame_free(&frame_dec);
            }
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "decode aframe error %d\n", ret);
            goto end;
        }

        // 2. 滤镜处理
        ret = filtering_frame(sctx->flt_ctx, frame_dec, frame_flt);
        if (ret == AVERROR_EOF) {        // 滤镜已冲洗
            flt_finished = true;
            av_log(NULL, AV_LOG_INFO, "filtering aframe EOF\n");
            if (frame_flt != NULL) {
                av_frame_free(&frame_flt);
            }
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "filtering aframe error %d\n", ret);
            goto end;
        }

flush_encoder:
        // 3. 编码
        ret = av_encode_frame(sctx->o_codec_ctx, frame_flt, &opacket);
        if (ret == AVERROR(EAGAIN)) {    // 需要获取新的frame喂给编码器
            //av_log(NULL, AV_LOG_INFO, "encode aframe need more packet\n");
            if (frame_flt != NULL) {
                av_frame_free(&frame_flt);
            }
            continue;
        } else if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "encode aframe EOF\n");
            enc_finished = true;
            goto end;
        }

        // 3. 更新编码帧中流序号，并进行时间基转换
        //    AVPacket.pts和AVPacket.dts的单位是AVStream.time_base，不同的封装格式其AVStream.time_base不同
        //    所以输出文件中，每个packet需要根据输出封装格式重新计算pts和dts
        opacket.stream_index = sctx->stream_idx;
        av_packet_rescale_ts(&opacket, sctx->o_codec_ctx->time_base, sctx->o_stream->time_base);
        av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");

        // 4. 将编码后的packet写入输出媒体文件
        ret = av_interleaved_write_frame(sctx->o_fmt_ctx, &opacket);
        if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "write aframe error %d\n", ret);
            goto end;
        }

        if (flt_finished) {
            goto flush_encoder;
        }

        if (enc_finished) {
            break;
        }
    }

    ret = 1;
    
end:
    av_packet_unref(&opacket);
    if (frame_flt != NULL) {
        av_frame_free(&frame_flt);
    }
    if (frame_dec != NULL) {
        av_frame_free(&frame_dec);
    }
    
    return ret;
}

static int transcode_audio_with_afifo(const stream_ctx_t *sctx, AVPacket *ipacket) {
    AVFrame *frame_dec = av_frame_alloc();
    AVFrame *frame_flt = av_frame_alloc();
    if (!frame_dec || !frame_flt) {
        perror("Could not allocate frame");
        exit(1);
    }
    AVFrame *frame_enc = NULL;

    AVPacket opacket;
    av_new_packet(&opacket, 0);
    /* Set the packet data and size so that it is recognized as being empty. */

    int enc_frame_size = sctx->o_codec_ctx->frame_size;
    AVAudioFifo* p_fifo = sctx->aud_fifo;
    int ret = 0;
    bool dec_finished = false;
    bool enc_finished = false;
    bool flt_finished = false;
    bool new_packet = true;
    static int s_pts = 0;
    
    while (1) {   // 处理一个packet，一个音频packet可能包含多个音频frame，循环每次处理一个frame
        dec_finished = false;
        enc_finished = false;

        // 1. 时间基转换，解码
        av_packet_rescale_ts(ipacket, sctx->i_stream->time_base, sctx->o_codec_ctx->time_base);
        ret = av_decode_frame(sctx->i_codec_ctx, ipacket, &new_packet, frame_dec);
        if (ret == AVERROR(EAGAIN)) {    // 需要读取新的packet喂给解码器
            //av_log(NULL, AV_LOG_INFO, "decode aframe need more packet\n");
            goto end;
        } else if (ret == AVERROR_EOF) {   // 解码器已冲洗
            dec_finished = true;
            av_log(NULL, AV_LOG_INFO, "decode aframe EOF\n");
            frame_dec = NULL;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "decode aframe error %d\n", ret);
            goto end;
        }

        // 2. 滤镜处理
        ret = filtering_frame(sctx->flt_ctx, frame_dec, frame_flt);
        if (ret == AVERROR_EOF) {        // 滤镜已冲洗
            flt_finished = true;
            av_log(NULL, AV_LOG_INFO, "filtering aframe EOF\n");
            frame_flt = NULL;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "filtering aframe error %d\n", ret);
            goto end;
        }

        // 3. 使用音频fifo，从而保证每次送入编码器的音频帧尺寸满足编码器要求
        // 3.1 将音频帧写入fifo，音频帧尺寸是解码格式中音频帧尺寸
        if (!dec_finished) {
            uint8_t** new_data = frame_flt->extended_data;  // 本帧中多个声道音频数据
            int new_size = frame_flt->nb_samples;           // 本帧中单个声道的采样点数
            
            // FIFO中可读数据小于编码器帧尺寸，则继续往FIFO中写数据
            ret = write_frame_to_audio_fifo(p_fifo, new_data, new_size);
            if (ret < 0) {
                av_log(NULL, AV_LOG_INFO, "write aframe to fifo error\n");
                goto end;
            }
        }

        // 3.2 从fifo中取出音频帧，音频帧尺寸是编码格式中音频帧尺寸
        // FIFO中可读数据大于编码器帧尺寸，则从FIFO中读走数据进行处理
        while ((av_audio_fifo_size(p_fifo) >= enc_frame_size) || dec_finished) {
            bool flushing = dec_finished && (av_audio_fifo_size(p_fifo) == 0);  // 已取空，刷洗编码器
            
            if (frame_enc != NULL) {
                av_frame_free(&frame_enc);
            }

            if (!flushing) {
                // 从FIFO中读取数据，编码，写入输出文件
                ret = read_frame_from_audio_fifo(p_fifo, sctx->o_codec_ctx, &frame_enc);
                if (ret < 0) {
                    av_log(NULL, AV_LOG_INFO, "read aframe from fifo error\n");
                    goto end;
                }

                // 4. fifo中读取的音频帧没有时间戳信息，重新生成pts
                frame_enc->pts = s_pts;
                s_pts += ret;
            }

flush_encoder:
            // 5. 编码
            ret = av_encode_frame(sctx->o_codec_ctx, frame_enc, &opacket);
            if (ret == AVERROR(EAGAIN)) {    // 需要获取新的frame喂给编码器
                //av_log(NULL, AV_LOG_INFO, "encode aframe need more packet\n");
                if (frame_enc != NULL) {
                    av_frame_free(&frame_enc);
                }
                continue;
            } else if (ret == AVERROR_EOF) {
                av_log(NULL, AV_LOG_INFO, "encode aframe EOF\n");
                enc_finished = true;
                goto end;
            }

            // 5.1 更新编码帧中流序号，并进行时间基转换
            //     AVPacket.pts和AVPacket.dts的单位是AVStream.time_base，不同的封装格式其AVStream.time_base不同
            //     所以输出文件中，每个packet需要根据输出封装格式重新计算pts和dts
            opacket.stream_index = sctx->stream_idx;
            av_packet_rescale_ts(&opacket, sctx->o_codec_ctx->time_base, sctx->o_stream->time_base);
            
            av_log(NULL, AV_LOG_DEBUG, "Muxing frame\n");

            // 6. 将编码后的packet写入输出媒体文件
            ret = av_interleaved_write_frame(sctx->o_fmt_ctx, &opacket);
            if (ret < 0) {
                av_log(NULL, AV_LOG_INFO, "write aframe error %d\n", ret);
                goto end;
            }

            if (flushing) {
                goto flush_encoder;
            }
        }

        if (enc_finished) {
            break;
        }
    }

    ret = 1;
    
end:
    av_packet_unref(&opacket);
    if (frame_enc != NULL) {
        av_frame_free(&frame_enc);
    }
    if (frame_flt != NULL) {
        av_frame_free(&frame_flt);
    }
    if (frame_dec != NULL) {
        av_frame_free(&frame_dec);
    }
    
    return ret;
}

static int transcode_video(const stream_ctx_t *sctx, AVPacket *ipacket) {
    AVFrame *frame_dec = av_frame_alloc();
    AVFrame *frame_flt = av_frame_alloc();
    if (!frame_dec || !frame_flt) {
        perror("Could not allocate frame");
        exit(1);
    }

    AVPacket opacket;
    av_new_packet(&opacket, 0);
    /* Set the packet data and size so that it is recognized as being empty. */

    bool dec_finished = false;
    bool enc_finished = false;
    bool flt_finished = false;
    bool unused_flag;

    bool new_packet = true;
    int ret = 0;

    // 一个视频packet只包含一个视频frame，但冲洗解码器时一个flush packet会取出
    // 多个frame出来，每次循环取处理一个frame
    while (1) {
        // 1. 时间基转换，解码
        if (ipacket->data != NULL) { // not a flush packet
            av_packet_rescale_ts(ipacket, sctx->i_stream->time_base, sctx->o_codec_ctx->time_base);
        }
        ret = av_decode_frame(sctx->i_codec_ctx, ipacket, &new_packet, frame_dec);
        if (ret == AVERROR(EAGAIN)) {    // 需要读取新的packet喂给解码器
            //av_log(NULL, AV_LOG_INFO, "decode vframe need more packet\n");
            goto end;
        } else if (ret == AVERROR_EOF) {   // 解码器已冲洗
            av_log(NULL, AV_LOG_INFO, "decode vframe EOF\n");
            dec_finished = true;
            av_frame_free(&frame_dec);  // flush filter
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "decode vframe error %d\n", ret);
            goto end;
        }

        // 2. 滤镜处理
        ret = filtering_frame(sctx->flt_ctx, frame_dec, frame_flt);
        if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "filtering vframe EOF\n");
            flt_finished = true;
            av_frame_free(&frame_flt);  // flush encoder
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "filtering vframe error %d\n", ret);
            goto end;
        }

flush_encoder:
        // 3. 编码
        if (frame_flt != NULL) {
            /*
            将原始视频帧frame送入视频编码器后生成编码帧packet，那么
            1. 手工设置每一帧frame的帧类型为I/B/P，则编码后的packet的帧类型和frame中的一样。编码器是否设置gop_size和max_b_frames两个参数无影响。  
            2. 将每一帧frame的帧类型设置为NONE，如果未设置编码器的“gop_size”(默认值-1)和“max_b_frames”(默认值0)两个参数，则编码器自动选择合适参数来进行编码，生成帧类型。
            3. 将每一帧frame的帧类型设置为NONE，如果设置了编码器的“gop_size”和“max_b_frames”两个参数，则编码器按照这两个参数来进行编码，生成帧类型。
            */
            // 3.1 设置帧类型
            frame_flt->pict_type = AV_PICTURE_TYPE_NONE;
        }
        // 3.2 编码
        ret = av_encode_frame(sctx->o_codec_ctx, frame_flt, &opacket);
        if (ret == AVERROR(EAGAIN)) {    // 需要读取新的packet喂给编码器
            //av_log(NULL, AV_LOG_INFO, "encode vframe need more packet\n");
            goto end;
        } else if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "encode vframe EOF\n");
            enc_finished = true;
            goto end;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "encode vframe error %d\n", ret);
            goto end;
        }

        // 3.3 更新编码帧中流序号，并进行时间基转换
        //     AVPacket.pts和AVPacket.dts的单位是AVStream.time_base，不同的封装格式其AVStream.time_base不同
        //     所以输出文件中，每个packet需要根据输出封装格式重新计算pts和dts
        opacket.stream_index = sctx->stream_idx;
        av_packet_rescale_ts(&opacket, sctx->o_codec_ctx->time_base, sctx->o_stream->time_base);

        // 4. 将编码后的packet写入输出媒体文件
        ret = av_interleaved_write_frame(sctx->o_fmt_ctx, &opacket);
        av_packet_unref(&opacket);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "write vframe error %d\n", ret);
            goto end;
        }

        if (flt_finished) {
            goto flush_encoder;
        }
    }
    
end:
    av_packet_unref(&opacket);
    av_frame_free(&frame_flt);
    av_frame_free(&frame_dec);

    return ret;
}

static int flush_audio(const stream_ctx_t *sctx) {
    AVPacket flush_packet;
    av_new_packet(&flush_packet, 0);
    flush_packet.data = NULL;
    flush_packet.size = 0;

    int ret;

    av_log(NULL, AV_LOG_INFO, "flushing audio\n");
    
    while (1) {
        ret = transcode_audio(sctx, &flush_packet);
        if (ret == AVERROR(EAGAIN)) {
            av_usleep(10000);
            av_log(NULL, AV_LOG_INFO, "flushing audio one more\n");
            continue;
        } else if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "flushing audio finished\n");
            return 0;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "flushing audio error %d\n", ret);
            return ret;
        } else  {
            av_usleep(10000);
            av_log(NULL, AV_LOG_INFO, "flushing audio... %d\n", ret);
        }
    }

    return 0;
}

static int flush_video(const stream_ctx_t *sctx)
{
    AVPacket flush_packet;
    av_new_packet(&flush_packet, 0);

    int ret;
    
    av_log(NULL, AV_LOG_INFO, "flushing video\n");
    
    while (1) {
        ret = transcode_video(sctx, &flush_packet);
        if (ret == AVERROR(EAGAIN)) {
            av_usleep(10000);
            av_log(NULL, AV_LOG_INFO, "flushing video one more\n");
            continue;
        } else if (ret == AVERROR_EOF) {
            av_log(NULL, AV_LOG_INFO, "flushing video finished\n");
            return 0;
        } else if (ret < 0) {
            av_log(NULL, AV_LOG_INFO, "flushing video error %d\n", ret);
            return ret;
        } else {
            av_usleep(10000);
            av_log(NULL, AV_LOG_INFO, "flushing video... %d\n", ret);
        }
    }

    return 0;
}

// ./transcode -i input.flv -c:v mpeg2video -c:a mp2 output.ts
int main(int argc, char **argv) {
    int ret;
    AVPacket ipacket = { .data = NULL, .size = 0 };

    enum AVMediaType type;
    unsigned int stream_index;
    unsigned int i;
    int got_frame;

    if (argc != 8 || strcmp(argv[1], "-i") != 0 || strcmp(argv[3], "-c:v") != 0 || strcmp(argv[5], "-c:a") != 0) {
        av_log(NULL, AV_LOG_ERROR, "Usage such as: %s -i input.flv -c:v libx264 -c:a aac output.ts\n", argv[0]);
        return 1;
    }

    printf("ERROR CODE: \n"
           "AVERROR(EAGAIN) %d\nAVERROR_EOF %d\nAVERROR(EINVAL) %d\nAVERROR(ENOMEM) %d\n", 
           AVERROR(EAGAIN), AVERROR_EOF, AVERROR(EINVAL), AVERROR(ENOMEM));

    // 1. 初始化：打开输入，打开输出，初始化滤镜
    inout_ctx_t ictx;
    char *in_fname = argv[2];
    ret = open_input_file(argv[2], &ictx);
    if (ret < 0) {
        goto end;
    }
    inout_ctx_t octx;
    AVAudioFifo** oafifo = NULL;    // AVAudioFifo* oafifo[]
    char *out_fname = argv[7];
    char *v_enc_name = argv[4];
    char *a_enc_name = argv[6];
    ret = open_output_file(out_fname, &ictx, v_enc_name, a_enc_name, &octx, &oafifo);
    if (ret < 0) {
        goto end;
    }
    filter_ctx_t *fctxs;
    ret = init_filters(&ictx, &octx, &fctxs);
    if (ret < 0) {
        return ret;
    }

    AVFrame *frame_dec = av_frame_alloc();
    AVFrame *frame_flt = av_frame_alloc();
    if (!frame_dec || !frame_flt) {
        perror("Could not allocate frame");
        exit(1);
    }
    AVFrame *frame_enc = NULL;

    bool read_finished = false;
    stream_ctx_t stream;
    enum AVMediaType codec_type;
    
    while (1) {
        if (ipacket.data == NULL) {
            av_packet_unref(&ipacket);
        }
        // 2. 从输入文件读取packet
        ret = av_read_frame(ictx.fmt_ctx, &ipacket);
        if (ret < 0) {
            if ((ret == AVERROR_EOF) || avio_feof(ictx.fmt_ctx->pb)) {
                // 输入文件已读完，发送NULL packet以冲洗(flush)解码器，否则解码器中缓存的帧取不出来
                av_log(NULL, AV_LOG_INFO, "av_read_frame() end of file\n");
                read_finished = true;
                break;
            } else {
                goto end;
            }
        }

        int stream_index = ipacket.stream_index;
        codec_type = ictx.fmt_ctx->streams[stream_index]->codecpar->codec_type;
        av_log(NULL, AV_LOG_DEBUG, "Demuxer gave frame of stream_index %u\n", stream_index);

        if (codec_type == AVMEDIA_TYPE_VIDEO || codec_type == AVMEDIA_TYPE_AUDIO) {
            stream.i_fmt_ctx = ictx.fmt_ctx;
            stream.i_codec_ctx = ictx.codec_ctx[stream_index];
            stream.i_stream = ictx.fmt_ctx->streams[stream_index];
            stream.stream_idx = stream_index;
            stream.flt_ctx = &fctxs[stream_index];
            stream.o_fmt_ctx = octx.fmt_ctx;
            stream.o_codec_ctx = octx.codec_ctx[stream_index];
            stream.o_stream = octx.fmt_ctx->streams[stream_index];
            if (codec_type == AVMEDIA_TYPE_VIDEO) {
                ret = transcode_video(&stream, &ipacket);
            } else {
                // AVCodecContext.frame_size表示音频帧中每个声道包含的采样点数。当AVCodecContext.frame_size
                // 标志有效时，音频帧尺寸是可变的，此值可能为0；否则，解码器的AVCodecContext.frame_size等于解码帧中的
                // AVFrame.nb_samples
                // 如果编码器不支持可变尺寸音频帧(第一个判断条件生效)，而原始音频帧的尺寸又和编码器帧尺寸不一样(第二个判
                // 断条件生效)，则需要引入音频帧FIFO，以保证每次从FIFO中取出的音频帧尺寸和编码器帧尺寸一样。音频FIFO输出
                // 的音频帧不含时间戳信息，因此需要重新生成时间戳
                if (((stream.o_codec_ctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE) == 0) &&
                    (stream.i_codec_ctx->frame_size != stream.o_codec_ctx->frame_size)) {
                    stream.aud_fifo = oafifo[stream_index];
                    ret = transcode_audio_with_afifo(&stream, &ipacket);
                } else {
                    ret = transcode_audio(&stream, &ipacket);
                }
            }
            if (ret == AVERROR(EAGAIN)) {
                //av_log(NULL, AV_LOG_INFO, "need read more packet\n");
                continue;
            } else if (ret < 0) {
                goto end;
            }
        } else {
            // AVPacket.pts和AVPacket.dts的单位是AVStream.time_base，不同的封装格式其AVStream.time_base不同
            // 所以输出文件中，每个packet需要根据输出封装格式重新计算pts和dts
            av_packet_rescale_ts(&ipacket,
                                 ictx.fmt_ctx->streams[stream_index]->time_base,
                                 octx.fmt_ctx->streams[stream_index]->time_base);

            ret = av_interleaved_write_frame(octx.fmt_ctx, &ipacket);
            if (ret < 0) {
                goto end;
            }
        }
    }

    /* flush filters and encoders */
    for (i = 0; i < ictx.fmt_ctx->nb_streams; i++) {
        enum AVMediaType codec_type = ictx.fmt_ctx->streams[i]->codecpar->codec_type;
        if (codec_type == AVMEDIA_TYPE_VIDEO || codec_type == AVMEDIA_TYPE_AUDIO) {
            stream.i_fmt_ctx = ictx.fmt_ctx;
            stream.i_codec_ctx = ictx.codec_ctx[i];
            stream.i_stream = ictx.fmt_ctx->streams[i];
            stream.stream_idx = i;
            stream.flt_ctx = &fctxs[i];
            stream.o_fmt_ctx = octx.fmt_ctx;
            stream.o_codec_ctx = octx.codec_ctx[i];
            stream.o_stream = octx.fmt_ctx->streams[i];
            if (codec_type == AVMEDIA_TYPE_VIDEO) {
                flush_video(&stream);
            } else {
                // 音频帧是一帧帧进行解码编码，解码器编码器中没有缓存帧，所以av_read_frame()处理到
                // 文件尾时，解码器编码器中也已全部处理完，没有剩余的帧，此处flush_audio()直接返回
                // AVERROR_EOF，其实flush_audio()这一句没必要调用。
                flush_audio(&stream); 
            }
        } else {
            continue;
        }
    }
    ret = av_write_trailer(octx.fmt_ctx);

end:
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));
    }

    av_packet_unref(&ipacket);
    
    // av_frame_free(&frame);
    for (i = 0; i < ictx.fmt_ctx->nb_streams; i++) {
        avcodec_free_context(&ictx.codec_ctx[i]);
        avcodec_free_context(&octx.codec_ctx[i]);
        deinit_filters(&fctxs[i]);
    }

    av_free(ictx.codec_ctx);
    av_free(octx.codec_ctx);
    av_free(fctxs);

    avformat_close_input(&ictx.fmt_ctx);
    if (octx.fmt_ctx && !(octx.fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&octx.fmt_ctx->pb);
    }
    avformat_free_context(octx.fmt_ctx);

    return ret;
}
