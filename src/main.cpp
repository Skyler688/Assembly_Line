#include "Assembly_line.h"
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
    
    for (int i = 0; i < 300; i++)
    {
        line.AddToAsyncBuffer(lineID, i);
    }
    
    line.StartTime();
    for (int i = 0; i < 50; i++)
    {
        line.AddToBuffer(lineID, i); 
    }

    int sync = line.LaunchQueue();
    // printf("\nsync queue: %d\n\n", sync);

    while (true)
    {
        for (int i = 0; i < 50; i++)
        {
            line.AddToBuffer(lineID, i); 
        }
        
        int size = line.LaunchAsyncQueue(); // must wait for normal queue to finish before using async
        // printf("async queue: %d\n", size);
        
        
        int sync = line.LaunchQueue();
        // printf("\nsync queue: %d\n\n", sync);
        
        if (size == 0) {
            // printf("\n");
            break;
        }
    }
    long long microseconds = line.EndTime();

    printf("Job duration: %lld\n", microseconds);
    // printf("Done\n\n");
}

int main() 
{
    int hardware_threads = hardwareThreads();

    for (size_t i = 0; i < 30; i++)
    {
        test(22);
        // printf("\nThreads: %zu\n", i + 1);
        // for (size_t a = 0; a < 3; a++)
        // {
        // }
        // hardware_threads++;
    }

    return 0;
}