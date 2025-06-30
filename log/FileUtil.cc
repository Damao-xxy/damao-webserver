#include <cstring>
#include "FileUtil.h"

/*
"ae" 模式表示以追加方式打开文件（a），并启用 O_CLOEXEC 标志（e，Linux 特有）。
O_CLOEXEC 确保文件描述符在 exec 系统调用后保持打开状态。
*/
FileUtil::FileUtil(std::string &file_name) : file_(::fopen(file_name.c_str(), "ae")), writtenBytes_(0)
{
    // 将文件的标准 I/O 缓冲区替换为自定义的 buffer_（64KB），减少 write 系统调用次数。
    ::setbuffer(file_, buffer_, sizeof(buffer_));
}
FileUtil::~FileUtil()
{
    if(file_)
    {
        ::fclose(file_);
    }
}
// 向文件写入数据
void FileUtil::append(const char *data, size_t len)
{
    size_t writen = 0;
    while(writen != len)
    {
        size_t remain = len - writen;
        size_t n = write(data + writen, remain);
        if(n != remain)
        {
            // 错误判断
            int err = ferror(file_);
            if(err)
            {
                fprintf(stderr, "AppendFile::append() failed %s\n", strerror(err));
                clearerr(file_);  //  清除文件指针的错误标志
                break;
            }
        }
        writen += n;
    }
    writtenBytes_ += writen;
}

void FileUtil::flush()
{
    ::fflush(file_);
}
// 真正向文件写入数据
size_t FileUtil::write(const char *data, size_t len)
{
    /*
    使用 fwrite_unlocked 替代 fwrite，避免标准 I/O 库的全局锁，提升多线程环境下的性能。
    fwrite 内部使用锁保护文件指针，fwrite_unlocked 跳过锁操作，适合已知线程安全的场景。
    */
    return ::fwrite_unlocked(data, 1, len, file_);
}