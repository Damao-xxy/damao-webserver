#ifndef TIMER_QUEUE_H
#define TIMER_QUEUE_H

#include "Timestamp.h"
#include "Channel.h"

#include <vector>
#include <set>

class EventLoop;
class Timer;

class TimerQueue{
public:
    typedef std::function<void()> TimerCallback;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    void addTimer(TimerCallback cb, Timestamp when, double interval);

private:
    typedef std::pair<Timestamp, Timer*> Entry;
    typedef std::set<Entry> TimerList;

    void addTimerInLoop(Timer* timer);

    void handleRead();

    void resetTimerfd(int timerfd_, Timestamp expiration);

    std::vector<Entry> getExpired(Timestamp now);
    void reset(const std::vector<Entry>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;         // 所属的EventLoop
    const int timerfd_;       // timerfd是Linux提供的定时器接口
    Channel timerfdChannel_;  // 封装timerfd_文件描述符
    TimerList timers_;        // 定时器队列（内部实现是红黑树）

    bool callingExpiredTimers_; // 标明正在获取超时定时器
};

#endif