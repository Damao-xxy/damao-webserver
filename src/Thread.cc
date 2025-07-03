#include <Thread.h>
#include <CurrentThread.h>

#include <semaphore.h>

std::atomic_int Thread::numCreated_(0);

Thread::Thread(ThreadFunc func, const std::string &name)
    : started_(false)
    , joined_(false)
    , tid_(0)
    , func_(std::move(func))
    , name_(name)
{
    setDefaultName();
}

Thread::~Thread()
{
    if(started_ && !joined_)
    {
        thread_->detach();  //// thread类提供了设置分离线程的方法 线程运行后自动销毁（非阻塞）
    }
}

/*
信号量同步：
使用 POSIX 信号量 sem_t 确保主线程等待新线程获取 tid_ 后再返回。
sem_post()：新线程获取 tid_ 后释放信号量。
sem_wait()：主线程等待，直到 tid_ 被正确设置。

线程执行流程：
新线程启动后，首先调用 CurrentThread::tid() 获取真实线程 ID。
通过信号量通知主线程 tid_ 已就绪。
执行用户传入的函数 func_。
*/

void Thread::start()  // 一个Thread对象 记录的就是一个新线程的详细信息
{
    started_ = true;
    sem_t sem;
    sem_init(&sem, false, 0);  // 初始化信号量（初始值为 0）

    thread_ = std::shared_ptr<std::thread>(new std::thread([&](){
        tid_ = CurrentThread::tid();  // 获取线程的tid值
        sem_post(&sem);  // 释放信号量（值+1）
        func_();  // 执行用户函数
    }));
    
    sem_wait(&sem);  // 等待信号量（阻塞直到值>0）
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}

void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf, sizeof buf, "Thread%d", num);
        name_ = buf;
    }
}
