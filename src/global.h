#ifndef _GLOBAL_INCLUDE_H_
#define _GLOBAL_INCLUDE_H_

#define CACHELINE 64

#define _cache_aligned \
__attribute__(align(CACHELINE))

#define always_inline \
static __attribute__((always_inline))

#define _packed	\
__attribute__((packed))

# define likely(x)  __builtin_expect(!!(x), 1)
# define unlikely(x)    __builtin_expect(!!(x), 0)

always_inline int 
check2pow(int num) {
	return num & (num - 1) == 0;
}

always_inline void
cmt_pause() {
	;
}

#endif /** GLOBAL_INCLUDE_H_ */