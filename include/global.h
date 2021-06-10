//
// Created by chimt on 2021/5/26.
//

#include<assert.h>
#include<sys/syscall.h>
#include<stdint.h>

#ifndef MEMORY_POOL_TEST_GLOBAL_H
#define MEMORY_POOL_TEST_GLOBAL_H

#define WORD_BYTES 64

#if WORD_BYTES == 64
#define count_leading_zeros(x) __builtin_clzll (x)
#define count_trailing_zeros(x) __builtin_ctzll (x)
#endif


#define CACHELINE 64
#define CMT_CACHELINE CACHELINE

#define CMT_BITS_PER_BYTES 8

#define _cache_aligned \
__attribute__((aligned(CACHELINE)))

#define CMT_PAGESIZE 4096

#define always_inline \
static __attribute__((always_inline))

#define _packed	\
__attribute__((packed))

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif 


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

always_inline uint64_t
log2_first_set(uint64_t x) {
    uint64_t result;
    result = count_trailing_zeros(x);
    return result;
}

typedef uint64_t cmt_tid

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))



#endif //MEMORY_POOL_TEST_GLOBAL_H

