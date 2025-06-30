#pragma once
#include <string>
#include <stdio.h>  //C 标准输入输出库，提供 FILE*、fwrite 等文件操作函数
#include <sys/types.h>  //提供系统类型定义，如 off_t（文件偏移量类型）

class FileUtil{
public:
    FileUtil(std::string& file_name);
    ~FileUtil();

    void append(const char* data, size_t len);
    void flush();

    off_t writtenBytes() const { return writtenBytes_; }

private:
    size_t write(const char* data, size_t len);
    FILE* file_;           // 文件指针，用于操作文件
    char buffer_[64*1024]; // 文件操作的缓冲区，大小为64KB，用于提高写入效率
    off_t writtenBytes_;  // 记录已写入文件的总字节数，off_t类型用于大文件支持
};