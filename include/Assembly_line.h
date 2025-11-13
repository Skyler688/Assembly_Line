#pragma once

#include <functional>
#include <vector>
#include <printf.h>
#include <thread>
#include <any>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <chrono>

using Task = std::function<void(std::any &data, int id)>;

class AssemblyLine
{
public:
    AssemblyLine();
    AssemblyLine(int threads);

    int CreateAssemblyLine(std::vector<Task> assemblyLine);
    void AddToBuffer(int assemblyLineID, std::any data);
    void AddToAsyncBuffer(int assemblyLineID, std::any data);
    int LaunchQueue();
    int LaunchAsyncQueue();
    // void Wait();
    // int Pause();
    // int GetSpeed();
    
    ~AssemblyLine();
    
private:
    struct Job
    {
        std::any data;
        int taskIndex;
        int lineID;
        int jobLength;
    };

    std::vector<Job> activeQueue;
    std::vector<Job> activeAsyncQueue;
    std::vector<Job> bufferQueue; 
    std::vector<Job> asyncBufferQueue;

    bool async_flag;

    std::vector<bool> sleeping;
    std::vector<bool> is_async;

    std::vector<std::vector<Task>> assemblyLines;

    void workerThread(int id);

    std::mutex mtx;
    std::condition_variable thread_wake;
    std::condition_variable jobs_done;
    std::condition_variable thread_is_async;

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milli = std::chrono::duration<double, std::milli>; // creating a millisecond type.

    TimePoint startTime;
    TimePoint endTime;

    bool pause_flag;
};