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

#ifndef _LOGGER_H
#define _LOGGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <ctime>
#include <map>

// #define LOG_TRACE(level) log::logger(__FILE__, __func__, __LINE__, level)
// #define LOG_INFO LOG_TRACE(log::LogLevel::INFO)
// #define LOG_DEBUG LOG_TRACE(log::LogLevel::DEBUG)
// #define LOG_WARNING LOG_TRACE(log::LogLevel::WARNING)
// #define LOG_ERROR LOG_TRACE(log::LogLevel::ERROR)
// #define LOG_SET_LEVEL(level) log::Logger::set_level(level)
// #define LOG_SET_TARGET(target, path) log::Logger::set_target(target, path)

namespace log {

class LogLevel {
public:
    enum Levels { INFO = 1, DEBUG = 2, WARNING = 4, ERROR = 8 }; // 日志水平
    enum Targets { FILES = 1, TERMINAL = 2}; // 输出位置
};

std::map<int, std::string> LogLevels {
    { LogLevel::INFO, "info" },     
    { LogLevel::DEBUG, "debug" },
    { LogLevel::WARNING, "warning" },      
    { LogLevel::ERROR, "error" },
};
     
std::string log_time() {
    char tmp[64];
    time_t ltime;
    time(&ltime);
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&ltime));
    return tmp;
}

class Logger {
    class LoggerStream : public std::ostringstream {
    public:
        LoggerStream(
            const char *file, 
            const char *func, 
            int line, 
            Logger &obj, 
            int level
        ) : 
        _obj(obj), 
        _flag(1) {
            std::ostringstream &now = *this;
            now << "[" << file << "] ";
            now << "[" << func << "] ";
            now << "[" << log_time() << "] ";
            now << "[" << LogLevels[level] << "] ";
            now << "(" << line << ") ";
            _flag = Logger::_level & level;
        }
        LoggerStream(const LoggerStream &ls) : _obj(ls._obj) {}
        ~LoggerStream() {
            if (_flag) output();
        }
    private:
        void output() {
            std::unique_lock<std::mutex> lock(_obj._mutex);
            std::ostringstream &now = *this;
            switch (_obj._target) {
            case LogLevel::FILES:
                this->_ofile.open(_obj._path, std::ios::out | std::ios::app);
                this->_ofile << this->str() << std::endl;
                now.flush();
                this->_ofile.close();
                break;
            case LogLevel::TERMINAL:
                now << this->str() << std::endl;
                now.flush();
                break;
            default:
                now << this->str() << std::endl;
                now.flush();
                break;
            }
            return ;
        }
        Logger &_obj;
        std::ofstream _ofile;
        int _flag;
    };

public:
    LoggerStream operator()(const char *file, const char *func, int line, int level) {
        return LoggerStream(file, func, line, *this, level);
    }
    static void set_level(int level) {
        Logger::_level = level;
        return ;
    }
    static void set_target(int target, const char *path) {
        Logger::_target = target;
        Logger::_path = path;
        return ;
    }
private:
    std::mutex _mutex;
    static int _level;
    static int _target;
    static const char *_path;
};

// int Logger::_level = 15;
// int Logger::_target = 15;
// const char *Logger::_path = "./";
// Logger logger;

}

#endif