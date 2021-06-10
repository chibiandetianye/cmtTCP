#ifndef _VEC_BOOTSTRAP_INCLDE_H_
#define _VEC_BOOTSTRAP_INCLDE_H_

#include<stdint.h>

#include"cmtInfroConfig.h"

typedef struct {
    uint32_t len;
    uint32_t capacity;
    //uint8_t numa_id; /** NUMA ID */
    uint8_t vector_data[0];
} vec_header_t;

#define VEC_NUMA_UNSPECIFIED (0xFF)

#define _vec_find(v)    ((vec_head_t *) (v) - 1)

#ifndef word
#define word uint64_t
#endif /* WORD */

#define _vec_round_size(s) \
  (((s) + sizeof(word) - 1) & ~(sizeof(word) - 1))

ALWAYS_INLINE int
vec_header_bytes(int header_bytes) {
    return round_pow2(header_bytes + sizeof(vec_header_t),
        sizeof(vec_header_t));
}

ALWAYS_INLINE void*
vec_header(void* v, int header_bytes) {
    return v - vec_header_bytes(header_bytes);
}

ALWAYS_INLINE void*
vec_header_end(void* v, int header_bytes) {
    return v + vec_header_bytes(header_bytes);
}

ALWAYS_INLINE void*
vec_aligned_header_bytes(int header_bytes, int align) {
    return round_pow2(header_bytes + sizeof(vec_header_t), align);
}

ALWAYS_INLINE void*
vec_aligned_header(void* v, int header_bytes, int align)
{
    return v - vec_aligned_header_bytes(header_bytes, align);
}

ALWAYS_INLINE void*
vec_aligned_header_end(void* v, int header_bytes, int align)
{
    return v + vec_aligned_header_bytes(header_bytes, align);
}

#define _vec_len(v)	(_vec_find(v)->len)

#define vec_len(v)	((v) ? _vec_len(v) : 0)
extern uint32_t vec_len_not_inline(void* v);

//#define _vec_numa(v) (_vec_find(v)->numa_id)
//
//#define vec_numa(v) ((v) ? _vec_numa(v) : 0)

#define vec_bytes(v) (vec_len (v) * sizeof (v[0]))

#define vec_max_len(v) 								\
  ((v) ? ((_vec_find(v)->capacity : 0)

#define vec_set_len(v, l) do {     \
    ASSERT(v);                     \
    ASSERT((l) <= vec_max_len(v)); \
    _vec_len(v) = (l);             \
} while (0)

#define vec_reset_length(v) do { if (v) vec_set_len (v, 0); } while (0)

#define vec_end(v)	((v) + vec_len (v))

#define vec_is_member(v,e) ((e) >= (v) && (e) < vec_end (v))

#define vec_elt_at_index(v,i)			\
({						\
  ASSERT ((i) < vec_len (v));			\
  (v) + (i);					\
})

#define vec_elt(v,i) (vec_elt_at_index(v,i))[0]

#define vec_foreach(var,vec) for (var = (vec); var < vec_end (vec); var++)

#define vec_foreach_backwards(var,vec) \
for (var = vec_end (vec) - 1; var >= (vec); var--)

#define vec_foreach_index(var,v) for ((var) = 0; (var) < vec_len (v); (var)++)

#define vec_foreach_index_backwards(var,v) \
  for ((var) = vec_len((v)) - 1; (var) >= 0; (var)--)

//always_inline int
//vec_get_numa (void *v)
//{
//  vec_header_t *vh;
//  if (v == 0)
//    return 0;
//  vh = _vec_find (v);
//  return vh->numa_id;
//}

#endif /* _VEC_BOOTSTRAP_INCLDE_H_  */
