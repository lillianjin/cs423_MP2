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

// Declare proc filesystem entry
static struct proc_dir_entry *proc_dir, *proc_entry;
// Declare file buffer
struct mutex lock;
// Declare timer
struct timer_list timer;
// Declare work queue
struct workqueue_struct *work_queue;

// Create a list head, which points to the head of the linked list
LIST_HEAD(my_head);
// Define a linked list proc_list
typedef struct proc_node {
    struct list_head head;
    int pid;
    unsigned long cpu_time;
} proc_node;


/*
Work handler function
work: pointer to the current system work
*/
static void work_handler(struct work_struct *work){
    proc_node *curr, *node;

    mutex_lock(&lock);
    // Check if the PID is valid
    list_for_each_entry_safe(curr, node, &my_head, head){
        if(get_cpu_use(curr->pid, &curr->cpu_time)!=0){
            list_del(&curr->head);
            kfree(curr);
        }
    }
    mutex_unlock(&lock);
    kfree(work);
    printk(KERN_ALERT "WORK INIT RUNNING\n");
}

/*
Time handler function
t: user defined data
*/
 void timer_function(unsigned long t) {
    // Declare work
    struct work_struct *work = (struct work_struct *)kmalloc(sizeof(struct work_struct), GFP_KERNEL);
    INIT_WORK(work, work_handler);
    // Create a work queue
    queue_work(work_queue, work);
    mod_timer(&timer, jiffies + HZ*5);
    printk(KERN_ALERT "TIMER RUNNING\n");
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
    proc_node *curr;
    char *buffer;

    // if the file is already read
    if(*offp > 0){
        return 0;
    }

    //allocate memory in buffer and set the content in buffer as 0
    buffer = (char *)kmalloc(4096, GFP_KERNEL);
    memset(buffer, 0, 4096);

    // Acquire the mutex
    mutex_lock(&lock);

    // record the location of current node inside "copied" after each entry
    list_for_each_entry(curr, &my_head, head){
        copied += sprintf(buffer + copied, "%d: %lu\n", curr->pid, curr->cpu_time);
        printk(KERN_ALERT "I AM READING %d: %s, %d, %lu\n", copied, buffer, curr->pid, curr->cpu_time);
    }

    // if the message length is larger than buffer size
    if (copied > count){
        kfree(buffer);
        return -EFAULT;
    }
    mutex_unlock(&lock);

    //Copy a block of data into user space.
    copy_to_user(buf, buffer, copied);

    // set the end of string character with value 0 (NULL)
    buffer[copied] = '\0';

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
    // Fetch PID from user
    // long mypid;
    char *buffer = (char*)kmalloc(4096, GFP_KERNEL);
    proc_node *new;
    long pid;

    memset(buffer, 0, 4096);
    // if the lengh of message is larger than buffer size or the file is already written, return
    if (count > 4096 || *offp>0) {
        kfree(buffer);
        return -EFAULT;
    }

    copy_from_user(buffer, buf, count);

    // Create a new node
    new = (proc_node *)kmalloc(sizeof(proc_node), GFP_KERNEL);
    // change string to long
    kstrtol(buffer, 10 , &pid);
    // new -> pid = (int)mypid;
    new->pid = (int)pid;

    // if not matched, not write
    if(get_cpu_use(new->pid, &new->cpu_time) != 0){
        kfree(buffer);
        kfree(new);
        return -EFAULT;
    }

    mutex_lock(&lock);
    INIT_LIST_HEAD(&new->head);
    // Add the node to the linked list
    list_add(&new->head, &my_head);
    mutex_unlock(&lock);

    printk(KERN_ALERT "I AM WRITING%s, %d, %lu\n", buffer, new->pid, new->cpu_time);

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

   // Initialize and set the timer
   setup_timer(&timer, timer_function, 0);
   mod_timer(&timer, jiffies + HZ*5);

   mutex_init(&lock);

   // Initialize workqueue
   work_queue = create_workqueue("work_queue");

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

    /*
    remove /proc/mp2/status and /proc/mp2 using remove_proc_entry(*name, *parent)
    Removes the entry name in the directory parent from the procfs
    */
    remove_proc_entry(FILENAME, proc_dir);
    remove_proc_entry(DIRECTORY, NULL);

    del_timer(&timer);

   // Wait for all the work finished
    flush_workqueue(work_queue);
    destroy_workqueue(work_queue);

    mutex_lock(&lock);
    proc_node *myproc, *n;
    // Free the linked list
    list_for_each_entry_safe(myproc, n, &my_head, head) {
        list_del(&myproc->head);
        kfree(myproc);
    }
    mutex_unlock(&lock);
    // detroy the lock
    mutex_destroy(&lock);
   printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);