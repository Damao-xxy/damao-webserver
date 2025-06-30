#include "AsyncLogging.h"
#include <stdio.h>
AsyncLogging::AsyncLogging(const std::string &basename, off_t rollSize, int flushInterval)
    : flushInterval_(flushInterval),
    running_(false),
    basename_(basename),
    rollSize_(rollSize),
    thread_(std::bind(&AsyncLogging::threadFunc, this), "Logging"),
    mutex_(),
    cond_(),
    currentBuffer_(new LargeBuffer),
    nextBuffer_(new LargeBuffer),
    buffers_()
{
    currentBuffer_->bzero();
    nextBuffer_->bzero();
    buffers_.reserve(16); // 只维持队列长度2~16.
}

void AsyncLogging::append(const char *logline, int len){
    std::lock_guard<std::mutex> lg(mutex_);
    if(currentBuffer_->avail() > static_cast<size_t>(len)){
        currentBuffer_->append(logline, len);
    }else{
        buffers_.push_back(std::move(currentBuffer_));
        if(nextBuffer_){
            currentBuffer_ = std::move(nextBuffer_);
        }else{
            currentBuffer_.reset(new LargeBuffer);
        }
        currentBuffer_->append(logline, len);
        // 唤醒后端线程写入磁盘
        cond_.notify_one();
    }
}

void AsyncLogging::threadFunc(){
    // output写入磁盘接口
    LogFile output(basename_, rollSize_);
    BufferPtr newBuffer1(new LargeBuffer);
    BufferPtr newBuffer2(new LargeBuffer);
    newBuffer1->bzero();
    newBuffer2->bzero();
    // 缓冲区数组置为16个，用于和前端缓冲区数组进行交换
    BufferVector buffersToWrite;
    buffersToWrite.reserve(16);
    while(running_){
        {
            // 互斥锁保护这样就保证了其他前端线程无法向前端buffer写入数据
            std::unique_lock<std::mutex> lg(mutex_);
            if(buffers_.empty()){
                cond_.wait_for(lg, std::chrono::seconds(3));
            }
            buffers_.push_back(std::move(currentBuffer_));
            currentBuffer_ = std::move(newBuffer1);
            if(!nextBuffer_){
                nextBuffer_ = std::move(newBuffer2);
            }
            buffersToWrite.swap(buffers_);
        }
        // 从待写缓冲区取出数据通过LogFile提供的接口写入到磁盘中
        for(auto &buffer : buffersToWrite){
            output.append(buffer->data(), buffer->length());
        }

        if(buffersToWrite.size() > 2){
            buffersToWrite.resize(2);
        }

        if(!newBuffer1){
            newBuffer1 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer1->reset();
        }

        if(!newBuffer2){
            newBuffer2 = std::move(buffersToWrite.back());
            buffersToWrite.pop_back();
            newBuffer2->reset();
        }
        buffersToWrite.clear();  // 清空后端缓冲队列
        output.flush();   // 清空文件夹缓冲区
    }
    output.flush();  // 确保一定清空。

}