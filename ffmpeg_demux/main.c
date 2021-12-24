#include <stdlib.h>
#include <libavformat/avformat.h>

int main (int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s input output.h264 output.aac\n", argv[0]);
        exit(1);
    }

    int ret = 0;
    const char *in_filename = argv[1];
    const char *out_v_filename = argv[2];
    const char *out_a_filename = argv[3];
    FILE *video_dst_file = fopen(out_v_filename, "wb");
    FILE *audio_dst_file = fopen(out_a_filename, "wb");

    AVPacket *pkt = av_packet_alloc();
    if (!pkt) {
        printf("Could not allocate AVPacket\n");
        goto end;
    }

    AVFormatContext *ifmt_ctx = NULL;
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", in_filename);
        goto end;
    } 

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        goto end;
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);
    int video_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    int audio_idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (video_idx < 0 || audio_idx < 0) {
        printf("find stream failed: %d %d\n", video_idx, audio_idx);
        goto end;
    }

    // AVPacket pkt;
    // av_new_packet(&pkt, sizeof(AVPacket));


    while (av_read_frame(ifmt_ctx, pkt) >= 0) {
        if (pkt->stream_index == video_idx) {
            ret = fwrite(pkt->data, 1, pkt->size, video_dst_file);
            printf("vp %llx %3"PRId64" %3"PRId64" (size=%5d)\n", pkt->pos, pkt->pts, pkt->dts, ret);
        } else if (pkt->stream_index == audio_idx) {
            ret = fwrite(pkt->data, 1, pkt->size, audio_dst_file);
            // printf("ap %x %3"PRId64" %3"PRId64" (size=%5d)\n", pkt.pos, pkt.pts, pkt.dts, ret);
        }
        av_packet_unref(pkt);
    }

    printf("Demuxing succeeded.\n");

end:
    avformat_close_input(&ifmt_ctx);
    fclose(video_dst_file);
    fclose(audio_dst_file);

    return 0;
}

