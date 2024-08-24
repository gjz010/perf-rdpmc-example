#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <perfmon/pfmlib.h>
int workload_matmul(){
    int n = 1000;
    std::vector<double> a(n*n);
    std::vector<double> b(n*n);
    std::vector<double> c(n*n);
    for(int i=0;i<n*n;i++){
        a[i]=1.0;
        b[i]=2.0;
        c[i]=3.0;
    }
    for (int i=0;i<n;i++){
        for (int j=0;j<n;j++){
            for (int k=0;k<n;k++){
                c[i*n+j] += a[i*n+k] * b[k*n+j];
            }
        }
    }
    return c[0];
}

// syscall wrapper
int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags){
    return syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}




int main(){
    printf("Hello, world!");
    pfm_initialize();
    struct perf_event_attr attr[1];
    attr[0].size = sizeof(perf_event_attr);
    // https://android.googlesource.com/platform/system/extras/+/android-8.1.0_r22/simpleperf/event_type_table.h
    attr[0].type = PERF_TYPE_HW_CACHE;
    //attr[0].config 
    pfm_terminate();
}