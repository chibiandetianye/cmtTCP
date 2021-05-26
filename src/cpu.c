#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<numa.h>
#include<sched.h>
#include<sys/stat.h>
#include<sys/syscall.h>
#include<assert.h>

#include"cpu.h"

inline int
get_num_CPUs()
{
	return sysconf(_SC_NPROCESSORS_ONLN);
}

pid_t 
gettid() {
	return syscall(__NR_gettid);
}

inline int
which_core_id(int thread_no) {

}

int cmt_core_affinitize(int cpu) {

}

