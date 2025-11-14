# Assembly Line

Assembly line is a performance maximizeing concerancy library, using an underling multithreading engine that automaticaly handles the thread pool and maximising cpu time.

## TODO

- (DONE) Test the async queue.
- Create a data streaming pipeline reading from disk and simulating passing it to something. "testing the system with handling larger amounts of data in ram to see the the ideal thread pool changes do to more expensive context switches."
- Add a return type line and a fully detached option so the assembly line is ether fully independent or needs to report the results back to main.
- Add more advanced async features like job priority that will rearange the queue to prioritize one task. "could be good to add a priority to the async assemblyLines and organize the queue based on that." Or a deadline trigger that will throttle/wait in the case of a timeout or chosen event, allowing the queue to catch up if falling behind.
- Add timing features to the lib like the ability to get blocking times, exicution times, and delay.

## Major Change

DONE -> added cooperative yielding with a priority to the sync queue, as well as a main thread block if all threads are not async and the sync queue is empty. This is to only add to the sync queue once all the jobs are done from last launch.

Get rid of the wait and pause and build in auto switching that will seamlessly switch between two active buffers preventing any threads from sleeping at all. This will need to be able to take a flag and go to the next buffer without sleeping. This will be challenging do to the temporary queue dropouts leading to early switches leaving work undone on the wait queue.
