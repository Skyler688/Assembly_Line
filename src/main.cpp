#include "AssemblyLine.h"
#include <printf.h>
#include <string>
// #include <iostream>


void test(int threads)
{
    AssemblyLine line(threads);

    std::vector<Task> assemblyLine;
    
    // Simulated heavy functions of variable workloads.
    Task task_1 = [](std::any &data)
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
    
    Task task_2 = [](std::any &data)
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
    
    Task task_3 = [](std::any &data)
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
    
    Task task_4 = [](std::any &data)
    {
        // std::cout << std::this_thread::get_id() << std::endl;
        float test = 0.0;
        for (int i = 0; i < 100; i++) {
            test += 0.1;
        }
        
        float num = std::any_cast<float>(data);
        // printf("thread_%d: %f\n", id, num);

        data = test;
    };
    
    assemblyLine.push_back(task_4);
    
    int lineID = line.CreateAssemblyLine(assemblyLine);
    
    assemblyLine.clear();

    Task test_1 = [](std::any &data)
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

    Task test_2 = [](std::any &data)
    {
        std::vector<int> test;
        for (int i = 0; i < 20; i++)
        {
            test.push_back(i * 100);
        }

        int num = std::any_cast<int>(data);
        num *= 238;

        std::string result = "job done";
        data = result;
    };

    assemblyLine.push_back(test_2);

    int testID = line.CreateAssemblyLine(assemblyLine);
    
    assemblyLine.clear();

    std::string hello = "hello";

    for (int i = 0; i < 300000; i++)
    {
        // line.AddToAsyncBuffer(piplineID, NULL);
        line.AddToAsyncBuffer(lineID, i);
        // line.AddToAsyncBuffer(testID, hello);
    }

    while (true)
    {
        SyncResults sync_results;
        AsyncResults async_results;

        for (int i = 0; i < 5; i++)
        {
            line.AddToBuffer(lineID, i); 
        }
        for (size_t i = 0; i < 8; i++)
        {
            line.AddToBuffer(testID, hello);
        }
        
        line.LaunchQueue(sync_results); // this will wait until all threads are async and the sync queue is empty.
        int size = line.LaunchAsyncQueue(async_results); // this will add to the async queue but will wait to run it until the sync queue is done.

        
        printf("sync results size: %zu\n", sync_results[testID].size);
        printf("async results size: %zu\n", async_results[lineID].size);
        
        for (size_t i = 0; i < sync_results[lineID].size; i++)
        {
            float result = std::any_cast<float>(sync_results[lineID].data[i]);
            printf("Sync Result: %f\n", result);
        }
        for (size_t i = 0; i < sync_results[testID].size; i++)
        {
            std::string result = std::any_cast<std::string>(sync_results[testID].data[i]);
            printf("Sync Result: %s\n", result.c_str());
        }

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