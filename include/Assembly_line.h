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
    void LaunchQueue();
    void LaunchAsyncQueue();
    void Wait();
    void Pause();
    int GetSpeed();
    
    ~AssemblyLine();
    
private:
    struct Job
    {
        std::any data;
        int taskIndex;
        int lineID;
    };

    std::vector<Job> activeQueue;
    std::vector<Job> bufferQueue; 
    std::vector<Job> asyncBufferQueue;

    std::vector<bool> sleeping;

    std::vector<std::vector<Task>> assemblyLines;

    void workerThread(int id);

    std::mutex mtx;
    std::condition_variable thread_wake;
    std::condition_variable jobs_done;

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Milli = std::chrono::duration<double, std::milli>; // creating a millisecond type.

    TimePoint startTime;
    TimePoint endTime;
};