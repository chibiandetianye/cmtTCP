//
// Created by chimt on 2021/5/26.
//

#include<assert.h>

#ifndef MEMORY_POOL_TEST_GLOBAL_H
#define MEMORY_POOL_TEST_GLOBAL_H

#define CACHELINE 64
#define CMT_CACHELINE CACHELINE

#define _cache_aligned \
__attribute__((aligned(CACHELINE)))

#define CMT_PAGESIZE 4096

#define always_inline \
static __attribute__((always_inline))

#define _packed	\
__attribute__((packed))

# define likely(x)  __builtin_expect(!!(x), 1)
# define unlikely(x)    __builtin_expect(!!(x), 0)

#define ASSERT(x)   assert((x))

always_inline int
check2pow(int num) {
    return num & (num - 1) == 0;
}

always_inline int
fls(int x)
{
    int r;


    __asm__("bsrl %1,%0\n\t"
            "jnz 1f\n\t"
            "movl $-1,%0\n"
            "1:" : "=r" (r) : "rm" (x));
    return r+1;
}

#define ROUND_2_UP(x)  ((1UL)<<fls((x) - 1))

always_inline void
cmt_pause() {
    ;
}

#endif //MEMORY_POOL_TEST_GLOBAL_H

