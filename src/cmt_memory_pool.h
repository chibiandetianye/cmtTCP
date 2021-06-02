

#ifndef CMT_MEMORY_POOL_INCLUDE_H
#define CMT_MEMORY_POOL_INCLUDE_H

#include<unistd.h>
#include<stdint.h>

#include"global.h"

#define CMT_MAX_ALLOC_FROM_POOL (512)

#define CMT_DEFAULT_POOL_SIZE (CMT_PAGESIZE)

#define CMT_POOL_ALIGNMENT 16

#define TOTAL_BINS 7

#ifndef PTR_SIZE
#define PTR_SIZE (sizeof(void*))
#endif /* PTR_SIZE */

/** \brief  data structure of finanlly allocated to user
    data structure lays like
    -----------------------------------------------------------
    pointer to next chunk
    -----------------------------------------------------------
    user memory space
    -----------------------------------------------------------
 */
typedef struct cmt_chunk {
    struct cmt_chunk* next;
} cmt_chunk_t;

/** \brief data structure of managing specific size memory size
   ------------------------------------------------------------
 */
typedef struct cmt_pool_bin {
    uint16_t         nums; //total num of free chunk
    struct cmt_bunk* free; //free list of memory chunk
} cmt_pool_bin_t;

/** \brief data structure of managing memory chunks which size
    larger than 512B
    -----------------------------------------------------------
 */
typedef struct large_chunk {
    uint32_t size; //size of memory chunk
    struct   large_chunk* next; // pointer to next large chunk
} large_chunk_t;

/** \brief data structure of recording remained memory space of the
    memory space originally allocate from system
    -----------------------------------------------------------
 */
typedef struct last_block {
    uint32_t size; // remained size
    char* data;
} last_block_t;

/** \brief data structure of mnaging a big and contiguous memory
    -----------------------------------------------------------
    \note
    Pool will apply for a new space from the system when
    main_arena is not sufficient. This structure is mainly useful
    used in resetting memory pool and destorying operation
    -----------------------------------------------------------
 */
typedef struct non_main_arena {
    struct non_main_arena*  next;    //next non_main_arena
    uint32_t                size;  //total size of managing size
    char*                   c[0];
} non_main_arena_t;

/** \brief data structure of controling the memory spin
    -----------------------------------------------------------
 */
typedef struct cmt_pool_control_block {
    uint64_t            size; //total size in the memory pool
    uint64_t            remained; //total free size in the memory pool
    large_chunk_t*      large_ctb; //control block of large chunks
    last_block_t        last; //record the last block
    non_main_arena_t*   non_main_arena;
} cmt_pool_control_block_t;

/** \brief data structure of memory pool
    -----------------------------------------------------------
 */
typedef struct cmt_pool {
    cmt_tid tid;  //pthread id
    cmt_pool_control_block_t    cb; // control block of memory chunk
    cmt_pool_bin_t              smallbins[TOTAL_BINS]; //management of small chunk
} cmt_pool_t;

/** \brief get a thread private memory pool */
cmt_pool_t* get_cmt_pool();

/** \brief get a specified size thead private memory pool 
    @param size total size of memory pool
            if size < CMT_DEFAULT_POOL_SIZE, the size will be up to
            CMT_DEFAULT_POOL_SIZE
*/
cmt_pool_t* get_cmt_pool_s(size_t size);

/** \brief create the memory pool
    -----------------------------------------------------------
    @param size total size of memory pool
            if size < CMT_DEFAULT_POOL_SIZE, the size will be up to
            CMT_DEFAULT_POOL_SIZE
 */
cmt_pool_t* cmt_create_pool(size_t size);

/** \brief detroy the memory pool
    -----------------------------------------------------------
    @param pool memory pool should be destory
 */
void cmt_destory_pool(cmt_pool_t* pool);

/** \brief reset a memory pool to iginal setting
    -----------------------------------------------------------
    @param pool memory pool should be reseted
*/
int cmt_reset_pool(cmt_pool_t* pool);

/** \brief alloc memory from pool
    -----------------------------------------------------------
    @param pool memory pool
    @param size size to allocated
    @return pointer to memory
 */
void* cmt_palloc(cmt_pool_t* pool, size_t size);

/** \brief alloc memory from pool and set the memory 0
    -----------------------------------------------------------
    @param pool memory pool
    @param size size to allocated
    @return pointer to memory
 */
void* cmt_pcalloc(cmt_pool_t* pool, size_t size);

/** \ief free memory to memory pool
    -----------------------------------------------------------
    @param pool memory pool
    @param size size to allocated
    @return p pointer to memory
 */
void cmt_pfree(cmt_pool_t* pool, void* p, size_t size);

/** \brief allocate size bytes and places the address of the
    memory be multiple of alignment
    -----------------------------------------------------------
    @param pool memory pool
    @param size size to allocated
    @param align alignment of address
    @return p pointer to memory
 */
 void* cmt_pmemalign(cmt_pool_t* pool, size_t align, size_t size);

/** \brief free the memory allocated by cmt_pmemalign
   -----------------------------------------------------------
   @param pool memory pool
   @param size size to allocated
   @param align alignment of address
   @param p pointer to memmory
   @return p pointer to memory
*/
 void cmt_pmemfree(cmt_pool_t* pool, void *p, size_t align, size_t size);

#endif //MEMORY_POOL_TEST_MEMORY_POOL_H

