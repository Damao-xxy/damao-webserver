#ifndef TIME_STAMP_H
#define TIME_STAMP_H

#include <iostream>
#include <string>
#include <sys/time.h>

class Timestamp{
public:
    Timestamp() : microSecondsSinceEpoch_(0){}

    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch){}

    // 获取当前时间戳
    static Timestamp now();
    std::string toString() const;

    //格式, "%4d年%02d月%02d日 星期%d %02d:%02d:%02d.%06d",时分秒.微秒
    std::string toFormattedString(bool showMicroseconds = false) const;

    //返回当前时间戳的微秒
    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
    //返回当前时间戳的秒数
    time_t secondsSinceEpoch() const {
        return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond);
    }

    // 失效的时间戳，返回一个值为0的Timestamp
    static Timestamp invalid(){
        return Timestamp();
    }

    static const int kMicroSecondsPerSecond = 1000*1000;

private:
    // 表示时间戳的微秒数(自epoch开始经历的微妙数)
    int64_t microSecondsSinceEpoch_;

};

inline bool operator<(Timestamp lhs, Timestamp rhs){
    return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}
inline bool operator==(Timestamp lhs, Timestamp rhs){
    return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

inline Timestamp addTime(Timestamp timestamp, double seconds){
    int64_t delta = static_cast<int64_t>(seconds*Timestamp::kMicroSecondsPerSecond);
    return Timestamp(timestamp.microSecondsSinceEpoch()+delta);
}

#endif