#define LINUX
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/proc_fs.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include "mp2_given.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP2");

#define DEBUG 1
#define DIRECTORY "mp2"
#define FILENAME "status"
#define READY 0
#define SLEEPING 1
#define RUNNING 2
#define REGISTRATION 'R'
#define YIELD 'Y'
#define DEREGISTRATION 'D'

/*
Define a structure to represent Process Control Block
*/
typedef struct mp2_task_struct {
    struct task_struct *task;
    struct list_head task_node;
    struct timer_list wakeup_timer;

    unsigned int pid;
    unsigned int task_period; // the priority in RMS
    unsigned int next_period; 
    unsigned int task_state;
    unsigned long process_time;
} mp2_task_struct;

// a kernel thread that is responsible for triggering the context switches as needed
static struct task_struct *dispatch_thread;
// current task
static mp2_task_struct *cur_task = NULL;
// Declare proc filesystem entry
static struct proc_dir_entry *proc_dir, *proc_entry;
// Declare mutex lock
// struct mutex mutexLock;
// Declare a slab cache
static struct kmem_cache *mp2_cache;
// Create a list head
LIST_HEAD(my_head);
// Define a spin lock
static DEFINE_SPINLOCK(sp_lock);

/*
Find task struct by pid
*/
mp2_task_struct* find_mptask_by_pid(unsigned long pid)
{
    mp2_task_struct* task;
    list_for_each_entry(task, &my_head, task_node) {
        if(task->pid == pid){
            return task;
        }
    }
    return NULL;
}

/*
Time handler function
t: user defined data
*/
 static void my_timer_function(unsigned long pid) {
    unsigned long flags; 
    printk(KERN_ALERT "TIMER RUNNING, pid is %u\n", pid);
    spin_lock_irqsave(&sp_lock, flags);
    mp2_task_struct *tsk = find_mptask_by_pid(pid);
    if(tsk == NULL){
        printk(KERN_ALERT "TASK IS NULL\n");
    }
    if(tsk->task_state == SLEEPING){
        tsk->task_state = READY;
    }
    wake_up_process(dispatch_thread);
    spin_unlock_irqrestore(&sp_lock, flags);
 }


/*
This function is used to check the scheduling accuracy whenever a new task comes in.
*/
static bool my_admission_control(mp2_task_struct *new){
    unsigned long tot_ratio = 0;
    mp2_task_struct *temp;
    unsigned long flags; 

    // compute the existing ratio sum
    spin_lock_irqsave(&sp_lock, flags);
    list_for_each_entry(temp, &my_head, task_node){
        tot_ratio += 1000 * temp->process_time / temp->task_period;
    }
    spin_unlock_irqrestore(&sp_lock, flags);
    // add new ratio
    tot_ratio += 1000 * new->process_time / new->task_period;
    printk(KERN_ALERT "RATIO IS %lu\n", tot_ratio);
    if(tot_ratio <= 693){
        printk(KERN_ALERT "MODULE %u PASSES ADMISSION CONTROL\n", new->pid);
        return true;
    }else{
        printk(KERN_ALERT "MODULE %u DOES NOT PASS ADMISSION CONTROL\n", new->pid);
        return false;
    }
}


/*
This function allows the application to notify to our kernel module its intent to use the RMS scheduler.
*/
static void mp2_register(unsigned int pid, unsigned int period, unsigned long process_time) {
    #ifdef DEBUG
    printk(KERN_ALERT "REGISTRATION MODULE LOADING\n");
    #endif
    // initiate and allocate, use slab allocator
    mp2_task_struct *curr_task = (mp2_task_struct *) kmem_cache_alloc(mp2_cache, GFP_KERNEL);
    curr_task->task = find_task_by_pid(pid);
    curr_task->pid = pid;
    curr_task->task_period = period;
    curr_task->task_state = SLEEPING;
    curr_task->process_time = process_time;
    
    // Setup the wakeup timer function
    setup_timer(&(curr_task->wakeup_timer), my_timer_function, (unsigned long)curr_task->pid);
    
    // check for admission_control
    if(!my_admission_control(curr_task)){
        printk(KERN_ALERT "ADMISSION RETURN\n");
        kmem_cache_free(mp2_cache, curr_task);
        return;
    }

    // add the task to task list
    unsigned long flags; 
    spin_lock_irqsave(&sp_lock, flags);
    list_add(&(curr_task->task_node), &my_head);
    spin_unlock_irqrestore(&sp_lock, flags);
    printk(KERN_ALERT "REGISTRATION MODULE LOADED\n");
}


/*
This function allows the application to notify the RMS scheduler that the application has finished using the RMS scheduler
*/
static void mp2_deregister(unsigned int pid) {
    #ifdef DEBUG
    printk(KERN_ALERT "DEREGISTRATION MODULE LOADING\n");
    #endif
    mp2_task_struct *stop;
    unsigned long flags; 
    spin_lock_irqsave(&sp_lock, flags);

    stop = find_mptask_by_pid(pid);
    stop->task_state = SLEEPING;
    del_timer(&(stop->wakeup_timer));
    list_del(&(stop->task_node));
    kmem_cache_free(mp2_cache, stop);

    if(cur_task == stop){
        cur_task = NULL;
        wake_up_process(dispatch_thread);
    }

    spin_unlock_irqrestore(&sp_lock, flags);
    printk(KERN_ALERT "DEREGISTRATION MODULE LOADED\n");

}
 

/*
This function is used to find the highest priority task
*/
static mp2_task_struct* find_highest_prioty_tsk(void){
    unsigned long flags; 
    mp2_task_struct *curr, *highest;
    unsigned long min_period = INT_MAX;
    spin_lock_irqsave(&sp_lock, flags);
    list_for_each_entry(curr, &my_head, task_node) {
        if(highest == NULL || curr->task_period < min_period){
            highest = curr;
            min_period = curr->task_period;
        }
    }
    spin_unlock_irqrestore(&sp_lock, flags);
    return highest;
}

/*
This function dispatches thread to switch next highest priority ready task
*/
static int dispatch_thread_function(void){
    mp2_task_struct *tsk;
    unsigned long flags; 
    struct sched_param sparam;

    while(1){
        // put dispatching thread to sleep
        set_current_state(TASK_INTERRUPTIBLE);
        schedule();
        // check if the kthread can return
        if(kthread_should_stop()){
            printk(KERN_ALERT "DISPATCHING THREAD FINISHED WORKING");
            return 0;
        }
        spin_lock_irqsave(&sp_lock, flags);
        printk(KERN_ALERT "DISPATCHING THREAD STARTS WORKING");
        tsk = find_highest_prioty_tsk();
        // current task has higher pirority/ lower period
        if(tsk != NULL){
            if(cur_task != NULL && cur_task->task_period > tsk->task_period){
                cur_task->task_state = READY;
                sparam.sched_priority = 0;
                sched_setscheduler(tsk->task, SCHED_NORMAL, 0);
                // let the higher piority task to run
                tsk->task_state = RUNNING;
                wake_up_process(tsk->task);
                sched_setscheduler(tsk->task, SCHED_FIFO, 99);
                cur_task = tsk;
            }
        }else{
            //if no task is ready
            if(cur_task != NULL){
                sparam.sched_priority = 0;
                sched_setscheduler(tsk->task, SCHED_NORMAL, 0);
            }
        }
        spin_unlock_irqrestore(&sp_lock, flags);
    }
    return 0;
}


/*
This function notifies the RMS scheduler that the application has finished its period.
*/
static void mp2_yield(unsigned int pid) {
    #ifdef DEBUG
    printk(KERN_ALERT "YIELD MODULE LOADING\n");
    #endif
    mp2_task_struct *tsk = find_mptask_by_pid(pid);
    unsigned long flags; 
    spin_lock_irqsave(&sp_lock, flags);
    if(tsk != NULL && tsk->task != NULL){
        printk(KERN_ALERT "tsk->next_period=%u, jiffies is %u\n", tsk->next_period, jiffies);
        // if first time yield
        if(tsk->next_period==0){
            tsk->next_period += jiffies + msecs_to_jiffies(tsk->task_period);
        }else{
            tsk->next_period += msecs_to_jiffies(tsk->task_period);
        }
        printk(KERN_ALERT "tsk->next_period=%u\n", tsk->next_period);
        // if next period has not start
        if(tsk->next_period >= jiffies){
            tsk->task_state = SLEEPING;
            mod_timer(&(tsk->wakeup_timer), tsk->next_period);
            set_task_state(tsk->task, TASK_UNINTERRUPTIBLE);
            cur_task = NULL;
            wake_up_process(dispatch_thread);
            schedule();
        }
    }
    spin_unlock_irqrestore(&sp_lock, flags);
    printk(KERN_ALERT "YIELD MODULE LOADED\n");
}



/*
This function is called then the /proc file is read
filp: file pointer
buf: points to the user buffer holding the data to be written or the empty buffer
count: the size of the requested data transfer
offp: a pointer to a “long offset type” object that indicates the file position the user is accessing
*/
ssize_t mp2_read (struct file *filp, char __user *buf, size_t count, loff_t *offp)
{
    int copied = 0;
    mp2_task_struct *curr;
    char *buffer;
    unsigned long flags; 

    // if the file is already read
    if(*offp > 0){
        return 0;
    }

    //allocate memory in buffer and set the content in buffer as 0
    buffer = (char *)kmalloc(4096, GFP_KERNEL);
    memset(buffer, 0, 4096);

    // Acquire the mutex
    spin_lock_irqsave(&sp_lock, flags);

    // record the location of current node inside "copied" after each entry
    list_for_each_entry(curr, &my_head, task_node){
        copied += sprintf(buffer + copied, "%u, %u, %u, %lu\n", curr->pid, curr->task_state, curr->task_period, curr->process_time);
        printk(KERN_ALERT "I AM READING: %s, %u, %u, %u, %lu\n", buffer, curr->pid, curr->task_state, curr-> task_period, curr->process_time);
    }

    // if the message length is larger than buffer size
    if (copied > count){
        kfree(buffer);
        return -EFAULT;
    }
    spin_unlock_irqrestore(&sp_lock, flags);

    // set the end of string character with value 0 (NULL)
    buffer[copied] = '\0';
    copied += 1;
    //Copy a block of data into user space.
    copy_to_user(buf, buffer, copied);

    // Free previously allocated memory
    kfree(buffer);

    *offp += copied;
    return copied;
}


/*
his function is called with the /proc file is written
filp: file pointer
buf: points to the user buffer holding the data to be written or the empty buffer
count: the size of the requested data transfer
offp: a pointer to a “long offset type” object that indicates the file position the user is accessing
*/
ssize_t mp2_write (struct file *filp, const char __user *buf, size_t count, loff_t *offp)
{
    char *buffer = (char *)kmalloc(4096, GFP_KERNEL);
    // mp2_task_struct *new;
    unsigned int pid;
    unsigned int period;
    unsigned long process_time;
    unsigned long flags; 

    memset(buffer, 0, 4096);
    // if the lengh of message is larger than buffer size or the file is already written, return
    if (count > 4096 || *offp>0) {
        kfree(buffer);
        return -EFAULT;
    }

    copy_from_user(buffer, buf, count);
    buffer[count] = '\0';

    if(count > 0){
        switch (buffer[0])
        {
            case REGISTRATION:
                sscanf(buffer + 3, "%u, %u, %lu\n", &pid, &period, &process_time);
                mp2_register(pid, period, process_time);
                break;
            case YIELD:
                sscanf(buffer + 3, "%u\n", &pid);
                mp2_yield(pid);
                break;
            case DEREGISTRATION:
                sscanf(buffer + 3, "%u\n", &pid);
                mp2_deregister(pid);
                break;
            default:
                kfree(buffer);
                return 0;
        }
    }

    // printk(KERN_ALERT "I AM WRITING: %s, parse results: pid %u, period %u, process_time %lu\n", buffer, pid, period, process_time);

    kfree(buffer);
    return count;
}

// Declare the file operations
static const struct file_operations file_fops = {
    .owner = THIS_MODULE,
    .write = mp2_write,
    .read  = mp2_read,
};


// mp2_init - Called when module is loaded
int __init mp2_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE LOADING\n");
   #endif
   // Insert your code here ...

   //Create proc directory "/proc/mp2/" using proc_dir(dir, parent)
   proc_dir = proc_mkdir(DIRECTORY, NULL);

   //Create file entry "/proc/mp2/status/" using proc_create(name, mode, parent, proc_fops)
   proc_entry = proc_create(FILENAME, 0666, proc_dir, &file_fops);

   // init a new cache of size sizeof(mp2_task_struct)
   mp2_cache = kmem_cache_create("mp2_cache", sizeof(mp2_task_struct), 0, SLAB_HWCACHE_ALIGN, NULL);
   
   dispatch_thread=kthread_create(dispatch_thread_function, NULL, "dispatch_thread");
   get_task_struct(dispatch_thread);

   spin_lock_init(&sp_lock);
   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;
}


// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
   #endif
   // Insert your code here ...
    mp2_task_struct *pos, *next;
    unsigned long flags; 

    spin_lock_irqsave(&sp_lock, flags);
    // Free the linked list
    list_for_each_entry_safe(pos, next, &my_head, task_node) {
        list_del(&pos->task_node);
        del_timer(&pos->wakeup_timer);
        kmem_cache_free(mp2_cache, pos);
    }
    kmem_cache_destroy(mp2_cache);
    spin_unlock_irqrestore(&sp_lock, flags);

    kthread_stop(dispatch_thread);
    put_task_struct(dispatch_thread);
    /*
    remove /proc/mp2/status and /proc/mp2 using remove_proc_entry(*name, *parent)
    Removes the entry name in the directory parent from the procfs
    */
    remove_proc_entry(FILENAME, proc_dir);
    remove_proc_entry(DIRECTORY, NULL);
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);