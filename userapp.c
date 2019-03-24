#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void REGISTER(unsigned int pid, unsigned int period, unsigned long process_time){
    FILE *f = fopen("/proc/mp2/status", "w");
    fprintf(f, "R, %u, %u. %lu", pid, period, process_time);
    fclose(f);
}

void DEREGISTER(unsigned int pid){
    FILE *f = fopen("/proc/mp2/status", "w");
    fprintf(f, "D, %u", pid);
    fclose(f);
}

void YIELD(unsigned int pid){
    FILE *f = fopen("/proc/mp2/status", "w");
    fprintf(f, "Y, %u", pid);
    fclose(f);
}

/*
    argc: should be 3 
    argv[0]: ./userapp
    argv[1]: period
    argv[2]: process_time (in milliseconds)
*/
int main(int argc, char* argv[]){
    unsigned int pid, period;
    unsigned long process_time;

    if(argc != 3){
        perror("Please input the command again in the format of './userapp period process_time(in ms)'" );
        return 1;
    }

    pid = getpid();
    period = strtoul(argv[1], NULL, 10);
    process_time = strtoul(argv[2], NULL, 10);

    printf("pid is %u, period is %u, process_time is %lu\n", pid, period, process_time);


}

