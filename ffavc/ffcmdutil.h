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

#ifndef _FFCMDUTIL_H
#define _FFCMDUTIL_H

#include <stdio.h>
#include "libavcodec/avcodec.h"
#include "libavfilter/avfilter.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

typedef struct OptionDef {
    const char *name;
    int flags;
#define HAS_ARG         0x0001
#define OPT_BOOL        0x0002
#define OPT_EXPERT      0x0004
#define OPT_STRING      0x0008
#define OPT_VIDEO       0x0010
#define OPT_AUDIO       0x0020
#define OPT_INT         0x0080
#define OPT_FLOAT       0x0100
#define OPT_SUBTITLE    0x0200
#define OPT_INT64       0x0400
#define OPT_EXIT        0x0800
#define OPT_DATA        0x1000
#define OPT_PERFILE     0x2000     /* the option is per-file (currently ffmpeg-only). implied by OPT_OFFSET or OPT_SPEC */
#define OPT_OFFSET      0x4000     /* option is specified as an offset in a passed optctx */
#define OPT_SPEC        0x8000     /* option is to be stored in an array of SpecifierOpt.Implies OPT_OFFSET. Next element after the offset is an int containing element count in the array. */
#define OPT_TIME        0x10000
#define OPT_DOUBLE      0x20000
#define OPT_INPUT       0x40000
#define OPT_OUTPUT      0x80000
    union {
        void *dst_ptr;
        int (*func_arg)(void *, const char *, const char *);
        size_t off;
    } u;
    const char *help;
    const char *argname;
} OptionDef;

/**
 * An option extracted from the commandline.
 * Cannot use AVDictionary because of options like -map which can be
 * used multiple times.
 */
typedef struct Option {
    const OptionDef  *opt;
    const char       *key;
    const char       *val;
} Option;

typedef struct OptionGroupDef {
    /* group name */
    const char *name;
    /**
     * Option to be used as group separator. Can be NULL for groups which
     * are terminated by a non-option argument (e.g. ffmpeg output files)
     */
    const char *sep;
    /**
     * Option flags that must be set on each option that is
     * applied to this group
     */
    int flags;
} OptionGroupDef;

typedef struct OptionGroup {
    const OptionGroupDef *group_def;
    const char *arg;

    Option *opts;
    int  nb_opts;

    AVDictionary *codec_opts;
    AVDictionary *format_opts;
    AVDictionary *sws_dict;
    AVDictionary *swr_opts;
} OptionGroup;

typedef struct OptionGroupList {
    const OptionGroupDef *group_def;

    OptionGroup *groups;
    int       nb_groups;
} OptionGroupList;

typedef struct OptionParseContext {
    OptionGroup global_opts;

    OptionGroupList *groups;
    int nb_groups;

    /* parsing state */
    OptionGroup cur_group;
} OptionParseContext;

/**
 * 通过回调函数，注册一个退出处理程序
 * @param cb-函数指针
 * @return void
 */
void register_exit(void (*cb)(int ret));

/**
 * 退出程序
 * @param ret-退出标识
 * @return void
 */
void exit_program(int ret);

#endif