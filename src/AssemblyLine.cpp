#include "AssemblyLine.h"

// Not a class function ment to be accessible before the class is created to grab the number of hardware threads.
int hardwareThreads()
{
    return std::thread::hardware_concurrency();
}

// ----------- Helpers -----------
void AssemblyLine::wakeSleepingThreads()
{
    if (threads_sleeping == 1)
    {
        thread_wake.notify_one();
    }  
    else if (threads_sleeping <= thread_count)
    {
        thread_wake.notify_all();
    }
}

// ----------- Public ------------

// Default constructor
AssemblyLine::AssemblyLine()
{
    int num_of_threads = std::thread::hardware_concurrency();

    kill_threads = false;
    threads_async = 0;
    threads_sleeping = 0;
    threads_dead = 0;

    thread_count = num_of_threads + 2;
    
    // TESTING NOTE ->
    //  From the testing i have done i found that creating a thread pool of 2+ the number of hardware threads results in the best average execution speed.
    //  I believe this is do to striking a balance between keeping cores busy and minimizing context switching. More threads tended to slowly reduce execution speeds.
    //  This may not be the case on some machines and will require further testing to gather data.
    for (int i = 0; i < thread_count; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this);
        workers.push_back(std::move(worker)); // moved into a list so they can be joined in the deconstructor.
    }
}

// Manual constructor
AssemblyLine::AssemblyLine(int threads)
{
    kill_threads = false;
    threads_async = 0;
    threads_sleeping = 0;
    threads_dead = 0;

    thread_count = threads;

    for (int i = 0; i < threads; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this);
        workers.push_back(std::move(worker));
    }
}

int AssemblyLine::CreateAssemblyLine(std::vector<Task> &assembly_line)
{
    std::lock_guard<std::mutex> lock(mtx); // locking just incase user adds assembly lines after queue launch.

    Result result;
    sync_results.push_back(result);
    async_results.push_back(result);
    assembly_lines.push_back(assembly_line);
    assembly_line_count++;
    Tasks empty;
    assembly_line.swap(empty);
    return assembly_lines.size() - 1; // Return the assembly lines index "ID"
}

// NOTE -> No locks are needed for adding to buffers.
void AssemblyLine::AddToBuffer(int assembly_line_id, const std::any &data)
{
    Job job;
    job.data = data;
    job.line_id = assembly_line_id;
    job.task_index = 0;
    job.job_length = assembly_lines[assembly_line_id].size();

    sync_buffer.push_back(job); // add jobs to the back of the queue fallowing FIFO "first in first out"
}

void AssemblyLine::AddToAsyncBuffer(int assembly_line_id, const std::any &data)
{
    Job job;
    job.data = data;
    job.line_id = assembly_line_id;
    job.task_index = 0;
    job.job_length = assembly_lines[assembly_line_id].size();

    async_buffer.push_back(job);
}

void AssemblyLine::LaunchQueue(SyncResults &results)
{    
    // If the passed results are not empty go ahead and empty it.
    if (!results.empty())
    {
        SyncResults().swap(results); // using swap() for efficiency over clear().
    }

    // Creating a list of defaults for each assembly line before the lock to avoid wasting mutex lock time.
    // The passed results is swapped with the actual results, needs to have a reset default for each assembly line. 
    for (size_t i = 0; i < assembly_line_count; i++) {
        Result default_result;
        results.push_back(default_result);
    }

    std::unique_lock<std::mutex> lock(mtx); 

    // .swap() is much faster than .insert() and can be done because the sync_queue is guaranteed to be empty upon calling this method.
    sync_queue.swap(sync_buffer);  

    // NOTE -> no need to clear the buffer as it has bean swapped with an empty deque.

    wakeSleepingThreads();

    // Wait for sync jobs to finish.
    thread_is_async.wait(lock, [&] {
        if (threads_async == thread_count && sync_queue.empty())
        {
            return true;
        }

        return false;
    });

    // After waiting update the results.
    results.swap(sync_results);  
}

int AssemblyLine::LaunchAsyncQueue(AsyncResults &results)
{
    if (!results.empty())
    {
        AsyncResults().swap(results);
    }

    for (size_t i = 0; i < assembly_line_count; i++) {
        Result default_result;
        results.push_back(default_result);
    }

    std::lock_guard<std::mutex> lock(mtx);

    if (!async_buffer.empty())
    {
        // Cannot use swap() for the async buffer as new jobs may be added before the queue is empty.
        async_queue.insert(
            async_queue.end(),
            std::make_move_iterator(async_buffer.begin()), // make_move_iterator() makes the insert much more efficient.
            std::make_move_iterator(async_buffer.end()) 
        );
        
        std::deque<Job> empty; // Creating a empty deque 
        async_buffer.swap(empty); // Using swap() instead of clear() because it is more efficient.
    }

    if (!async_queue.empty())
    {
        wakeSleepingThreads();
    }

    results.swap(async_results);

    return async_queue.size();
}

// Deconstructor 
AssemblyLine::~AssemblyLine()
{
    waitForWorkersToDie();

    // Join threads
    for (size_t i = 0; i < thread_count; i++) {
        if (workers[i].joinable())
        {
            workers[i].join();
        }
    }
}

// ------------ Private ------------
void AssemblyLine::waitForWorkersToDie()
{
    std::unique_lock<std::mutex> lock(mtx);

    // Flag used to break the worker threads wile loop so they can be joined.
    kill_threads = true;

    wakeSleepingThreads();

    // Waits for all threads to break ther wile loop.
    thread_is_dead.wait(lock, [&] { 
        if (threads_dead == thread_count)
        {
            return true;
        }

        return false;
    });
}

// -------------- WORKER THREAD CODE --------------

// IMPORTANT NOTES -> 
//  The threads use cooperative yielding to avoid any cpu idle time during transitions from sync and async.
//  The sync_queue has priority in this system and will run until empty before using the async_queue.
//  Int flags are used to share the state of each worker thread to the main thread.
void AssemblyLine::workerThread()
{
    bool is_async = false;
    bool sleeping = false;

    while (true)
    {
        std::unique_lock<std::mutex> lock(mtx);

        // Will wait "sleep" once both queue's are empty.
        thread_wake.wait(lock, [&] {
            // kill_threads is used to break the while loop so the thread can be joined in the deconstruction of the class.
            if (kill_threads)
            {
                return true;
            }
            else if (!sync_queue.empty())
            { 
                if (is_async)
                {
                    is_async = false;
                    threads_async--;
                }

                if (sleeping)
                {
                    sleeping = false;
                    threads_sleeping--;
                }

                return true;
            }
            else 
            {
                if (!is_async)
                {
                    is_async = true;
                    threads_async++;

                    // Used to notify the wait in the LaunchQueue() to make it block until all sync jobs are done.
                    thread_is_async.notify_one(); 
                }

                if (async_queue.empty())
                {
                    if (!sleeping)
                    {
                        sleeping = true;
                        threads_sleeping++;
                    }
                    return false;
                }
                else 
                {
                    if (sleeping)
                    {
                        sleeping = false;
                        threads_sleeping--;
                    }
                    return true;
                }
            }
        });

        // Break the while loop so thread can be killed.
        if (kill_threads)
        {
            break;
        }

        // Grab the task from the end of the respective queue and remove it.
        Job job;

        if (!is_async)
        {
            job = sync_queue.front();
            sync_queue.pop_front();
        }
        else
        {
            job = async_queue.front();
            async_queue.pop_front();
        }

        Task task = assembly_lines[job.line_id][job.task_index]; // Grabbing the actual function from the assembly_lines.
        
        lock.unlock(); // Unlock the mutex. 

        task(job.data);

        // NOTE -> 
        //  job.data is passed by reference so no need to return anything. 
        //  This is more efficient in the event of larger peaces of data being passed.

        // Check if an error was passed.
        if (job.data.type() == typeid(TaskError))
        {
            // If an error is reported in any task cast a mutable reference.
            TaskError &error = std::any_cast<TaskError&>(job.data);
            error.task_index = job.task_index;

            lock.lock();

            if (!is_async)
            {
                sync_results[job.line_id].data.push_back(job.data); // NOTE -> push_back() will create its own copy of the passed data so you can pass a reference.
                sync_results[job.line_id].length++;
            }
            else
            {
                async_results[job.line_id].data.push_back(job.data);
                async_results[job.line_id].length++;
            }

            // NOTE -> In the event of a error no next job is posted into the queue.
        } 
        else
        {
            // If there is a next job in the assembly line add it to the front of the respective queue.
            if (job.job_length - 1 > job.task_index)
            {
                // NOTE -> the job.data has already bean modified by the task function, and both the line_id and job length remain the same.
                job.task_index++;
    
                // Must lock the mutex again before accessing a queue
                lock.lock();
    
                // NOTE -> Adding the next job to the front of the queue to fallow FIFO "first in first out".
                if (!is_async) 
                {
                    sync_queue.push_front(job); 
                }
                else
                {
                    async_queue.push_front(job);
                }
    
                //NOTE -> 
                //  In some situations it is possible for the one of the queue's to become temporarily empty.
                //  If this occurs and the opposing queue is also empty a thread may get put to sleep to soon,
                //  leading to cpu idle time. From my testing this dose indead bring threads back in this event,
                //  preventing threads from being put to sleep to soon.
                thread_wake.notify_one(); 
            } 
            else
            {
                // If there is not next job copy over the resulting data into the results.
                lock.lock();
    
                if (!is_async)
                {
                    sync_results[job.line_id].data.push_back(job.data);
                    sync_results[job.line_id].length++;
                }
                else
                {
                    async_results[job.line_id].data.push_back(job.data);
                    async_results[job.line_id].length++;
                }
            }
        }
        lock.unlock();

    } // End of the while loop.

    std::lock_guard<std::mutex> lock(mtx);
    threads_dead++;
    thread_is_dead.notify_one();
}