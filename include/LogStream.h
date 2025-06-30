#pragma once
#include <string.h>
#include <string>
#include "noncopyable.h"
#include "FixedBuffer.h"
class GeneralTemplate : noncopyable{
public:
    GeneralTemplate() : data_(nullptr), len_(0) {}

    explicit GeneralTemplate(const char* data, int len) : data_(data), len_(len) {}

    const char* data_;
    int len_;
};
// LogStream类用于管理日志输出流，重载输出流运算符<<，将各种类型的值写入内部缓冲区
class LogStream : noncopyable {
public:
    typedef FixedBuffer<kSmallBufferSize> Buffer;

    void append(const char *buffer, int len){
        buffer_.append(buffer, len);
    }

    const Buffer &buffer() const{
        return buffer_;
    }

    void reset_buffer(){
        buffer_.reset();
    }

    // 重载输出流运算符<<，用于将不同类型的值写入缓冲区
    LogStream &operator<<(bool express);
    LogStream &operator<<(short number);
    LogStream &operator<<(unsigned short);
    LogStream &operator<<(int);
    LogStream &operator<<(unsigned int);
    LogStream &operator<<(long);
    LogStream &operator<<(unsigned long);
    LogStream &operator<<(long long);
    LogStream &operator<<(unsigned long long);
    LogStream &operator<<(float number);
    LogStream &operator<<(double);
    LogStream &operator<<(char str);
    LogStream &operator<<(const char *);
    LogStream &operator<<(const unsigned char *);
    LogStream &operator<<(const std::string &);
    LogStream &operator<<(const GeneralTemplate& g);
    
private:
    // 定义最大数字大小常量
    static constexpr int kMaxNumberSize = 32;

    // 对于整型需要特殊的处理，模板函数用于格式化整型
    template <typename T>
    void formatInteger(T num);

    Buffer buffer_;

};