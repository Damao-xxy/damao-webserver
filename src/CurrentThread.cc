#include <CurrentThread.h>

namespace CurrentThread
{
    thread_local int t_cachedTid = 0;  // 在源文件中定义线程局部变量
    void cacheTid()
    {
        if(t_cachedTid == 0)
        {
            /*
            syscall(SYS_gettid)：在 Linux 系统中获取当前线程的真实 ID
            static_cast<pid_t>：将系统调用结果转换为pid_t类型
            */
            t_cachedTid = static_cast<pid_t>(::syscall(SYS_gettid));
        }
    }
}