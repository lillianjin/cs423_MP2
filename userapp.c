#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

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
    if(!f){
        perror("Proc file not exists!");
        return 0;
    }
    while(1){
        curr = getline(&line, &n, f);
        // printf("curr is %ld\n", curr);
        if(curr == 1){
            break;
        }else{
            tmp = strtok(line, ":");
            printf("this line is %s\n", tmp);
            cur_pid = strtok(tmp, ",");
            // printf("the pid in line is %s\n", cur_pid);
            if(strtoul(cur_pid, NULL, 10) == pid){
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

void do_job(){
    long long res = 1;
	for(int i=0; i<100000000; i++){
		for(int j=20; j > 0; j--){
			res = res * j;
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
        perror("Please input the command again in the format of './userapp period process_time(in ms) num_of_jobs'" );
        return 0;
    }

    pid = getpid();
    period = strtoul(argv[1], NULL, 10);
    process_time = strtoul(argv[2], NULL, 10);
    num = atoi(argv[3]);

    printf("pid is %u, period is %u, process_time is %lu, number of jobs is %d\n", pid, period, process_time, num);

    if(process_time > period){
        perror("Period must be larger than process time");
        return 0;
    }

    // REGISTERATION
    REGISTER(pid, period, process_time);
    if(my_read_status(pid)==0){
        printf("Registration failed.\n");
        return 0;
    }
    printf("Registration succeeded.\n");

    // YIELD
    // gettimeofday(&init, NULL);
    printf("Yield begins.\n");
    YIELD(pid);
    t = 0;
    while(t < num){
        gettimeofday(&start, NULL);
        printf("job %d is running, pid: %u, start time:\t%llu ms\n", t, pid, (unsigned long long)(start.tv_sec * 1000));
        do_job();
        gettimeofday(&end, NULL);
        printf("job %d finished, pid: %u, end time:\t%llu ms\n", t, pid, (unsigned long long)(end.tv_sec * 1000));
        YIELD(pid);
        t++;
    }
    printf("Yield succeeded.\n");

    
    // DEREGISTERATION
    // DEREGISTER(pid);
    // if(my_read_status(pid)==1){
    //     printf("Unregistration failed.\n");
    //     return 0;
    // }
    // printf("Unregistration succeeded.\n");



}
