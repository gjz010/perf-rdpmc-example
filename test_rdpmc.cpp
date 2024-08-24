#include <cassert>
#include <cstdint>
#include <cstring>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <optional>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <vector>
#include <perfmon/pfmlib.h>
#include <perfmon/pfmlib_perf_event.h>
#include <err.h>
#include <sys/mman.h>
#include <sched.h>
#include <array>
#define PAGE_SIZE 4096
double workload_matmul(){
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
#include <cpuid.h>
#define rmb() asm volatile("" ::: "memory")
#define rdpmc(x) __rdpmc(x)
#define rdtsc() __rdtsc()

// https://w0.hatenablog.com/entry/20140307/1394139628
#define barrier() do{rmb();\
    unsigned tmp;\
    __cpuid(0, tmp, tmp, tmp, tmp);rmb();}while(0)
template <typename T>
static inline T atomic_load(T* t)
{
    auto t_v = static_cast<volatile T*>(t);
    return __atomic_load_n(t_v, __ATOMIC_RELAXED);
}
using u32 = uint32_t;
using u64 = uint64_t;
struct LibPFM{
    LibPFM(){
        int ret = pfm_initialize();
        assert(ret == PFM_SUCCESS);
    }
    perf_event_attr get_perf_arg(const char* name){
        perf_event_attr attr;
        pfm_perf_encode_arg_t arg;
        memset(&arg, 0, sizeof(arg));
        arg.size = sizeof(arg);
        arg.attr = &attr;
        int ret = pfm_get_os_event_encoding(name, PFM_PLM3, PFM_OS_PERF_EVENT, &arg);
        assert(ret == PFM_SUCCESS);
        return attr;
    }
    ~LibPFM(){
        pfm_terminate();
    }
};

struct PerfResult{
    u64 count;
    u64 enabled;
    u64 running;
    PerfResult(u64 count, u64 enabled, u64 running):
        count(count), enabled(enabled), running(running){}
};
struct PerfEvent{
    int fd;
    perf_event_mmap_page* header;
    // https://github.com/intel/libipt/blob/4a06fdffae39dadef91ae18247add91029ff43c0/doc/howto_capture.md?plain=1#L77
    void* base, *data;
    size_t mapped_len;
    PerfEvent(const perf_event_attr& perf_config){
        // https://gist.github.com/teqdruid/2473071
        struct perf_event_attr attr;
        memset(&attr, 0, sizeof(attr));
        attr.size = sizeof(perf_event_attr);
        
        attr.type = perf_config.type;
        attr.config  = perf_config.config;
        attr.config1 = perf_config.config1;
        attr.config2 = perf_config.config2;
        attr.config3 = perf_config.config3;
        /*
        attr.type = PERF_TYPE_HARDWARE;
        attr.config = PERF_COUNT_HW_INSTRUCTIONS;
        */
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING|PERF_FORMAT_ID|PERF_FORMAT_LOST;
        fd = perf_event_open(&attr, 0, -1, -1, 0);
        if(fd<0){
            perror("PerfEvent creation failed");
        }
        assert(fd>=0);
        size_t n = 4;
        mapped_len = (((size_t)1)+(1<<n)) * PAGE_SIZE;
        base = mmap(NULL, mapped_len, PROT_READ, MAP_SHARED, fd, 0);
        assert(base!=MAP_FAILED);
        header = (perf_event_mmap_page*)base;
        data = (char*)base + header->data_offset;
    }
    void reset(){
        ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    }
    void enable(){
        barrier();
        ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
        barrier();

    }
    void disable(){
        barrier();
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        barrier();

    }
    PerfResult read_count(){
        assert(fd>=0);
        std::array<u64, 5> readout = {};
        auto result = read(fd, &readout, sizeof(readout));
        assert(result==sizeof(readout));
        //printf("readout: %ld %ld %ld %ld %ld\n", readout[0], readout[1], readout[2], readout[3], readout[4]);
        return PerfResult(readout[0], readout[1], readout[2]);
    }
    bool cap_user_rdpmc(){
        return header->cap_user_rdpmc;
    }
    bool cap_user_time(){
        return header->cap_user_time;
    }
    std::optional<PerfResult> read_count_rdpmc(){
        assert(cap_user_rdpmc());
        assert(cap_user_time());
        // https://man7.org/linux/man-pages/man2/perf_event_open.2.html
        volatile perf_event_mmap_page* pc = header;
        u32 seq, idx;
        idx = pc->index;
        u64 offset = 0;
        u64 regval = 0;
        volatile u32* lock = &pc->lock;
        u64 enabled, running;
        //int spin = 0;
        do {
            //spin++;
            seq = atomic_load(lock);
            barrier();
            enabled = pc->time_enabled;
            running = pc->time_running;
            /*
            enabled = pc->time_enabled;
            do{
                auto cyc = rdtsc();
                auto time_offset = pc->time_offset;
                auto time_mult   = pc->time_mult;
                auto time_shift  = pc->time_shift;
                u64 quot, rem;
                u64 delta;

                quot  = cyc >> time_shift;
                rem   = cyc & (((u64)1 << time_shift) - 1);
                delta = time_offset + quot * time_mult +
                        ((rem * time_mult) >> time_shift);
            }while(0);
            */
            idx = pc->index;
            offset = pc->offset;
            regval = 0;
            auto pmc_width = pc->pmc_width;
            if(idx) {
                regval = rdpmc(idx - 1);
                regval <<= 64 - pmc_width;
                regval >>= 64 - pmc_width; // signed shift right
            }else{
                return std::nullopt;
            }
            barrier();
        } while (atomic_load(lock)!= seq);
        //printf("Spinned %d (offset=%lu regval = %lu idx=%d)\n", seq, offset, regval, idx);
        //printf("Enabled=%ld running=%ld\n", enabled, running);
        auto count = offset + regval;
        return PerfResult(count, enabled, running);
    }
    ~PerfEvent(){
        int ret = munmap(base, mapped_len);
        assert(ret==0);
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
        l2_request_references.enable();
        auto cntr_start_val = l2_request_references.read_count_rdpmc();
        workload_matmul();
        auto cntr_end_val = l2_request_references.read_count_rdpmc();
        l2_request_references.disable();


        auto result = l2_request_references.read_count();
        printf("Count from read(): %ld, running=%ld, enabled=%ld\n", result.count, result.running, result.enabled);
        if(cntr_start_val.has_value() && cntr_end_val.has_value()){
            auto& cntr_start = *cntr_start_val;
            auto& cntr_end = *cntr_end_val;
            printf("Count cntr_start: %ld, running=%ld, enabled=%ld\n", cntr_start.count, cntr_start.running, cntr_start.enabled);
            printf("Count cntr_end: %ld, running=%ld, enabled=%ld\n", cntr_end.count, cntr_end.running, cntr_end.enabled);
            printf("Count rdpmc: %ld\n", cntr_end.count-cntr_start.count);
        }else{
            printf("rdpmc() failed. This may happen on first access.\n");
        }
        tot += result.count;
    }
    printf("Total = %lld\n", tot);


    //attr[0].config 
    
}