#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <stddef.h>

// 网络库底层的缓冲区类型定义
class Buffer
{
public:
    static const size_t kCheapPrepend = 8;  //kCheapPrepend预留 8 字节头部空间，便于在数据前添加元信息（如长度字段）。
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initalSize = kInitialSize)
        : buffer_(kCheapPrepend + initalSize),
          readerIndex_(kCheapPrepend),
          writerIndex_(kCheapPrepend)
{
}

    size_t readableBytes() const { return writerIndex_ - readerIndex_; }
    size_t writableBytes() const { return buffer_.size() - writerIndex_; }
    size_t prependableBytes() const { return readerIndex_; }

    //数据检索（retrieve）方法
    const char *peek() const { return begin() + readerIndex_; }  // 返回缓冲区中可读数据的起始地址
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            readerIndex_ += len;  // 仅移动读指针
        }
        else
        {
            retrieveAll();  // 读取全部数据
        }
    }
    void retrieveAll()
    {
        readerIndex_ = kCheapPrepend;
        writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString() { return retrieveAsString(readableBytes()); }
    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        retrieve(len);  // 上面一句把缓冲区中可读的数据已经读取出来 这里肯定要对缓冲区进行复位操作
        return result;
    }

    void ensureWritableBytes(size_t len)
    {
        if(writableBytes() < len)
        {
            makeSpace(len);  // 扩容
        }
    }

    // 把[data, data+len]内存上的数据添加到writable缓冲区当中
    void append(const char *data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data+len, beginWrite());
        writerIndex_ += len;
    }
    char *beginWrite() { return begin() + writerIndex_; }
    const char *beginWrite() const { return begin() + writerIndex_; }

    // 从fd上读取数据
    ssize_t readFd(int fd, int *saveErrno);
    // 通过fd发送数据
    ssize_t writeFd(int fd, int *saveErrno);

private:
    // vector底层数组首元素的地址 也就是数组的起始地址
    char *begin() { return &*buffer_.begin(); }
    const char *begin() const { return &*buffer_.begin(); }

    void makeSpace(size_t len)
    {
        if(writableBytes() + prependableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, begin() + writerIndex_, begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }
    
    std::vector<char> buffer_;
    size_t readerIndex_;  // 读指针位置
    size_t writerIndex_;  // 写指针位置
};