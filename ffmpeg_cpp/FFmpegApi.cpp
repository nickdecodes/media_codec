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

#include "FFmpegApi.h"

/**
 * 命令行调用API主入口
 * @param cmd_str-命令行参数
 * @return int
 */
int FFmpegApi::cmdApi(std::string cmd_str) {
    int ret = 0;
    // LOG_SET_TARGET(log::LogLevel::FILES, "./tmp.log");
    // LOG_SET_LEVEL(log::LogLevel::DEBUG | log::LogLevel::INFO);

    // // 分析参数
    // LOG_DEBUG << "分析参数 " << cmd_str;

    // // 打开输入文件
    // LOG_DEBUG << "打开输入文件";

    // // 打开输出文件
    // LOG_DEBUG << "打开输出文件";

    // // 初始化过滤器
    // LOG_DEBUG << "初始化过滤器";

    // // 进行编码
    // LOG_DEBUG << "进行编码";

    return ret;
}