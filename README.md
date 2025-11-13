# Assembly Line

Assembly line is a performance maximizeing concerancy framework, using an underling multithreading engine that automaticaly handles the thread pool and maximising cpu time.

## TODO

- (DONE) Test the async queue.
- Create a data streaming pipeline reading from disk and simulating passing it to something. "testing the system with handling larger amounts of data in ram to see the the ideal thread pool changes do to more expensive context switches."
- Add a simulation feature that can run a simulation of the app trying diffrent thread amounts to find the ideal pool for the given machine.
- Add more advanced async features like job priority that will rearange the queue to prioritize one task. "could be good to add a priority to the async assemblyLines and organize the queue based on that." Or a deadline trigger that will throttle/wait in the case of a timeout or chosen event, allowing the queue to catch up if falling behind.
