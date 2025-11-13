#include "Assembly_line.h"

// Private
void AssemblyLine::workerThread(int id)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mtx);
        
        // Will wait "sleep" once the queue is empty.
        thread_wake.wait(lock, [&] {
            thread_alive[id] = true;
            threads_alive.notify_one();

            // The pause flag is for the async pausing.
            if (!pause_flag) 
            {
                if (activeQueue.empty())
                {
                    if (sleeping[id] == false)
                    {
                        // printf("SLEPT: %d\n", id);
                    }
                    sleeping[id] = true;
                    jobs_done.notify_one(); // notify the Wait() if the queue is empty.
                    return false;
                } 
                else
                {
                    if (sleeping[id] == true)
                    {
                        // printf("AWAKE: %d\n", id);
                    }
                    sleeping[id] = false;
                    return true;
                } 
            }
            else
            {
                sleeping[id] = true;
                // printf("paused: %d\n", id);
                pause_done.notify_one();
                return false;
            }
        });

        // Grab the task from the end of the list and remove it.
        Job job = activeQueue.back();
        Task task = assemblyLines[job.lineID][job.taskIndex];
        std::any jobData = job.data;
        activeQueue.pop_back();

        // Unlock the mutex so other threads can have access. 
        lock.unlock();
 
        // Run the passed function.
        task(jobData, id); // passed by reference so i can replace the jobData with data for the next function.

        // if there is a next job in the line add it to queue.
        if (assemblyLines[job.lineID].size() - 1 > job.taskIndex)
        {
            Job nextJob;
            nextJob.taskIndex = job.taskIndex + 1;
            nextJob.data = jobData; // Make sure to replace the passed data with new data.
            nextJob.lineID = job.lineID;

            // lock again
            lock.lock();
            activeQueue.push_back(nextJob);
            
            // Given the potential variation in tasks within each assembly line it could cause the queue to become temperarlly empty,
            // putting threads to sleep before all the work is done. By calling the notify_one() this will wake a sleeping thread if available preventing 
            // premature thread sleeping. This is most notable with tasks that have a very large discrepancy from the rest causing the queue to empty
            // near the end of the run before suddenly adding more work back in the queue.

            if (!pause_flag) // if pausing want to avoid waking any threads.
            {
                thread_wake.notify_one(); 
            }
            lock.unlock();
        } 
    }
}

// Public
AssemblyLine::AssemblyLine()
{
    int numOfThreads = std::thread::hardware_concurrency();

    pause_flag = false;
    
    for (int i = 0; i < numOfThreads - 1; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);

        sleeping.push_back(true);
        thread_alive.push_back(false);
        worker.detach();
    }
}

AssemblyLine::AssemblyLine(int threads)
{
    pause_flag = false;

    for (int i = 0; i < threads; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);
        
        sleeping.push_back(true);
        thread_alive.push_back(false);
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

    bufferQueue.push_back(job);
}

void AssemblyLine::AddToAsyncBuffer(int assemblyLineID, std::any data)
{
    Job job;
    job.data = data;
    job.lineID = assemblyLineID; // NOTE -> Can just use the same ID list for the async functions as well.
    job.taskIndex = 0;

    asyncBufferQueue.push_back(job);
}

void AssemblyLine::LaunchQueue()
{
    std::lock_guard<std::mutex> lock(mtx); // Is a lock tied to the functions scope, cannot be manually locked and unlocked.
    
    activeQueue.insert(
        activeQueue.end(),
        bufferQueue.begin(),
        bufferQueue.end()
    );

    bufferQueue.clear(); // Clear the buffer once transferred to the active queue.
    
    startTime = Clock::now();
    thread_wake.notify_all();
}

void AssemblyLine::LaunchAsyncQueue()
{
    std::lock_guard<std::mutex> lock(mtx);

    activeQueue.insert(
        activeQueue.end(),
        asyncBufferQueue.begin(),
        asyncBufferQueue.end()
    );

    asyncBufferQueue.clear();

    thread_wake.notify_all();
}

void AssemblyLine::Wait()
{
    std::unique_lock<std::mutex> lock(mtx);

    int threads = thread_alive.size();

    jobs_done.wait(lock, [&] {
        for (size_t i = 0; i < threads; i++)
        {
            if (!sleeping[i]) 
            {
                return false;
            }
        }

        if (activeQueue.size() == 0)
        {
            return true;
        }
        else
        {
            return false;
        }
    });

    for (size_t i = 0; i < threads; i++)
    {
        thread_alive[i] = false;
    }

    endTime = Clock::now();
}

int AssemblyLine::Pause()
{
    std::unique_lock<std::mutex> lock(mtx);

    int threads = thread_alive.size();

    threads_alive.wait(lock, [&] {
        for (size_t i = 0; i < threads; i++)
        {
            if (!thread_alive[i]) 
            {
                return false;
            }
        }

        return true;
    });

    pause_flag = true;
    
    pause_done.wait(lock, [&] {
        for (size_t i = 0; i < threads; i++)
        {
            if (!sleeping[i]) 
            {
                return false;
            }
        }

        return true;
    });

    // Save the async queue back to the buffer
    asyncBufferQueue.insert(
        asyncBufferQueue.end(),
        activeQueue.begin(),
        activeQueue.end()
    );

    // Clear the queue to stop all threads
    activeQueue.clear();

    for (size_t i = 0; i < threads; i++)
    {
        thread_alive[i] = false;
    }

    pause_flag = false;
    return asyncBufferQueue.size();
}

int AssemblyLine::GetSpeed()
{
    Milli duration = endTime - startTime;

    return duration.count();
}

AssemblyLine::~AssemblyLine()
{
    
}