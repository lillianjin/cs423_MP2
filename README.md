# mp2 

## Implementation and Design Decisions
### 1) mp2_init  
Load the module and create proc directory and status file.  
This function requires to initialize proc file directory, slab cache, spinlock, and dispatching thread.  

### 2) mp2_exit  
Unload the module and remove proc directory and status file.  
This function requires to free all the memories used glabally or stop the process, including dispatching thread, task cache and file directory.  

### 3) mp2_register  
Create and insert task struct into the process list.  
This function requires to initialize each variables of the task struct and only register the task which pass the admission control check.  

### 4) mp2_yield  
Make the process yield for next process.  
This function requires to update timer, wake up the dispatching thread and make high-prioty task to sleep until next period starts.  

### 5) mp2_deregister  
Destroy the task struct from the process list.  
This function requires to remove the corresponding task from process task list and release memory of all data structures.  

### 6) mp2_write()  
Fetch task pid, period and process time from userapp.  
This function requires to divide the input into three different cases(R, D, Y).  

### 7) mp2_read()  
Read task pid, period and process time from /proc.  

### 8) dispatch_thread_function()  
Help to dispatch thread in the linked list and choose a highest prioty ready task to preempt the current executing task.  
This function requirese to find the highest prioty ready task first, and if there is a current running task, we need to change the corresponding states of both tasks and adjust their priority.  

### 9) admission_control()  
Sum up the total raio of process time divided by task period, then add the ratio of new task. If the (overall ratio * 1000) <= 693, and this new task passes the check and can be registered.  

### 10) userapp.c  
Read in the user command and do the whole process automatically. This app will print each step and execution time of doing the job.   

## Command to run the program
### 1) Compile the modules and user_app
```make modules```\
```make app```

### 2) Insert compiled module into the kernel
```sudo insmod lujin2_MP2.ko```

### 3) Run from user program
```./userapp [task_period] [task_process_time] [number_of_jobs] & ./userapp [task_period] [task_process_time] [number_of_jobs]```  
Mention that the [task_process] should be the same, as we compare the prioty of tasks by their periods. As a result, the input of [task_process] should remain the same. Also, the unit of both [task_process] and [task_process_time] are in ms.  

### 4) Remove the module  
```sudo rmmod lujin2_MP2```


### Sample Output
### 1) Sample Input  
```./userapp 9000 2000 3 & ./userapp 5000 2000 3 &```

### 2) Screenshot of the result
```c
Read task succeeded: pid is 1965, period is 9000, process_time is 2000, number of jobs is 3.
Read task succeeded: pid is 1966, period is 5000, process_time is 2000, number of jobs is 3.
Task 1966 registeration begins.
Task in /proc/mp2/status are listed: 
1966, 1, 5000, 2000
Task 1966 registration succeeded.
Task 1965 registeration begins.
Task in /proc/mp2/status are listed: 
1965, 1, 9000, 2000
1966, 1, 5000, 2000
Task 1965 registration succeeded.
Job 0 of task 1966 is running, start time:      1553587058.223311 s.
Job 0 of task 1966 finished, end time:          1553587060.223311 s.
Task 1966 yield begins...
Job 0 of task 1965 is running, start time:      1553587062.239308 s.
Job 1 of task 1966 is running, start time:      1553587063.215313 s.
Job 1 of task 1966 finished, end time:          1553587065.215315 s.
Task 1966 yield begins...
Job 0 of task 1965 finished, end time:          1553587065.887535 s.
Task 1965 yield begins...
Job 2 of task 1966 is running, start time:      1553587068.215315 s.
Job 2 of task 1966 finished, end time:          1553587070.215309 s.
Task 1966 yield begins...
Job 1 of task 1965 is running, start time:      1553587071.215366 s.
Job 1 of task 1965 finished, end time:          1553587073.215392 s.
Task 1965 yield begins...
Task in /proc/mp2/status are listed: 
1965, 1, 9000, 2000
Task 1966 unregistration succeeded.
Job 2 of task 1965 is running, start time:      1553587080.224030 s.
Job 2 of task 1965 finished, end time:          1553587082.224042 s.
Task 1965 yield begins...
Task in /proc/mp2/status are listed: 
Task 1965 unregistration succeeded.
```

