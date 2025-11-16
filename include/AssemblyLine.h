#pragma once

// #include <printf.h>
#include <functional>
#include <vector>
#include <deque>
#include <thread>
#include <any>
#include <iostream>
#include <mutex>
#include <condition_variable>

// This is the data type used to create the assemblyLines.
using Task = std::function<void(std::any &data)>;

struct Result
{
    int size;
    std::vector<std::any> data;

    // Explicit default constructor
    Result() : size(0), data({}) {}
};

using SyncResults = std::vector<Result>;
using AsyncResults = std::vector<Result>;

int hardwareThreads();

class AssemblyLine
{
public:
    AssemblyLine();
    AssemblyLine(int threads);

    int CreateAssemblyLine(const std::vector<Task> &assembly_line);
    void AddToBuffer(int assembly_line_id, std::any data);
    void AddToAsyncBuffer(int assembly_line_id, std::any data);
    void LaunchQueue(SyncResults &results);
    int LaunchAsyncQueue(AsyncResults &results);
    // void WaitAll(); // change to wait for async, can be usfull in the event of async task neading to be done by sertain time.

    ~AssemblyLine();
    
private:
    void workerThread();
    void waitForWorkersToDie();

    // Helpers
    void wakeSleepingThreads();

    // Worker thread list, used in the deconstructor to join threads.
    std::vector<std::thread> workers; 
    
    // The list of functions that make up an assembly line.
    std::vector<std::vector<Task>> assembly_lines;
    int assembly_line_count; // Used to track the total assembly lines without needing to use a mutex lock.

    // The structure used in the queue's
    struct Job
    {
        std::any data;
        int task_index;
        int line_id; // assembly_lines index
        int job_length;
    };

    // The deque data type allows for O(1) insertion and deletion from both ends, a vector would require shifting every element leading to O(N).
    std::deque<Job> sync_queue;
    std::deque<Job> async_queue;
    std::deque<Job> sync_buffer; 
    std::deque<Job> async_buffer;

    int thread_count;

    // Flags
    bool kill_threads;
    int threads_sleeping; // Using vector because compile time size is unknown and requires access by index.
    int threads_async;
    int threads_dead;

    std::mutex mtx; 

    std::condition_variable thread_wake;
    std::condition_variable thread_is_async;
    std::condition_variable thread_is_dead;

    SyncResults sync_results;
    AsyncResults async_results;
};