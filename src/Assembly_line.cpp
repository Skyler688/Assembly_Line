#include "Assembly_line.h"

// Private
void AssemblyLine::workerThread(int id)
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(mtx);
        // Will wait "sleep" once the queue is empty.
        thread_wake.wait(lock, [&] {
            if (activeQueue.empty())
            {
                if (sleeping[id] == false)
                {
                    // printf("SLEPT: %d\n", id);
                    sleeping[id] = true;
                }
                return false;
            } else {
                if (sleeping[id] == true)
                {
                    // printf("AWAKE: %d\n", id);
                    sleeping[id] = false;
                }
                return true;
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
            lock.unlock();

            // Given the potential variation in tasks within each assembly line it could cause the queue to become temperarlly empty,
            // putting threads to sleep before all the work is done. By calling the notify_one() this will wake a sleeping thread if available preventing 
            // premature thread sleeping. This is most notable with tasks that have a very large discrepancy from the rest causing the queue to empty
            // near the end of the run before suddenly adding more work back in the queue.
            thread_wake.notify_one(); 
        } else {
            jobs_done.notify_one();
        }
    }
}

// Public
AssemblyLine::AssemblyLine()
{
    int numOfThreads = std::thread::hardware_concurrency();
    
    for (int i = 0; i < numOfThreads - 1; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);
        worker.detach();

        sleeping.push_back(true);
    }
}

AssemblyLine::AssemblyLine(int threads)
{
    for (int i = 0; i < threads; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);
        worker.detach();

        sleeping.push_back(true);
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

    jobs_done.wait(lock, [&] {
        bool all_sleeping = true;
        for (size_t i = 0; i < sleeping.size(); i++)
        {
            if (sleeping[i] == false) 
            {
                all_sleeping = false;
            }
        }

        if (all_sleeping && activeQueue.size() == 0)
        {
            endTime = Clock::now();
            return true;
        } else {
            return false;
        }
    });
}

void AssemblyLine::Pause()
{
    std::lock_guard<std::mutex> lock(mtx);

    // Save the async queue back to the buffer
    asyncBufferQueue.insert(
        asyncBufferQueue.end(),
        activeQueue.begin(),
        activeQueue.end()
    );

    // Clear the queue to stop all threads
    activeQueue.clear();
}

int AssemblyLine::GetSpeed()
{
    Milli duration = endTime - startTime;

    return duration.count();
}

AssemblyLine::~AssemblyLine()
{
    
}