#include "Assembly_line.h"

int hardwareThreads()
{
    return std::thread::hardware_concurrency();
}

// Private
void AssemblyLine::workerThread(int id)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mtx);

        bool local_async_flag;

        
        // Will wait "sleep" once the queue is empty.
        thread_wake.wait(lock, [&] {
            if (kill_threads)
            {
                return true;
            }
            else if (!activeQueue.empty())
            {
                local_async_flag = false;
                is_async[id] = local_async_flag;

                if (sleeping[id])
                {
                    // printf("AWAKE: %d\n", id);
                }
                sleeping[id] = false;
                return true;
            }
            else 
            {
                local_async_flag = true;
                is_async[id] = local_async_flag;

                thread_is_async.notify_one();

                if (activeAsyncQueue.empty())
                {
                    if (!sleeping[id])
                    {
                        // printf("ASYNC SLEPT: %d\n", id);
                    }
                    sleeping[id] = true;
                    return false;
                }
                else 
                {
                    if (sleeping[id])
                    {
                        // printf("ASYNC AWAKE: %d\n", id);
                    }
                    sleeping[id] = false;
                    return true;
                }
            }
        });

        if (kill_threads)
        {
            break;
        }

        // Grab the task from the end of the list and remove it.
        Job job;

        if (!local_async_flag)
        {
            job = activeQueue.front();
            // printf("sync\n");
            activeQueue.pop_front();
        }
        else
        {
            job = activeAsyncQueue.front();
            // printf("async\n");
            activeAsyncQueue.pop_front();
        }

        Task task = assemblyLines[job.lineID][job.taskIndex];
        std::any jobData = job.data;
        
        // Unlock the mutex so other threads can have access. 
        lock.unlock();

    
        // Run the passed function.
        task(jobData, id); // passed by reference so i can replace the jobData with data for the next function.

        // if there is a next job in the line add it to queue.
        if (job.jobLength - 1 > job.taskIndex)
        {
            Job nextJob;
            nextJob.taskIndex = job.taskIndex + 1;
            nextJob.data = jobData; // Make sure to replace the passed data with new data.
            nextJob.lineID = job.lineID;
            nextJob.jobLength = job.jobLength;

            // lock again
            lock.lock();
            if (!local_async_flag) // using a local flag collected during job selection to prevent adding job to wrong buffer.
            {
                activeQueue.push_front(nextJob); // adding to the front of the queue to fallow FIFO "first in first out".
            }
            else
            {
                activeAsyncQueue.push_front(nextJob);
            }
            
            // Given the potential variation in tasks within each assembly line it could cause the queue to become temperarlly empty,
            // putting threads to sleep before all the work is done. By calling the notify_one() this will wake a sleeping thread if available preventing 
            // premature thread sleeping. This is most notable with tasks that have a very large discrepancy from the rest causing the queue to empty
            // near the end of the run before suddenly adding more work back in the queue.
            thread_wake.notify_one(); 
            lock.unlock();
        } 
    }

    std::lock_guard<std::mutex> lock(mtx);
    dead[id] = true;
    thread_is_dead.notify_one();
}

// Public
AssemblyLine::AssemblyLine()
{
    int numOfThreads = std::thread::hardware_concurrency();

    async_flag = true;
    kill_threads = false;
    
    for (int i = 0; i < numOfThreads; i++)
    {
        sleeping.push_back(false);
        is_async.push_back(true);
        dead.push_back(false);

        std::thread worker(&AssemblyLine::workerThread, this, i);
        workers.push_back(std::move(worker));
    }
}

AssemblyLine::AssemblyLine(int threads)
{
    // printf("Creating workers\n");
    async_flag = true;
    kill_threads = false;

    for (int i = 0; i < threads; i++)
    {
        sleeping.push_back(false);
        is_async.push_back(true);
        dead.push_back(false);

        std::thread worker(&AssemblyLine::workerThread, this, i);
        workers.push_back(std::move(worker));
    }
    // printf("Workers created\n");
}

int AssemblyLine::CreateAssemblyLine(std::vector<Task> assemblyLine)
{
    assemblyLines.push_back(assemblyLine);
    return assemblyLines.size() - 1; // Return the assembly lines index
}

void AssemblyLine::AddToBuffer(int assemblyLineID, std::any data)
{
    Job job;
    job.data = data;
    job.lineID = assemblyLineID;
    job.taskIndex = 0;
    job.jobLength = assemblyLines[assemblyLineID].size();

    bufferQueue.push_back(job); // add jobs to the back of the queue to fallow FIFO "first in first out"
}

void AssemblyLine::AddToAsyncBuffer(int assemblyLineID, std::any data)
{
    Job job;
    job.data = data;
    job.lineID = assemblyLineID; // NOTE -> Can just use the same ID list for the async functions as well.
    job.taskIndex = 0;
    job.jobLength = assemblyLines[assemblyLineID].size();

    asyncBufferQueue.push_back(job);
}

int AssemblyLine::LaunchQueue()
{    
    std::unique_lock<std::mutex> lock(mtx); 

    // Wait for all threads to be async, otherwise the transition is not over and will block until all synchronous tasks are done.
    thread_is_async.wait(lock, [&] {
        for (size_t i = 0; i < is_async.size(); i++)
        {
            if (!is_async[i])
            {
                return false;
            }

        }

        // printf("activeQueue: %d\n", activeQueue.size());

        if (activeQueue.empty())
        {
            return true;
        }
        return false;
    });

    if (!bufferQueue.empty())
    {
        activeQueue.insert(
            activeQueue.end(),
            bufferQueue.begin(),
            bufferQueue.end()
        );

        bufferQueue.clear(); // Clear the buffer once transferred to the active queue.

        // check if any threads are sleeping
        bool threads_sleeping = false;
        for (size_t i = 0; i < sleeping.size(); i++)
        {
            if (sleeping[i]) 
            {
                threads_sleeping = true;
                break;
            }
        }
    
        if (threads_sleeping)
        {
            thread_wake.notify_all();
        }  
    }
    else 
    {
        printf("queue empty\n");
    }

    return activeQueue.size();
}

int AssemblyLine::LaunchAsyncQueue()
{
    std::lock_guard<std::mutex> lock(mtx);

    if (!asyncBufferQueue.empty())
    {
        activeAsyncQueue.insert(
            activeAsyncQueue.end(),
            asyncBufferQueue.begin(), 
            asyncBufferQueue.end() 
        );
        
        asyncBufferQueue.clear();
    }

    if (!activeAsyncQueue.empty())
    {
        bool threads_sleeping = false;
        for (size_t i = 0; i < sleeping.size(); i++)
        {
            if (sleeping[i]) 
            {
                threads_sleeping = true;
                break;
            }
        }
    
        if (threads_sleeping)
        {
            thread_wake.notify_all();
        }
    }

    return activeAsyncQueue.size();
}

void AssemblyLine::WaitAll()
{
    std::unique_lock<std::mutex> lock(mtx);

    thread_is_async.wait(lock, [&] {
        for (size_t i = 0; i < is_async.size(); i++)
        {
            if (!is_async[i])
            {
                return false;
            }
        }

        if (activeQueue.empty() && activeAsyncQueue.empty())
        {
            bool threads_sleeping = true;
            for (size_t i = 0; i < sleeping.size(); i++)
            {
                if (!sleeping[i])
                {
                    threads_sleeping = false;
                    break;
                }
            }

            if (threads_sleeping)
            {
                return true;
            }
            return false;
        }
        return false;
    });
}

void AssemblyLine::StartTime()
{
    start_time = Clock::now();
}

long long AssemblyLine::EndTime()
{
    end_time = Clock::now();

    auto duration = end_time - start_time;

    return std::chrono::duration_cast<Micro>(duration).count();
}

void AssemblyLine::waitForWorkersToDie()
{
    std::unique_lock<std::mutex> lock(mtx);

    kill_threads = true;

    thread_wake.notify_all();

    thread_is_dead.wait(lock, [&] {
        for (size_t i = 0; i < dead.size(); i++)
        {
            if (!dead[i])
            {
                return false;
            }
        }

        return true;
    });
}

AssemblyLine::~AssemblyLine()
{
    waitForWorkersToDie();
    for (size_t i = 0; i < workers.size(); i++) {
        if (workers[i].joinable())
        {
            workers[i].join();
        }
    }
}