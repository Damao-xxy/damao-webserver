#include <functional>
#include <string>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>

#include <TcpConnection.h>
#include <Logger.h>
#include <Socket.h>
#include <Channel.h>
#include <EventLoop.h>

static EventLoop *CheckLoopNotNull(EventLoop *loop)
{
    if(loop == nullptr)
    {
        LOG_FATAL<<"mainLoop is null!";
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop *loop,
                             const std::string &nameArg,
                             int sockfd,
                             const InetAddress &localAddr,
                             const InetAddress &peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop, sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024)
{
    channel_->setReadCallback(std::bind(&TcpConnection::handleRead, this, std::placeholders::_1));
    channel_->setWriteCallback(std::bind(&TcpConnection::handleWrite, this));
    channel_->setCloseCallback(std::bind(&TcpConnection::handleClose, this));
    channel_->setErrorCallback(std::bind(&TcpConnection::handleError, this));

    LOG_INFO<<"TcpConnection::ctor: ["<<name_.c_str()<<"] st fd="<<sockfd;
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO<<"TcpConnection::dtor["<<name_.c_str()<<"] at fd="<<channel_->fd()<<"state="<<(int)state_;
}

void TcpConnection::send(const std::string &buf)
{
    if(state_ == kConnected)
    {
        if(loop_->isInLoopThread())
        {
            sendInLoop(buf.c_str(), buf.size());
        }
        else
        {
            loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));
        }
    }
}

/**
 * 发送数据 应用写的快 而内核发送数据慢 需要把待发送数据写入缓冲区，而且设置了水位回调
 **/
void TcpConnection::sendInLoop(const void *data, size_t len)
{
    ssize_t nwrote = 0;
    ssize_t remaining = len;
    bool faultError = false;

    if(state_ == kDisconnected)
    {
        LOG_ERROR<<"disconnected, give up writing";
    }

    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        nwrote = ::write(channel_->fd(), data, len);
        if(nwrote >= 0)
        {
            remaining = len - nwrote;
            if(remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            nwrote = 0;
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR<<"TcpConnection::sendInLoop";
                if(errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }

    if(!faultError && remaining > 0)
    {
        size_t oldLen = outputBuffer_.readableBytes();
        if(oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highWaterMarkCallback_)
        {
            loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
        }
        outputBuffer_.append((char *)data + nwrote, remaining);
        if(!channel_->isWriting())
        {
            channel_->enableWriting();
        }
    }
}

void TcpConnection::shutdown()
{
    if(state_ == kConnected)
    {
        setState(kDisconnecting);
        loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
    }
}

void TcpConnection::shutdownInLoop()
{
    if(!channel_->isWriting())  // 说明当前outputBuffer_的数据全部向外发送完成
    {
        socket_->shutdownWrite();
    }
}

// 连接建立
void TcpConnection::connectEstablished()
{
    setState(kConnected);
    channel_->tie(shared_from_this());
    channel_->enableReading();  // 向poller注册channel的EPOLLIN读事件

    // 新连接建立 执行回调
    connectionCallback_(shared_from_this());
}

// 连接销毁
void TcpConnection::connectDestroyed()
{
    if(state_ == kConnected)
    {
        setState(kDisconnected);
        channel_->disableAll();
        connectionCallback_(shared_from_this());
    }
    channel_->remove();
}

// 读是相对服务器而言的 当对端客户端有数据到达 服务器端检测到EPOLLIN 就会触发该fd上的回调 handleRead取读走对端发来的数据
void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0)  // 有数据到达
    {
        // 已建立连接的用户有可读事件发生了 调用用户传入的回调操作onMessage shared_from_this就是获取了TcpConnection的智能指针
        messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
    }
    else if(n == 0)  // 客户端断开
    {
        handleClose();
    }
    else  // 出错了
    {
        errno = savedErrno;
        LOG_ERROR<<"TcpConnection::handleRead";
        handleError();
    }
}

void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int savedErrno = 0;
        ssize_t n = outputBuffer_.writeFd(channel_->fd(), &savedErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);  //从缓冲区读取reable区域的数据移动readindex下标
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    // TcpConnection对象在其所在的subloop中 向pendingFunctors_中加入回调
                    loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();  // 在当前所属的loop中把TcpConnection删除掉
                }
            }
        }
        else
        {
            LOG_ERROR<<"TcpConnection::handleWrite";
        }
    }
    else
    {
        LOG_ERROR<<"TcpConnection fd="<<channel_->fd()<<"is down, no more writing";
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO<<"TCPConnection::handleClose fd="<<channel_->fd()<<" state="<<(int)state_;
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr);  // 连接回调
    closeCallback_(connPtr);  // 执行关闭连接的回调 执行的是TcpServer::removeConnection回调方法
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof optval;
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR<<"TcpConnection::handleError name: "<<name_.c_str()<<" - SO_ERROR:% "<<err;
}

void TcpConnection::sendFile(int fileDescriptor, off_t offset, size_t count)
{
    if(connected())
    {
        if(loop_->isInLoopThread())
        {
            sendFileInLoop(fileDescriptor, offset, count);
        }
        else
        {
            loop_->runInLoop(
                std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, count));
        }
    }
    else
    {
        LOG_ERROR<<"TcpConnection::sendFile - not connected";
    }
}

void TcpConnection::sendFileInLoop(int fileDescriptor, off_t offset, size_t count)
{
    ssize_t bytesSent = 0;
    size_t remaining = count;
    bool faultError = false;

    if(state_ == kDisconnecting)  // 表示此时连接已经断开就不需要发送数据了
    {
        LOG_ERROR<<"disconnected, give up sending";
        return;
    }

    // 表示Channel第一次开始写数据或者outputBuffer缓冲区中没有数据
    if(!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
    {
        bytesSent = sendfile(socket_->fd(), fileDescriptor, &offset, remaining);
        if(bytesSent >= 0)
        {
            remaining -= bytesSent;
            if(remaining == 0 && writeCompleteCallback_)
            {
                loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
            }
        }
        else
        {
            if(errno != EWOULDBLOCK)
            {
                LOG_ERROR<<"TcpConnection::sendFileInLoop";
                if(errno == EPIPE || errno == ECONNRESET)
                {
                    faultError = true;
                }
            }
        }
    }
    // 处理剩余数据
    if(!faultError && remaining > 0)
    {
        // 继续发送剩余数据
        loop_->queueInLoop(
            std::bind(&TcpConnection::sendFileInLoop, shared_from_this(), fileDescriptor, offset, remaining));
        
    }    
}