# mp2 

## Implementation and Design Decisions
1) mp2_init\
Load the module and create proc directory and status file. \
This function requires to initialize proc file directory, slab cache, spinlock, and dispatching thread.\\

2) mp2_exit\
Unload the module and remove proc directory and status file.\
This function requires to free all the memories used glabally or stop the process, including dispatching thread, task cache and file directory.\\

3) mp2_register\
Create and insert mp

## Command to run the program
### Basic Functions
1) Compile the modules and user_app\
```make modules```\
```make app```

2) Insert compiled module into the kernel\
```sudo insmod lujin2_MP2.ko```

3) Display the messages from the kernel ring buffer\
```dmesg```

4) Remove the module\
```sudo rmmod lujin2_MP2```


### Schedule Functions
1) Register the task to /proc/mp2/status
```echo "R, [pid], [task_period], [task_procss_time]" > /proc/mp2/status```

2) Yield task in /proc/mp2/status
```echo "Y, [pid]" > /proc/mp2/status```

3) Deregister from /proc/mp2/status
```echo "D, [pid]```

4) Run from user program
```./userapp [task_period] [task_procss_time] [number_of_jobs]```

### Sample Output
1) Output of running user program
![photo1](picture1.JPG)

2) Screenshot of ```cat /proc/mp2/status``` during the running process above
![photo1](picture2.JPG)
