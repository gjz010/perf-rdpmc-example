#include <cassert>
#include <linux/perf_event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

int
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
    long int ret;

    ret = syscall(SYS_perf_event_open, hw_event, pid, cpu,
                    group_fd, flags);
    assert(ret>=0);
    return (int) ret;
}

int
main(void)
{
    int                     fd;
    long long               count;
    struct perf_event_attr  pe;

    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_INSTRUCTIONS;
    pe.disabled = 1;
    pe.exclude_kernel = 0;
    pe.exclude_hv = 1;

    fd = perf_event_open(&pe, getpid(), -1, -1, 0);
    if (fd == -1) {
        fprintf(stderr, "Error opening leader %llx\n", pe.config);
        exit(EXIT_FAILURE);
    }

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    
    printf("Measuring instruction count for this printf\n");
    ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    size_t result = read(fd, &count, sizeof(count));
    assert(result == sizeof(count));
    printf("Used %lld instructions\n", count);

    close(fd);
}