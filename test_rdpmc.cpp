#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <inttypes.h>
#include <err.h>
#include <sys/mman.h>
#define PAGE_SIZE 4096
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
// already provided by libpfm
/*
int perf_event_open(struct perf_event_attr *hw_event, pid_t pid, int cpu, int group_fd, unsigned long flags){
    return syscall(SYS_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}
*/
#include <x86intrin.h>
// https://w0.hatenablog.com/entry/20140307/1394139628
#define barrier() _mm_mfence()
#define rdpmc(x) __rdpmc(x)
#define rdtsc() __rdtsc()

struct LibPFM{
    LibPFM(){
        int ret = pfm_initialize();
        assert(ret == PFM_SUCCESS);
    }
    pfm_perf_encode_arg_t get_perf_arg(const char* name){
        perf_event_attr attr;
        pfm_perf_encode_arg_t arg;
        memset(&arg, 0, sizeof(arg));
        arg.size = sizeof(arg);
        arg.attr = &attr;
        int ret = pfm_get_os_event_encoding(name, PFM_PLM3, PFM_OS_PERF_EVENT, &arg);
        assert(ret == PFM_SUCCESS);
        return arg;
    }
    ~LibPFM(){
        pfm_terminate();
    }
};

struct PerfEvent{
    int fd;
    perf_event_mmap_page* header;
    // https://github.com/intel/libipt/blob/4a06fdffae39dadef91ae18247add91029ff43c0/doc/howto_capture.md?plain=1#L77
    void* base, *data;
    PerfEvent(const pfm_perf_encode_arg_t& perf_config){
        // https://gist.github.com/teqdruid/2473071
        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(perf_event_attr);
        
        attr.type = perf_config.attr->type;
        attr.config  = perf_config.attr->config;
        attr.config1 = perf_config.attr->config1;
        attr.config2 = perf_config.attr->config2;
        attr.config3 = perf_config.attr->config3;
        /*
        attr.type = PERF_TYPE_HARDWARE;
        attr.config = PERF_COUNT_HW_INSTRUCTIONS;
        */
        // Does these really work?
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        fd = perf_event_open(&attr, getpid(), -1, -1, 0);
        if(fd<0){
            perror("PerfEvent creation failed");
        }
        assert(fd>=0);
        int n = 4;
        int m = 4;
        base = mmap(NULL, (1+(std::pow(2, n))) * PAGE_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
        assert(base!=MAP_FAILED);
        header = (perf_event_mmap_page*)base;
        data = (char*)base + header->data_offset;
        /*
        header->aux_offset = header->data_offset + header->data_size;
        header->aux_size   = (std::pow(2, m)) * PAGE_SIZE;
        
        aux = mmap(NULL, header->aux_size, PROT_READ, MAP_SHARED, fd,
               header->aux_offset);
        assert(aux!=MAP_FAILED);
        */
    }
    void reset(){
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    }
    void enable(){
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }
    void disable(){
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    }
    long long read_count(){
        assert(fd>=0);
        long long count = 0;
        int result = read(fd, &count, sizeof(count));
        assert(result==sizeof(count));
        return count;
    }
    bool cap_user_rdpmc(){
        return header->cap_user_rdpmc;
    }
    long long read_count_rdpmc(){
        assert(cap_user_rdpmc());
        // https://man7.org/linux/man-pages/man2/perf_event_open.2.html
        using u32 = uint32_t;
        using u64 = uint64_t;
        auto pc = header;
        u32 seq, time_mult, time_shift, idx, width;
        u64 count, enabled, running;
        u64 cyc, time_offset;

        do {
            seq = pc->lock;
            barrier();
            enabled = pc->time_enabled;
            running = pc->time_running;

            if (pc->cap_user_time && enabled != running) {
                cyc = rdtsc();
                time_offset = pc->time_offset;
                time_mult   = pc->time_mult;
                time_shift  = pc->time_shift;
            }

            idx = pc->index;
            count = pc->offset;

            if (pc->cap_user_rdpmc && idx) {
                width = pc->pmc_width;
                count += rdpmc(idx - 1);
            }

            barrier();
        } while (pc->lock != seq);
        return count;
    }
    ~PerfEvent(){
        close(fd);
    }
};

int main(){
    printf("Hello, world!\n");
    LibPFM pfm;

    PerfEvent l2_request_references(pfm.get_perf_arg("L2_REQUEST.REFERENCES"));
    if(l2_request_references.cap_user_rdpmc()){
        printf("cap_user_rdpmc is OK!\n");
    }else{
        printf("cap_user_rdpmc is BROKEN!\n");
    }
    long long tot = 0;
    for(int i=0; i<10; i++){
        l2_request_references.reset();
        long long cntr_start = l2_request_references.read_count_rdpmc();
        l2_request_references.enable();
        workload_matmul();
        l2_request_references.disable();
        long long cntr_end = l2_request_references.read_count_rdpmc();


        printf("Count: %lld\n", l2_request_references.read_count());
        printf("Count rdpmc: %lld (from %lld to %lld)\n", cntr_end-cntr_start, cntr_start, cntr_end);
        tot += cntr_end - cntr_start;
    }
    printf("Total = %lld\n", tot);


    //attr[0].config 
    
}