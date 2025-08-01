#pragma once

#include "noncopyable.h"
#include "Thread.h"
#include "FixedBuffer.h"
#include "LogStream.h"
#include "LogFile.h"

#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>

class AsyncLogging{
public:
    AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval = 3);
    ~AsyncLogging(){
        if(running_){
            stop();
        }
    }
    void append(const char *logline, int len);
    void start(){
        running_ = true;
        thread_.start();
    }
    void stop(){
        running_ = false;
        cond_.notify_one();
    }

private:
    typedef FixedBuffer<kLargeBufferSize> LargeBuffer;
    typedef std::vector<std::unique_ptr<LargeBuffer>> BufferVector;
    // BufferVector::value_type 是 std::vector<std::unique_ptr<Buffer>> 的元素类型，也就是 std::unique_ptr<Buffer>。
    typedef BufferVector::value_type BufferPtr;
    void threadFunc();
    const int flushInterval_;
    std::atomic<bool> running_;
    const std::string basename_;
    const off_t rollSize_;
    Thread thread_;
    std::mutex mutex_;
    std::condition_variable cond_;

    BufferPtr currentBuffer_;
    BufferPtr nextBuffer_;
    BufferVector buffers_;

};
