#include <errno.h>
#include <sys/uio.h>
#include <unistd.h>

#include <Buffer.h>

/**
 * 从fd上读取数据 Poller工作在LT模式
 * Buffer缓冲区是有大小的！ 但是从fd上读取数据的时候 却不知道tcp数据的最终大小
 *
 * @description: 从socket读到缓冲区的方法是使用readv先读至buffer_，
 * Buffer_空间如果不够会读入到栈上65536个字节大小的空间，然后以append的
 * 方式追加入buffer_。既考虑了避免系统调用带来开销，又不影响数据的接收。
 **/
ssize_t Buffer::readFd(int fd, int *saveErrno)
{
    // 栈额外空间，用于从套接字往出读时，当buffer_暂时不够用时暂存数据，待buffer_重新分配足够空间后，在把数据交换给buffer_。
    char extrabuf[65536] = {0};

    /*
    struct iovec {
        ptr_t iov_base; // iov_base指向的缓冲区存放的是readv所接收的数据或是writev将要发送的数据
        size_t iov_len; // iov_len在各种情况下分别确定了接收的最大长度以及实际写入的长度
    };
    */

    // 使用iovec分配两个连续的缓冲区
    struct iovec vec[2];
    const size_t writable = writableBytes();  // 这是Buffer底层缓冲区剩余的可写空间大小 不一定能完全存储从fd读出的数据

    // 第一块缓冲区，指向可写空间
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;
    // 第二块缓冲区，指向栈空间
    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    /*
    若当前缓冲区的可写空间 writable 小于 extrabuf 的大小，使用2个缓冲区读取：
    主缓冲区（buffer_）
    辅助缓冲区（extrabuf）
    否则，仅使用1 个缓冲区（主缓冲区）读取。
    */
    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;  //iovcnt：iovec数组中的元素数量。
    const ssize_t n = ::readv(fd, vec, iovcnt);  //从fd读取数据，按顺序写入iov指定的多个缓冲区，返回实际读取的字节数。

    if(n < 0)
    {
        *saveErrno = errno;
    }
    else if(n <= writable)
    {
        writerIndex_ +=n;
    }
    else
    {
        writerIndex_ = buffer_.size();
        append(extrabuf, n - writable);
    }
    return n;
}

// inputBuffer_.readFd表示将对端数据读到inputBuffer_中，移动writerIndex_指针
// outputBuffer_.writeFd表示将数据写入到outputBuffer_中，从readerIndex_开始，可以写readableBytes()个字节
ssize_t Buffer::writeFd(int fd, int *saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if(n < 0)
    {
        *saveErrno = errno;
    }
    return n;
}