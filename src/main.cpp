#include "Assembly_line.h"
#include <printf.h>
#include <string>
// #include <iostream>

std::vector<Task> assemblyLine;

int main() 
{
    AssemblyLine line(25);

    // Simulated heavy functions of variable workloads.
    Task task_1 = [](std::any &data, int id)
    {
        // std::cout << std::this_thread::get_id() << std::endl;
        float test = 0.0;
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        test = 0.0;
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        test = 0.0;
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        test = 0.0;
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        test = 0.0;
        
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
        // for (int i = 0; i < 2000000; i++) {
        //     test += 0.1;
        // }
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
        for (int i = 0; i < 2000000; i++) {
            test += 0.1;
        }
        test = 0.0;
        // for (int i = 0; i < 2000000; i++) {
        //     test += 0.1;
        // }
        
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
        for (int i = 0; i < 10000; i++) {
            test += 0.1;
        }
        
        float num = std::any_cast<float>(data);
        // printf("thread_%d: %f\n", id, num);
    };

    assemblyLine.push_back(task_4);

    int lineID = line.CreateAssemblyLine(assemblyLine);

    assemblyLine.clear();

    for (int i = 0; i < 200; i++)
    {
        line.AddToBuffer(lineID, i);
    }

    line.LaunchQueue();
    
    // Wait() will block until the jobs are done.
    // NOTE -> This uses a condition variable and will keep the main thread idle until job is done. 
    // Also important to understand that you can do other things with the main thread before calling this like staging the async queue.
    // If the jobs finnish before this function is reached it will simply keep going without blocking, this is a safety messure to 
    line.Wait();

    printf("Jobs Done in %d milliseconds\n", line.GetSpeed());

    while (true) 
    {
        std::string command;
        std::cin >> command;

        if (command == "exit")
        {
            break;
        }
    }

    return 0;
}