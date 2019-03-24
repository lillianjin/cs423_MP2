#include <stdlib.h>

void REGISTER(unsigned int pid, unsigned int period, unsigned long process_time){
    FILE *f = fopen("/proc/mp2/status", "w");
    fprintf(f, "R, %u, %lu. %lu", pid, period, process_time);
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

int main(int argc, char* argv[]){
    unsigned int pid = gitpid();
    
}

