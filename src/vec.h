#ifndef _VEC_INCLUDE_H_
#define _VEC_INCLUDE_H_

#include<stdlib.h>

#include"vec_bootstrap.h"

extern void* vec_resize_allocate_memory(
	pool_t* pool,
	void* v, int length_increment,
	int data_bytes,
	int data_align,
	);

ALWAYS_INLINE int
_vec_will_expand(void* v,
	int data_bytes) {

	if (LIKELY(v != 0)) {
		vec_header_t* vh = vec_header(v);
		return vh->capacity > data_bytes ? 0 : 1;
	}

	return 1;
}


/** brief low-level vector resize function, usually not called directly
	@param P memory pool
	@param V pointer to a vector
	@param DB data_bytes required in vector
	@param HB header size in bytes (may be zero)
	@param A alignment
	@return v pointer to a resized vector
*/
#define _vec_resize(P, V, DB, HB, A) (
{
	__typeof__((V)) _v;
	_v = _vec_resize_inline(P, (void*)_v, DB, HB, A);
	_v;
})


ALWAYS_INLINE void*
_vec_resize_inline(
	pool_t* pool,
	void* v,
	int data_bytes, int header_bytes, int data_align) {
	int ret;

	ret = _vec_will_expand(v, data_bytes);
	if (UNLIKELY(ret == 1)) {
		v = vec_resize_allocate_memory(pool, v, length_increment,
			data_bytes, data_aligned);
#ifdef DBGERR
		if (!v) {
			TRACE_ERROR("lack of memory");
		}
#endif
	}

	return v;
}


/** \brief Resize a vector
	@param P memory pool
	@param V pointer to a vector
	@param N number of elm to add
	@param H header size in bytes (may be zero)
	@param A alignment (may be zero)
	@return V (value result of macro parameter)
*/
#define vec_resize_ha(P, V, N, H, A)	\
do {
int size = N * sizeof(V[0]);
V = _vec_resize(P, V, size, H, A);
} while (0)

#define vec_resize(V, N) vec_resize_ha(V, N, 0, 0)

#define vec_resize_aligned(V, N, A) vec_resize_ha(V, N, 0, A)

/** \brief alloc more N space for adding
	@param P memory pool
	@param V pointer to a vector
	@param N number of element for adding
	@param H header size in bytes(may be zero)
	@param A alignement (may be zero)
*/
#define vec_alloc_ha(P,V, N, H, A)	\
do {
uint32_t len = vec_len(V);
int size = (int)len + N * sizeof(V[0]);
vec_resize_ha(P, V, size, H, A);
} while (0)

#define vec_alloc(V, N) vec_alloc_ha(V, N, 0, 0)

#define vec_alloc_align(V, N, A) vec_alloc_ha(V, N, 0, A)

/** /brief Create a new vector of given type and length
	@param P pool of memory
	@param T type of element in the vector
	@param N number of element to add
	@param H header size in bytes (may be zero)
	@param A alignment (may be zero)
*/
#define vec_new_ha(P, T, N, H, A)	\
({
(T*)_vec_resize(P, (T*)0, N * sizeof(T), (H), (A))
	})

#define vec_new(T, N) vec_new_ha(T, N, 0, 0)

#define vec_new_aligned(T, N, A) vec_new_ha(T, N, 0, A)

	/** /brief free vector's memory
		@param P memory pool
		@param V pointer to vector
		@param H size of header
	*/
#define vec_free_h(P, V, H)	\
do {
	if (LIKELY(V)) {
		pool.free(V, H);
	}
} while (0)

#define vec_free(V)	vec_free_h(V, 0)

#define vec_free_header(h) 

#define vec_dup_ha(P, V, H, A) \
({
	__typeof__((V)[0])* _v = 0;
uint32_t len = vec_len(V);
if (len > 0) {
	vec_resize_ha(P, _v, len, H, A);
	memcpy(_v, V, len * sizeof(_v[0]);
}
(__typeof__((V[0]))*)_v;
		})

#define vec_dup(V) vec_dup_ha(V, 0, 0)

#define vec_dup_aligned(V, A) vec_dup_ha(V, 0, A)

		/** \brief Copy a vector, memory wrapper.
			@param DST destination
			@param SRC source
		*/
#define vec_copy(DST, SRC) memcpy(DST, SRC, sizeof(SRC[0]) * vec_len(SRC))

		/** \brief Clone a vector. Make a new vector with the same size as
			the given vector
		*/
#define vec_clone(P, NEW_V, ODL_V)	\
do {
			uint32_t len = vec_len(OLD_V);
		if (len > 0) {
			vec_resize_ha(P, NEW_V, len, (0), (0))
		}
} while (0)

/** /brief Make sure the ith index is validated
	@param P memory pool
	@param V pointer to a vector
	@param I index which will be valid
	@param H header size in bytes
	@param A alignemnt
*/
#define vec_validate_ha(P, V, I, H, A) 
do {
	uint32_t idx = (unit32_t)(I);
	uint32_t len = vec_len(V);
	if (len < idx) {
		vec_resize_ha(P, V, idx, H, A);
	}
}

#define vec_validate(V, I) vec_validate(V, I, 0, 0)

#define vec_validate(V, I, A) vec_validate(V, I, 0, A)

/** /brief add 1 element to end of vector
	@param P memory pool
	@param V pointer to a vector
	@param E elment to add
	@param H header size in bytes
	@oaram A alignment
*/
#define vec_add1_ha(P, V, E, H, A)	\
do {
int len = (int)vec_len(V);
V = _vec_resize(P, V, (len + 1) * (sizeof((V)[0])), H, A);
(V)[len] = E;
ven_len(V) += 1;
} while (0)

#define vec_add1(V, E) vec_add1_ha(V, E, 0, 0)

#define vec_add1_aligned(V, E, A) vec_add1_ha(V, E, 0, A)

/** \brief Add N elements to end of vector V,
	return pointer to new elements in P. (general version)

	@param V pointer to a vector
	@param NP pointer to new vector element(s)
	@param N number of elements to add
	@param H header size in bytes (may be zero)
	@param A alignment (may be zero)
	@return V and NP (value-result macro parameters)
*/
#define vec_add2_ha(P, V, NP, N, H, A)	\
do {
int len = (int)vec_len(V);
V = _vec_resize(P, V, (len + N) * (sizeof((V)[0])), H, A);
NP = V + n;
} while (0)

#define vec_add2(V, NP, N) vec_add2_ha(V, NP, N, 0, 0)

#define vec_add2_aligned(V, NP, N, A) vec_add2_ha(V, NP, N. 0, A)

/** \brief Add N elements to end of vector V (general version)
	@param P memory pool
	@param V pointer to a vector
	@param E pointer to element(s) to add
	@param N number of elements to add
	@param H header size in bytes (may be zero)
	@param A alignment (may be zero)
	@return V (value-result macro parameter)
*/
#define vec_add_ha(P, V, E, N, H, A)	\
do {
int len = (int)vec_len(V);
V = _vec_resize(P, V, (len + N) * (sizeof((V)[0])), H, A);
memcpy((V)+len, (E), N * sizeof((V)[0]));
} while (0)

#define vec_add(V, E, N) vec_add(V, E, 0, 0)

#define vec_add_aligned(V, E, N, 0, A) vec_add(V, E, 0, A)

/** \brief Returns last element of a vector and decrements its length

	@param V pointer to a vector
	@return E element removed from the end of the vector
*/
#define vec_pop(V)	\
({
	uint32_t len = _vec_len(V);	\
	ASSERT(len > 0);	\
len -= 1;	\
_vec_len(V) = len;	\
(V)[len];	\
	})

#define vec_pop2(V)	\
({	\
	uint32_t len = vec_len(V);			\
  if (len > 0) (E) = vec_pop(V);		\
  _v(l) > 0;	\ 
	})

	/** \brief Insert N vector elements starting at element M,
		initialize new elements (general version).
		@param memory pool
		@param V (possibly NULL) pointer to a vector.
		@param N number of elements to insert
		@param M insertion point
		@param INIT initial value (can be a complex expression!)
		@param H header size in bytes (may be zero)
		@param A alignment (may be zero)
		@return V (value-result macro parameter)
	*/
#define vec_insert_init_empty_ha(P, V, N, M, INIT, H, A)	\
do {	\
int len = (int)vec_len(V);						\
	V = _vec_resize(P, V, (len + (N)) * (sizeof((V)[0])), H, A);	\
memmove((V)+(M)+(N), (V)+(M),						\
	(len - (M)) * sizeof((V)[0]));						\
memset(V + (M), INIT, (N) * sizeof((V)[0]));			\
} while (0)

#define vec_insert_ha(P, V, N, M, H, A) vec_insert_init_empty_ha(P, V, N, M, 0, H, A)

#define vec_insert(P, V, N, M) vec_insert_ha(P, V, N, M, 0, 0)

#define vec_insert_aligned(P, V, N, M, A) vec_insert_ha(P, V, N, M, 0, A)

#define vec_insert_init_empty(P, V, N, M, INIT) \
	vec_insert_init_empty_ha(P, V, N, M, INIT, 0, 0)

#define vec_insert_init_empty_aligned(P, V, N, M, INT, A)	\
	vec_insert_init_empty_ha(P, V, N, M, INIT, 0, A)

	/** \brief Insert N vector elements starting at element M,
		insert given elements (general version)
		@param P memory pool
		@param V (possibly NULL) pointer to a vector.
		@param E element(s) to insert
		@param N number of elements to insert
		@param M insertion point
		@param H header size in bytes (may be zero)
		@param A alignment (may be zero)
		@return V (value-result macro parameter)
		*/
#define vec_insert_elts_ha(V,E,N,M,H,A)					\
do {	\
int len = (int)vec_len(V);						\
	V = _vec_resize(P, V, (len + (N)) * (sizeof((V)[0])), H, A);	\
memmove((V)+(M)+(N), (V)+(M),						\
	(len - (M)) * sizeof((V)[0]));						\
memcpy(V + (M), E, (N) * sizeof((V)[0]));			\
} while (0)
#define vec_insert_elts(V,E,N,M)	vec_insert_elts_ha(V,E,N,M,0,0)

#define vec_insert_elts_aligned(V,E,N,M,A) vec_insert_elts_ha(V,E,N,M,0,A)

		/** \brief Delete N elements starting at element M
			@param V pointer to a vector
			@param N number of elements to delete
			@param M first element to delete
			@return V (value-result macro parameter)
		*/
#define vec_delete(V,N,M)					\
do {
	int len = (int)vec_len(V);
if (len - (N)-(M) > 0) {
	memove((V)+(M), (V)+(M)+(N),
		len - (N)-(M) * sizeof(V[0]));
	_vec_len((V)) -= (N);
}

} while (0)

//#define vec_del1(v,i)				\


//#define vec_append(v1,v2)						\
//
//#define vec_append_aligned(v1,v2,align)	\

#define vec_prepend(v1,v2) 

#define vec_zero(var)	\

#define vec_set(v,val)		\

#define vec_is_equal(v1,v2) \

#define vec_cmp(v1,v2)		\

//#define vec_search(v,E)		\




#endif /** _VEC_INCLUDE_H_ */