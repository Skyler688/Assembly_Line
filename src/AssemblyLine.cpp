#include "AssemblyLine.h"

// Not a class function
int hardwareThreads()
{
    return std::thread::hardware_concurrency();
}

// ----------- Public ------------
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
        std::thread worker(&AssemblyLine::workerThread, this, i);
        workers.push_back(std::move(worker)); // moved into a list so they can be joined in the deconstructor.
    }
}

AssemblyLine::AssemblyLine(int threads)
{
    kill_threads = false;
    threads_async = 0;
    threads_sleeping = 0;
    threads_dead = 0;

    thread_count = threads;

    for (int i = 0; i < threads; i++)
    {
        std::thread worker(&AssemblyLine::workerThread, this, i);
        workers.push_back(std::move(worker));
    }
}

int AssemblyLine::CreateAssemblyLine(std::vector<Task> assembly_line)
{
    assembly_lines.push_back(assembly_line);
    return assembly_lines.size() - 1; // Return the assembly lines index
}

void AssemblyLine::AddToBuffer(int assembly_line_id, std::any data)
{
    Job job;
    job.data = data;
    job.line_id = assembly_line_id;
    job.task_index = 0;
    job.job_length = assembly_lines[assembly_line_id].size();

    sync_buffer.push_back(job); // add jobs to the back of the queue to fallow FIFO "first in first out"
}

void AssemblyLine::AddToAsyncBuffer(int assembly_line_id, std::any data)
{
    Job job;
    job.data = data;
    job.line_id = assembly_line_id;
    job.task_index = 0;
    job.job_length = assembly_lines[assembly_line_id].size();

    async_buffer.push_back(job);
}

void AssemblyLine::LaunchQueue() //TODO -> pass a return list by reference.
{    
    std::unique_lock<std::mutex> lock(mtx); 

    // .swap() is much faster than .insert() and can be done because the sync_queue is guaranteed to be empty upon calling this method.
    sync_queue.swap(sync_buffer);  

    // NOTE -> no need to clear the buffer as it has bean swapped with an empty deque.

    if (threads_sleeping == 1)
    {
        thread_wake.notify_one();
    }  
    else if (threads_sleeping <= thread_count)
    {
        thread_wake.notify_all();
    }

    // Wait for sync jobs to finish.
    thread_is_async.wait(lock, [&] {
        if (threads_async == thread_count && sync_queue.empty())
        {
            return true;
        }

        return false;
    });
}

int AssemblyLine::LaunchAsyncQueue() // TODO -> pass a return list by reference.
{
    std::lock_guard<std::mutex> lock(mtx);

    if (!async_buffer.empty())
    {
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
        if (threads_sleeping == 1)
        {
            thread_wake.notify_one();
        }
        else if (threads_sleeping <= thread_count)
        {
            thread_wake.notify_all();
        }
    }

    return async_queue.size();
}

// Deconstructor 
AssemblyLine::~AssemblyLine()
{
    waitForWorkersToDie();

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

    kill_threads = true;

    thread_wake.notify_all(); // incase any threads are sleeping

    // Waits for all threads to break ther wile loop so they can be joined.
    thread_is_dead.wait(lock, [&] { 
        if (threads_dead == thread_count)
        {
            return true;
        }

        return false;
    });
}

// This is the code that runs on each thread of the thead pool.

// IMPORTANT NOTES -> 
//  The threads use cooperative yielding to avoid any cpu idle time during transitions from sync and async.
//  The sync_queue has priority in this system and will run until empty before using the async_queue.
//  The passed id is used to identify individual threads in the flag lists.
void AssemblyLine::workerThread(int id)
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

        Task task = assembly_lines[job.line_id][job.task_index]; // Grabbing the actual function
        std::any job_data = job.data; // Grab the data to be passed into the function
        
        lock.unlock(); // Unlock the mutex. 

        // Run the grabbed function.
        // NOTE -> 
        //  job_data is passed by reference so no need to return anything. 
        //  This is more efficient in the event of larger peaces of data being passed.
        task(job_data, id);

        // If there is a next job in the assembly line add it to the front of the respective queue.
        if (job.job_length - 1 > job.task_index)
        {
            Job nextJob;
            nextJob.task_index = job.task_index + 1;
            nextJob.data = job_data; // Make sure to replace the passed data with new data.
            nextJob.line_id = job.line_id;
            nextJob.job_length = job.job_length;

            // Must lock the mutex again before accessing a queue
            lock.lock();

            // NOTE -> Adding the next job to the front of the queue to fallow FIFO "first in first out".
            if (!is_async) 
            {
                sync_queue.push_front(nextJob); 
            }
            else
            {
                async_queue.push_front(nextJob);
            }

            //NOTE -> 
            //  In some situations it is possible for the one of the queue's to become temporarily empty.
            //  If this occurs and the opposing queue is also empty a thread may get put to sleep to soon,
            //  leading to cpu idle time. From my testing this dose indead bring threads back in this event,
            //  preventing threads from being put to sleep to soon.
            thread_wake.notify_one(); 
            lock.unlock();
        } 
        else
        {
            // TODO -> add finish data to a list that can be accessed by the main thread. can be used for status report or returning processed data.
        }
    }

    std::lock_guard<std::mutex> lock(mtx);
    threads_dead++;
    thread_is_dead.notify_one();
}