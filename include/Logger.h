#pragma once

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <errno.h>
#include "LogStream.h"
#include <functional>
#include "Timestamp.h"

#define OPEN_LOGGING

class SourceFile{
public:
    explicit SourceFile(const char* filename) : data_(filename){
        /**
         * 找出data中出现/最后一次的位置，从而获取具体的文件名
         * 2022/10/26/test.log
         */
        const char* slash = strrchr(filename, '/');
        if(slash){
            data_ = slash + 1;
        }
        size_ = static_cast<int>(strlen(data_));
    }

    const char* data_;
    int size_;
};
class Logger{
public:
    enum LogLevel{
        TRACE,
        DEBUG,
        INFO,
        WARN,
        ERROR,
        FATAL,
        LEVEL_COUNT,
    };
    Logger(const char *filename, int line, LogLevel level);
    ~Logger();

    LogStream& stream(){ return impl_.stream_; }

    typedef std::function<void(const char* msg, int len)> OutputFunc;
    typedef std::function<void()> FlushFunc;
    static void setOutput(OutputFunc);
    static void setFlush(FlushFunc);

private:
    class Impl{
    public:
        typedef Logger::LogLevel LogLevel;
        Impl(LogLevel level, int saveErrno, const char *filename, int line);
        void formatTime();
        void finish();

        Timestamp time_;
        LogStream stream_;
        LogLevel level_;
        int line_;
        SourceFile basename_;
    };

    Impl impl_;
};

const char* getErrnoMsg(int savedErrno);
/**
 * 当日志等级小于对应等级才会输出
 * 比如设置等级为FATAL，则logLevel等级大于DEBUG和INFO，DEBUG和INFO等级的日志就不会输出
 */
#ifdef OPEN_LOGGING
#define LOG_DEBUG Logger(__FILE__, __LINE__, Logger::DEBUG).stream();
#define LOG_INFO Logger(__FILE__, __LINE__, Logger::INFO).stream();
#define LOG_WARN Logger(__FILE__, __LINE__, Logger::WARN).stream();
#define LOG_ERROR Logger(__FILE__, __LINE__, Logger::ERROR).stream();
#define LOG_FATAL Logger(__FILE__, __LINE__, Logger::FATAL).stream();
#else
#define LOG(level) LogStream()
#endif