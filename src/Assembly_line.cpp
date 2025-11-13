#include "Assembly_line.h"

// Private
void AssemblyLine::workerThread(int id)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mtx);

        bool local_async_flag;

        
        // Will wait "sleep" once the queue is empty.
        thread_wake.wait(lock, [&] {
            if (!activeQueue.empty())
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

        // Grab the task from the end of the list and remove it.
        Job job;

        if (!local_async_flag)
        {
            job = activeQueue.back();
            // printf("sync\n");
            activeQueue.pop_back();
        }
        else
        {
            job = activeAsyncQueue.back();
            // printf("async\n");
            activeAsyncQueue.pop_back();
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
                activeQueue.push_back(nextJob);
            }
            else
            {
                activeAsyncQueue.push_back(nextJob);
            }
            
            // Given the potential variation in tasks within each assembly line it could cause the queue to become temperarlly empty,
            // putting threads to sleep before all the work is done. By calling the notify_one() this will wake a sleeping thread if available preventing 
            // premature thread sleeping. This is most notable with tasks that have a very large discrepancy from the rest causing the queue to empty
            // near the end of the run before suddenly adding more work back in the queue.
            thread_wake.notify_one(); 
            lock.unlock();
        } 
    }
}

// Public
AssemblyLine::AssemblyLine()
{
    int numOfThreads = std::thread::hardware_concurrency();

    async_flag = true;
    
    for (int i = 0; i < numOfThreads - 1; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);

        sleeping.push_back(false);
        is_async.push_back(true);
        worker.detach();
    }
}

AssemblyLine::AssemblyLine(int threads)
{
    async_flag = true;

    for (int i = 0; i < threads; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);
        
        sleeping.push_back(false);
        is_async.push_back(true);
        worker.detach();
    }
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

    bufferQueue.push_back(job);
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

// void AssemblyLine::Wait()
// {
//     std::unique_lock<std::mutex> lock(mtx);

//     int threads = thread_alive.size();

//     jobs_done.wait(lock, [&] {
//         for (size_t i = 0; i < threads; i++)
//         {
//             if (!sleeping[i]) 
//             {
//                 return false;
//             }
//         }

//         if (activeQueue.size() == 0)
//         {
//             return true;
//         }
//         else
//         {
//             return false;
//         }
//     });

//     for (size_t i = 0; i < threads; i++)
//     {
//         thread_alive[i] = false;
//     }

//     endTime = Clock::now();
// }

// int AssemblyLine::Pause()
// {
//     std::unique_lock<std::mutex> lock(mtx);

//     int threads = thread_alive.size();

//     threads_alive.wait(lock, [&] {
//         for (size_t i = 0; i < threads; i++)
//         {
//             if (!thread_alive[i]) 
//             {
//                 return false;
//             }
//         }

//         return true;
//     });

//     pause_flag = true;
    
//     pause_done.wait(lock, [&] {
//         for (size_t i = 0; i < threads; i++)
//         {
//             if (!sleeping[i]) 
//             {
//                 return false;
//             }
//         }

//         return true;
//     });

//     // Save the async queue back to the buffer
//     asyncBufferQueue.insert(
//         asyncBufferQueue.end(),
//         activeQueue.begin(),
//         activeQueue.end()
//     );

//     // Clear the queue to stop all threads
//     activeQueue.clear();

//     for (size_t i = 0; i < threads; i++)
//     {
//         thread_alive[i] = false;
//     }

//     pause_flag = false;
//     return asyncBufferQueue.size();
// }

// int AssemblyLine::GetSpeed()
// {
//     Milli duration = endTime - startTime;

//     return duration.count();
// }

AssemblyLine::~AssemblyLine()
{
    
}