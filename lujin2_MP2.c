#define LINUX

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
struct mutex mutexLock;
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
    spin_unlock_irqrestore(&sp_lock, flags);
    wake_up_process(dispatch_thread);
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
    setup_timer(&curr_task->wakeup_timer, my_timer_function, (unsigned long)curr_task->pid);

    // check for admission_control

    // add the task to task list
    mutex_lock(&mutexLock);
    list_add(&(curr_task->task_node), &my_head);
    mutex_unlock(&mutexLock);
}


/*
This function allows the application to notify the RMS scheduler that the application has finished using the RMS scheduler
*/
static void mp2_deregister(unsigned int pid) {
    #ifdef DEBUG
    printk(KERN_ALERT "DEREGISTRATION MODULE LOADING\n");
    #endif
    mp2_task_struct *curr, *next;
    mutex_lock(&mutexLock);
    list_for_each_entry_safe(curr, next, &my_head, task_node) {
        // remove the task and destroy all data structure
        del_timer(&(curr->wakeup_timer));
        list_del(&curr->task_node);
        kmem_cache_free(mp2_cache, curr);
        }
    mutex_unlock(&mutexLock);
}
 

/*
This function notifies the RMS scheduler that the application has finished its period.
*/
static void mp2_yield(unsigned int pid) {
    #ifdef DEBUG
    printk(KERN_ALERT "YIELD MODULE LOADING\n");
    #endif
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

    // if the file is already read
    if(*offp > 0){
        return 0;
    }

    //allocate memory in buffer and set the content in buffer as 0
    buffer = (char *)kmalloc(4096, GFP_KERNEL);
    memset(buffer, 0, 4096);

    // Acquire the mutex
    mutex_lock(&mutexLock);

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
    mutex_unlock(&mutexLock);

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
    unsigned long period;
    unsigned long process_time;

    memset(buffer, 0, 4096);
    // if the lengh of message is larger than buffer size or the file is already written, return
    if (count > 4096 || *offp>0) {
        kfree(buffer);
        return -EFAULT;
    }

    copy_from_user(buffer, buf, count);
    buffer[count] = '\0';
    printk(KERN_ALERT "buffer[0] is: %c", buffer[0]);

    if(count > 0){
        switch (buffer[0])
        {
            case REGISTRATION:
                sscanf(buffer + 2, "%u,%lu,%lu\n", &pid, &period, &process_time);
                mp2_register(pid, period, process_time);
                break;
            case YIELD:
                sscanf(buffer + 2, "%u\n", &pid);
                break;
            mp2_yield(pid);
            case DEREGISTRATION:
                sscanf(buffer + 2, "%u\n", &pid);
                mp2_deregister(pid);
                break;
            default:
                kfree(buffer);
                return 0;
        }
    }

    printk(KERN_ALERT "I AM WRITING: %s, %u, %u, %lu\n", buffer, pid, period, process_time);

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

    /*
    remove /proc/mp2/status and /proc/mp2 using remove_proc_entry(*name, *parent)
    Removes the entry name in the directory parent from the procfs
    */

    remove_proc_entry(FILENAME, proc_dir);
    remove_proc_entry(DIRECTORY, NULL);

    //Destory slab cache
    if(mp2_cache != NULL){
        kmem_cache_destroy(mp2_cache);
    }

    mutex_lock(&mutexLock);
    // Free the linked list
    list_for_each_entry_safe(pos, next, &my_head, task_node) {
        list_del(&pos->task_node);
        kfree(pos);
    }
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);