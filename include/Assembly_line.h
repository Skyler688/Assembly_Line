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

// This is the data type used to create the assemblyLines.
using Task = std::function<void(std::any &data, int id)>;

int hardwareThreads();

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
    void WaitAll();

    void StartTime();
    long long EndTime();

    ~AssemblyLine();
    
private:
    void workerThread(int id);
    void waitForWorkersToDie();

    std::vector<std::thread> workers;

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
    bool kill_threads;

    std::vector<bool> sleeping;
    std::vector<bool> is_async;
    std::vector<bool> dead;

    std::vector<std::vector<Task>> assemblyLines;

    std::mutex mtx; 

    std::condition_variable thread_wake;
    std::condition_variable thread_is_async;
    std::condition_variable thread_is_dead;

    // will be used for extra features latter.
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Micro = std::chrono::duration<double, std::micro>; // creating a microseconds type.

    TimePoint start_time;
    TimePoint end_time;
};