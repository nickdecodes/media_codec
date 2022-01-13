/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2022, nickdecodes
 * All rights reserved.
 *
 * modification, are permitted provided that the following conditions are met:
 * Redistribution and use in source and binary forms, with or without
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ffavapi.h"

/**
 * 退出程序时清理空间
 * @param ret-退出结果
 * @return void
 */
// static void cleanup(int ret) {
//     int i, j;

//     for (i = 0; i < nb_filtergraphs; i++) {
//         FilterGraph *fg = filtergraphs[i];
//         avfilter_graph_free(&fg->graph);
//         for (j = 0; j < fg->nb_inputs; j++) {
//             InputFilter *ifilter = fg->inputs[j];
//             struct InputStream *ist = ifilter->ist;

//             while (av_fifo_size(ifilter->frame_queue)) {
//                 AVFrame *frame;
//                 av_fifo_generic_read(ifilter->frame_queue, &frame,
//                                      sizeof(frame), NULL);
//                 av_frame_free(&frame);
//             }
//             av_fifo_freep(&ifilter->frame_queue);
//             av_freep(&ifilter->displaymatrix);
//             if (ist->sub2video.sub_queue) {
//                 while (av_fifo_size(ist->sub2video.sub_queue)) {
//                     AVSubtitle sub;
//                     av_fifo_generic_read(ist->sub2video.sub_queue,
//                                          &sub, sizeof(sub), NULL);
//                     avsubtitle_free(&sub);
//                 }
//                 av_fifo_freep(&ist->sub2video.sub_queue);
//             }
//             av_buffer_unref(&ifilter->hw_frames_ctx);
//             av_freep(&ifilter->name);
//             av_freep(&fg->inputs[j]);
//         }
//         av_freep(&fg->inputs);
//         for (j = 0; j < fg->nb_outputs; j++) {
//             OutputFilter *ofilter = fg->outputs[j];

//             avfilter_inout_free(&ofilter->out_tmp);
//             av_freep(&ofilter->name);
//             av_freep(&fg->outputs[j]);
//         }
//         av_freep(&fg->outputs);
//         av_freep(&fg->graph_desc);

//         av_freep(&filtergraphs[i]);
//     }
//     av_freep(&filtergraphs);

//     av_freep(&subtitle_out);

//     /* close files */
//     for (i = 0; i < nb_output_files; i++) {
//         OutputFile *of = output_files[i];
//         AVFormatContext *s;
//         if (!of)
//             continue;
//         s = of->ctx;
//         if (s && s->oformat && !(s->oformat->flags & AVFMT_NOFILE))
//             avio_closep(&s->pb);
//         avformat_free_context(s);
//         av_dict_free(&of->opts);

//         av_freep(&output_files[i]);
//     }
//     for (i = 0; i < nb_output_streams; i++) {
//         OutputStream *ost = output_streams[i];

//         if (!ost)
//             continue;

//         av_bsf_free(&ost->bsf_ctx);

//         av_frame_free(&ost->filtered_frame);
//         av_frame_free(&ost->last_frame);
//         av_packet_free(&ost->pkt);
//         av_dict_free(&ost->encoder_opts);

//         av_freep(&ost->forced_keyframes);
//         av_expr_free(ost->forced_keyframes_pexpr);
//         av_freep(&ost->avfilter);
//         av_freep(&ost->logfile_prefix);

//         av_freep(&ost->audio_channels_map);
//         ost->audio_channels_mapped = 0;

//         av_dict_free(&ost->sws_dict);
//         av_dict_free(&ost->swr_opts);

//         avcodec_free_context(&ost->enc_ctx);
//         avcodec_parameters_free(&ost->ref_par);

//         if (ost->muxing_queue) {
//             while (av_fifo_size(ost->muxing_queue)) {
//                 AVPacket *pkt;
//                 av_fifo_generic_read(ost->muxing_queue, &pkt, sizeof(pkt), NULL);
//                 av_packet_free(&pkt);
//             }
//             av_fifo_freep(&ost->muxing_queue);
//         }

//         av_freep(&output_streams[i]);
//     }
// #if HAVE_THREADS
//     free_input_threads();
// #endif
//     for (i = 0; i < nb_input_files; i++) {
//         avformat_close_input(&input_files[i]->ctx);
//         av_packet_free(&input_files[i]->pkt);
//         av_freep(&input_files[i]);
//     }
//     for (i = 0; i < nb_input_streams; i++) {
//         InputStream *ist = input_streams[i];

//         av_frame_free(&ist->decoded_frame);
//         av_packet_free(&ist->pkt);
//         av_dict_free(&ist->decoder_opts);
//         avsubtitle_free(&ist->prev_sub.subtitle);
//         av_frame_free(&ist->sub2video.frame);
//         av_freep(&ist->filters);
//         av_freep(&ist->hwaccel_device);
//         av_freep(&ist->dts_buffer);

//         avcodec_free_context(&ist->dec_ctx);

//         av_freep(&input_streams[i]);
//     }

//     if (vstats_file) {
//         if (fclose(vstats_file))
//             av_log(NULL, AV_LOG_ERROR,
//                    "Error closing vstats file, loss of information possible: %s\n",
//                    av_err2str(AVERROR(errno)));
//     }
//     av_freep(&vstats_filename);
//     av_freep(&filter_nbthreads);

//     av_freep(&input_streams);
//     av_freep(&input_files);
//     av_freep(&output_streams);
//     av_freep(&output_files);

//     uninit_opts();

//     avformat_network_deinit();

//     if (received_sigterm) {
//         av_log(NULL, AV_LOG_INFO, "Exiting normally, received signal %d.\n",
//                (int) received_sigterm);
//     } else if (ret && atomic_load(&transcode_init_done)) {
//         av_log(NULL, AV_LOG_INFO, "Conversion failed!\n");
//     }
//     term_exit();
//     ffmpeg_exited = 1;
// }

/**
 * 命令行调用API主入口
 * @param argc-参数个数
 * @param argv-参数数组
 * @return int
 */
int main(int argc, char *argv[]) {
    int i, ret;
    /* parse options and open all input/output files */
    ret = parse_options(argc, argv);
    return 0;
}