#ifndef _CPU_INCLUDE_H_
#define _CPU_INCLUDE_H_

#include<pthread.h>

int get_nums_cpu()

pid_t gettid();

int
cmt_core_affinitize(int cpu);


#endif /** _CPU_INCLUDE_H_ */
