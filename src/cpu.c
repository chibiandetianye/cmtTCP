#include<stdio.h>
#include<sys/types.h>
#include<sys/sysinfo.h>
#include<stdio.h>
#include<errno.h>

#define __USE_GNU
#include<sched.h>
#include<ctype.h>
#include<string.h>
#include<pthread.h>

#include"cpu.h"

int 
get_nums_cpu() {
	return sysconf(_SC_NPROCESSORS_CONF);
}

pid_t 
gettid() {
	return syscall(__NR_gettid);
}

int 
cmt_core_affinitize(int cpu) {
	cpu_set_t cpus;
	size_t n;
	int ret;

	n = get_nums_cpu();

	if (cpu < 0 || cpu >= (int)n) {
		errno = -EINVAL;
		return -1;
	}

	CPU_ZERO(&cpus);
	CPU_SET((unsigned)cpu, &cpus);

	ret = sched_setaffinity(Gettid(), sizeof(cpus), &cpus);
	return ret;
}

