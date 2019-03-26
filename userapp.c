#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>


void REGISTER(unsigned int pid, unsigned int period, unsigned long process_time){
    FILE *f = fopen("/proc/mp2/status", "w");
    if(!f){
        perror("Proc file not exists!");
        return;
    }
    fprintf(f, "R, %u, %u, %lu", pid, period, process_time);
    fclose(f);
}

void DEREGISTER(unsigned int pid){
    FILE *f = fopen("/proc/mp2/status", "w");
    if(!f){
        perror("Proc file not exists!");
        return;
    }
    fprintf(f, "D, %u", pid);
    fclose(f);
}

void YIELD(unsigned int pid){
    FILE *f = fopen("/proc/mp2/status", "w");
    if(!f){
        perror("Proc file not exists!");
        return;
    }
    fprintf(f, "Y, %u", pid);
    fclose(f);
}

// check if the task is successfully registered in proc system
int my_read_status(unsigned int pid){
    FILE *f = fopen("/proc/mp2/status", "r+");
    ssize_t curr;
    char *tmp, *cur_pid;
    size_t n;
    char *line = malloc(4096);
    int answer = 0;
    if(!f){
        perror("Proc file not exists!");
        return 0;
    }
    printf("Task in /proc/mp2/status are listed: \n");
    while(1){
        curr = getline(&line, &n, f);
        // printf("curr is %ld\n", curr);
        if(curr == 1){
            fclose(f);
            return answer;
        }else{
            tmp = strtok(line, ":");
            printf("%s", tmp);
            cur_pid = strtok(tmp, ",");
            // printf("the pid in line is %s", cur_pid);
            if(strtoul(cur_pid, NULL, 10) == pid){
                answer = 1;
            }
        }
    }
}

void do_job(unsigned long time){
    long long res = 1;
    clock_t t0 = clock(), t1;
    // unsigned long long t0, t1;
    // t0 = (unsigned long long)start.tv_sec * 1000000 + start.tv_usec;
    t1 = t0 + time/1000 * CLOCK_PER_SEC;
	for(int i=0; i<100000000; i++){
		for(int j=20; j > 0; j--){
			res = res * j;
		}
        if(clock() > t1){
            break;
        }
	}
}

/*
    argc: should be 4 
    argv[0]: ./userapp
    argv[1]: period
    argv[2]: process_time (in milliseconds)
    argv[3]: number of jobs
*/
int main(int argc, char* argv[]){
    int num;
    unsigned int pid, period;
    unsigned long process_time;
    struct timeval start, end;
    int t;

    if(argc != 4){
        perror("Please input the command again in the format of './userapp period process_time(in ms) num_of_jobs'." );
        return 0;
    }

    pid = getpid();
    period = strtoul(argv[1], NULL, 10);
    process_time = strtoul(argv[2], NULL, 10);
    num = atoi(argv[3]);

    printf("Read task succeeded: pid is %u, period is %u, process_time is %lu, number of jobs is %d.\n", pid, period, process_time, num);

    if(process_time > period){
        perror("Period must be larger than process time");
        return 0;
    }

    // REGISTERATION
    printf("Task %u registeration begins.\n", pid);
    REGISTER(pid, period, process_time);
    if(my_read_status(pid)==0){
        printf("Task %u registration failed.\n", pid);
        return 0;
    }
    printf("Task %u registration succeeded.\n", pid);

    // YIELD
    // gettimeofday(&init, NULL);
    // printf("Task %u yield begins.\n", pid);
    YIELD(pid);
    t = 0;
    while(t < num){
        gettimeofday(&start, NULL);
        printf("Job %d of task %u is running, start time:\t%llu.%llu s.\n", t, pid, (unsigned long long)start.tv_sec, (unsigned long long)start.tv_usec);
        do_job(process_time);
        gettimeofday(&end, NULL);
        printf("Job %d of task %u finished, end time:    \t%llu.%llu s.\n", t, pid, (unsigned long long)end.tv_sec, (unsigned long long)end.tv_usec);
        printf("Task %u yield begins...\n", pid);
        YIELD(pid);
        t++;
    }
    // printf("Task %u yield succeeded.\n", pid);

    
    // DEREGISTERATION
    DEREGISTER(pid);
    if(my_read_status(pid)==1){
        printf("Task %u unregistration failed.\n", pid);
    }
    printf("Task %u unregistration succeeded.\n", pid);

    return 0;
}
