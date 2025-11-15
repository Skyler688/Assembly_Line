#include "AssemblyLine.h"
#include <printf.h>
#include <string>
// #include <iostream>


void test(int threads)
{
    std::vector<Task> assemblyLine;
    AssemblyLine line(threads);
    
    // Simulated heavy functions of variable workloads.
    Task task_1 = [](std::any &data, int id)
    {
        // std::cout << std::this_thread::get_id() << std::endl;
        float test = 0.0;
        for (int i = 0; i < 200000; i++) {
            test += 0.1;
        }
        
        // NOTE -> The data variable can be any data type. it is important to cast it as the correct type.
        int num = std::any_cast<int>(data);
        // printf("thread_%d: %d\n", id, num);
    
        
        data = test;
    };
    
    assemblyLine.push_back(task_1);
    
    Task task_2 = [](std::any &data, int id)
    {
        // std::cout << std::this_thread::get_id() << std::endl;
        float test = 0.0;
        for (int i = 0; i < 200; i++) {
            test += 0.1;
        }
        // for (int i = 0; i < 2000000; i++) {
        //     test += 0.1;
        // }
        float num = std::any_cast<float>(data);
        // printf("thread_%d: %f\n", id, num);
    
    
        std::string message = "hello"; // Can change the data type as long as the next function in the line casts the new type correctly.
    
        data = message;
    };
    
    assemblyLine.push_back(task_2);
    
    Task task_3 = [](std::any &data, int id)
    {
        // std::cout << std::this_thread::get_id() << std::endl;
        float test = 0.0;
        for (int i = 0; i < 2000; i++) {
            test += 0.1;
        }
        
        std::string message = std::any_cast<std::string>(data);
        // printf("thread_%d: %s\n", id, message.c_str());
    
        float num = test;
    
        data = num;
    };
    
    assemblyLine.push_back(task_3);
    
    Task task_4 = [](std::any &data, int id)
    {
        // std::cout << std::this_thread::get_id() << std::endl;
        float test = 0.0;
        for (int i = 0; i < 100; i++) {
            test += 0.1;
        }
        
        float num = std::any_cast<float>(data);
        // printf("thread_%d: %f\n", id, num);
    };
    
    assemblyLine.push_back(task_4);
    
    int lineID = line.CreateAssemblyLine(assemblyLine);
    
    assemblyLine.clear();

    Task test_1 = [](std::any &data, int id)
    {
        float test = 0.0;
        for (int i = 0; i < 200; i++)
        {
            test += 0.1;
        }

        std::string hello = std::any_cast<std::string>(data);

        int num = 300;
        data = num + 2000;
    };

    assemblyLine.push_back(test_1);

    Task test_2 = [](std::any &data, int id)
    {
        std::vector<int> test;
        for (int i = 0; i < 20; i++)
        {
            test.push_back(i * 100);
        }

        int num = std::any_cast<int>(data);
        num *= 238;
    };

    assemblyLine.push_back(test_2);

    int testID = line.CreateAssemblyLine(assemblyLine);
    
    assemblyLine.clear();

    using DataChunk = std::vector<char>; 
    using Task = std::function<void(std::any&, int)>; 

    // GOOGLE GEMINIE GENERATED TEST
    // --- Pipeline Setup ---
    // NOTE: Use a large size (e.g., 4MB) to ensure cache impact
    const size_t LARGE_DATA_SIZE = 4 * 1024 * 1024; // 4 MB

    // Function 1: Simulate I/O Read (File Loading)
    Task simulate_read_stage = [](std::any &data, int id) {
        // 1. Simulate I/O Waiting Time (e.g., waiting for disk)
        std::this_thread::sleep_for(std::chrono::milliseconds(2)); 

        // 2. Allocate and initialize the large data chunk
        DataChunk buffer(LARGE_DATA_SIZE, 0);
        // Simulate reading data into the buffer
        buffer[0] = 'R'; 
        buffer[LARGE_DATA_SIZE - 1] = 'D';

        // 3. Pass the large data chunk to the next stage
        data = std::move(buffer); // Use std::move to avoid expensive copying

        // printf("Thread %d: Finished Read/Load (4MB)\n", id);
    };

    assemblyLine.push_back(simulate_read_stage);

    // Function 2: Simulate Complex Processing (Data Transformation)
    Task simulate_process_stage = [](std::any &data, int id) {
        // 1. Retrieve the large data chunk from the previous stage
        DataChunk buffer = std::any_cast<DataChunk>(std::move(data)); // Cast and move to claim ownership

        // 2. Simulate computationally intensive work on the data
        long long checksum = 0;
        for (size_t i = 0; i < buffer.size(); i += 1024) { // Iterate every 1024th byte
            checksum += buffer[i];
        }
        
        // Simulate CPU-intensive work (a few milliseconds)
        for (volatile int i = 0; i < 500000; i++); 

        // 3. Modify the buffer for the next stage (e.g., applying a filter)
        for (size_t i = 0; i < buffer.size(); ++i) {
            if (buffer[i] == 0) buffer[i] = 1;
        }

        // 4. Pass the modified large data chunk to the next stage
        data = std::move(buffer);

        // printf("Thread %d: Finished Processing (Checksum: %lld)\n", id, checksum);
    };

    assemblyLine.push_back(simulate_process_stage);

    // Function 3: Simulate I/O Write (File Save or Network Send)
    Task simulate_write_stage = [](std::any &data, int id) {
        // 1. Retrieve the large data chunk
        DataChunk buffer = std::any_cast<DataChunk>(std::move(data));
        
        // 2. Perform a final check or cleanup
        size_t final_size = buffer.size();

        // 3. Simulate I/O Waiting Time (e.g., waiting for network or disk write completion)
        std::this_thread::sleep_for(std::chrono::milliseconds(1)); 

        // 4. Result/Cleanup (data is consumed, not passed further)
        // The data chunk goes out of scope here, freeing the memory.

        // To pass a simple completion status to the next (non-existent) stage, we set a smaller type.
        data = final_size; 

        // printf("Thread %d: Finished Write/Save (Size: %zu)\n", id, final_size);
    };

    assemblyLine.push_back(simulate_write_stage);

    int piplineID = line.CreateAssemblyLine(assemblyLine);

    std::string hello = "hello";

    for (int i = 0; i < 300000; i++)
    {
        // line.AddToAsyncBuffer(piplineID, NULL);
        line.AddToAsyncBuffer(lineID, i);
        line.AddToAsyncBuffer(testID, hello);
    }

    while (true)
    {
        for (int i = 0; i < 50; i++)
        {
            line.AddToBuffer(lineID, i); 
            line.AddToBuffer(testID, hello);
        }
        
        line.LaunchQueue(); // this will wait until all threads are async and the sync queue is empty.
        int size = line.LaunchAsyncQueue(); // this will add to the async queue but will wait to run it until the sync queue is done.

        std::this_thread::sleep_for(std::chrono::milliseconds(8)); // allow the async a bit of time to run.
        
        // printf("size: %d\n", size);
        if (size == 0) {
            break;
        }

        // printf("Loop Done\n");
    }
    printf("Done Done\n");
}

int main() 
{
    test(18);

    return 0;
}