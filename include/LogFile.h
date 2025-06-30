#pragma once
#include "FileUtil.h"
#include <mutex>
#include <memory>
#include <ctime>

class LogFile{
public:
    /**
     * @brief 构造函数
     * @param basename 日志文件基本名称
     * @param rollsize 日志文件大小达到多少字节时滚动，单位:字节
     * @param flushInterval 日志刷新间隔时间，默认3秒
     * @param checkEveryN_ 写入checkEveryN_次后检查是否需要滚动，默认1024次
     */
    LogFile(const std::string &basename, off_t rollsize, int flushInterval = 3, int checkEveryN_ = 1024);
    ~LogFile();

    void append(const char *data, int len);

    void flush();

    bool rollFile();

private:
    static std::string getLogFileName(const std::string &basename, time_t *now);
    void appendInlock(const char *data, int len);

    const std::string basename_;
    const off_t rollsize_;
    const int flushInterval_;
    const int checkEveryN_;
    
    int count_;

    std::mutex mutex_;
    time_t startOfPeriod_;// 本次写log周期的起始时间(秒)
    time_t lastRoll_;
    time_t lastFlush_;
    std::unique_ptr<FileUtil> file_;
    const static int kRollPerSeconds_ = 60*60*24;
};