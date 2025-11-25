# **AssemblyLine**

The AssemblyLine is a high performance, cooperative thread pool library built in C++. This library is highly optimized for building multi stage, concurrent capable pipelines, and it seamlessly supports both synchronous and asynchronous operations.

It uses an automated cooperative yielding system that will efficiently transfer worker threads between synchronous and asynchronous task queues, virtually eliminating CPU downtime. This system operates using First In, First Out (FIFO) job queues to ensure predictable execution order.

## **API and Workflow**

### _Creating a AssemblyLine instance and thread pool._

```cpp
#include "AssemblyLine.h"

int main()
{
    // This is a function independent of the AssemblyLine class.
    // It return's the amount of true concurrent capable threads the machine has.
    int hardware_threads = hardwareThreads();

    // This will create the assembly line instance and auto create a optimal thread pool.
    AssemblyLine assembly_line_instance;

    // Optionally you may pass the amount of threads you wish to create.
    // AssemblyLine assembly_line_instance(10);

}
```

### _Creating an assembly line._

```cpp
#include "AssemblyLine.h"

// IMPORTANT NOTE -> It is very crucial that any variables used in the task's are unique to each functions scope.
// Any data that needs to flow from function to function must pass through the (std::any &data) variable. This passed variable is protected
// from race conditions by the underlying engine, and insures the data is processed in the expected order.

// Creating the list that each function "task" of the assembly line will be placed.
Tasks assembly_line;

// Task functions must be void and pass a std::any type by reference.
void task_function(std::any &data)
{
    // The data type must be cast, please double check you are casting the correct type.
    // In this example we will assume the data was passed is an int.
    int num_from_cast = std::any_cast<int>(data);

    // Write your task's logic hear...

    // To pass data to the next task in the assembly line simply assign the new data to the passed std::any variable.
    data = std::string("Hello next task"); // NOTE -> can be any data type you want as long as it is cast correctly in the next function in the line.
}

// Create a task variable for that function.
Task task_1 = task_function;

// You can also just make a lambda function.
Task task_2 = [](std::any &data)
{
    // Last task's data passed is a std::string, so make sure to cast it as a std::string.
    std::string string_from_cast = std::any_cast<std::string>(data);

    // Functions code...

    // This will be the last function in are assembly line, the data will be what is returned back to the main thread.
    data = std::string("Hello to the main thread");
};

// Adding are tasks to the assembly line list.
assembly_line.push_back(task_1);
assembly_line.push_back(task_2);

// And finally lets add this new assembly line to are class.
int assembly_line_id = assembly_line_instance.CreateAssemblyLine(assembly_line);

// The above example showcases how the any data type is passed down the assembly line.
// However in real applications we are almost always going to be using much larger and more complex data types.
// In this case it is best to cast the data by mutable reference.
Task example_task = [](std::any &data)
{
    // Lets pretend we have a struct called LargeData and don't want to create any copy's of it to save on memory usage.
    // Casting the data by mutable reference
    LargeData &large_data = std::any_cast<LargeData&>(data);

    // Now we can modify and interact with the original data.
    large_data.example = "New data";

    // NOTE -> The original passed source is being modified so there is no need to assign the passed data a new value,
    // assuming the next function in the assembly line will be using the same data type.
}

// IMPORTANT NOTES ->
//      The passed assembly_line will become empty after creation, so no need to manually clear it if reusing it to create other assembly lines afterword.
//      This method returns a int that is the assembly lines index or "ID", this is how we will stage what assembly lines to add to the job queue's.
```

### _Adding jobs to the queue's_

```cpp
#include "AssemblyLine.h"

// There are two job queue's, a synchronous queue and a asynchronous queue.
// To add jobs to a queue we first stage everything into buffer's.

// Adding to the synchronous buffer
assembly_line_instance.AddToBuffer(assembly_line_id, int(100)) // passing the target assembly line's id and then the data to pass.

// This is only adding one job to the queue buffer however and we can add much more and with any assembly lines we want.
// Lets pretend we have two more assembly lines for this example, line_1 and line_2. We can add lets say 100 jobs to the buffer for
// line_1 and 20 jobs for line_2.
for (int i = 0; i < 100; i++)
{
    // just passing index as an example, in a real program you would likely be looping through a list of data to process.
    assembly_line_instance.AddToBuffer(line_1, int(i));
}

for (int i = 0: i < 20; i++)
{
    assembly_line_instance.AddToBuffer(line_2, int(i));
}

// IMPORTANT NOTE -> The buffers fallow FIFO "first in first out", so the first jobs added will be the first to execute.

// Adding to the asynchronous buffer.
assembly_line_instance.AddToAsyncBuffer(assembly_line_id, int(10));

// This is exactly the same to the synchronous buffer process.

// NOTES ->
//      Assembly lines are both sync and async capable and can be used interchangeably, this only effects wether the execution is blocking or not.
//      Buffers can not be modified, once you add jobs to them make sure to add them in the order you want them to execute.
```

### _Launching queue's._

```cpp
#include "AssemblyLine.h"

// Before we launch a queue we must first create are return lists. These can be used to return any data back from the end of are assembly lines.
SyncResults sync_results;
AsyncResults async_results;

// Launching the synchronous queue.
assembly_line_instance.LaunchQueue(sync_results);

// IMPORTANT NOTE -> The LaunchQueue() method is blocking and will wait until all of the synchronous jobs are done.

// Launching/updating the asynchronous queue. "Non blocking"
int queue_size = assembly_line_instance.LaunchAsyncQueue(async_results);

// IMPORTANT NOTES ->
//      The LaunchAsyncQueue() method uses active polling and will only add the finished jobs at that moment to the async_results.
//      This method also returns the current async queue size. This is useful for gauging how well the async is able to keep up with work demand.
//      Both the synchronous and asynchronous launch methods are designed to work together seamlessly, and can be called in any order you wish, or placed in a loop.
//      The underlying thread management engine uses a cooperative task yielding system and will prioritize the synchronous queue. This is done to eliminate cpu idle time.
//      When using the LaunchAsyncQueue() it will at minimum run one task on each thread before switching to synchronous, ensuring progress is maid even in very tight loops.
```

### _Extracting returned data_

```cpp
#include "AssemblyLine.h"

// To extract returned data we will simply use for loops.

// Extracting return data from a assembly line.
// NOTE -> This process is the same for both sync and async.
for (int i = 0; sync_results[assembly_line_id].length; i++)
{
    // Make sure to cast the returned data as the correct type, if you recall are last task set the resulting data to a std::string.
    std::string result = std::any_cast<std::string>(sync_results[assembly_line_id].data[i]);

    // NOTE -> Could ether pass the return data into a array/list, or just write any final logic in the for loop.
}

// IMPORTANT NOTES ->
//      The results lists will automatically be emptied next time you call the launch methods, so no need to worry about clearing old data.
//      Even if not returning data from a assembly line you must still pass the SyncResults and AsyncResults to the launch methods.
```

### _Task errors_

```cpp
#include "AssemblyLine.h"

// This library has a built in struct for reporting errors back to the main thread.

// This is how to create an error message inside of one of the tasks within an assembly line.
Task example_task = [](std::any &data)
{
    // Create an instance of the TaskError struct
    TaskError error;

    // Add your error message, it is of type std::string.
    error.message = "Error message hear"

    // And finally set the passed data to the error.
    data = error;
}

// IMPORTANT NOTES ->
//      The underling thread engine will place this error into the results returned back to the main thread.
//      It also will stop any task's afterwords in the assembly line from running.

// This is how you would check if an error accord in the results check.
for (int i = 0; i < sync_results[assembly_line_id].length; i++)
{
    // Check if the resulting data is an error.
    if (sync_results[assembly_line_id].data[i].type() == typeid(TaskError))
    {
        // Remember to cast the TaskError
        TaskError error = std::any_cast<TaskError>(sync_results[assembly_line_id].data[i]);

        // Grabbing the task index and message
        printf("TASK ERROR -> Assembly line: %d, Task index: %d, Message: %s", assembly_line_id, error.task_index, error.message.c_str());
    }
}
```

### _Logging back to the main thread_

In progress...

## **TODO**

- Refine the Makefile to auto build for cross platform. "Triple boot the macbook so you can test and develop cross platform apps"
- Seek out feedback from others on possible problems/improvements or useful features.
- Add documentation about concurrency overhead and methods that can be used to create ample task load. Things like Task Aggregation or batching, as well as instructions on how to test if the system is performing well or not. "In testing i noticed light tasks lead to very high os system overhead do to the frequency of thread lock's sleeps and wakes, heavy tasks perform better"
- ?Create a efficient log system that can report back info to the main thread in results. This will be more useful for development and debugging then print or cout when multithreading do to IO sync causing strange and poor performance.
