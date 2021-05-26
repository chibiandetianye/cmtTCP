
#ifndef _QUEUE_INCLUDE_H_
#define _QUEUE_INCLUDE_H_

#include"global.h"

#ifdef QUEUE_MACRO_DEBUG
#define trashit(e) do{			\
(x) = (void*)-1;				\
} while(0)
#else
#define trashit(e)
#endif /** QUEUE_MACRO_DEBUG */

/** \brief Singly-linked List declarations.
 */
typedef struct slist_node{
	struct slist_node* sle_next;
} slist_node_t;

/** \brief Singly-linked List declarations.
 */
typedef struct slist_head{
	slist_node_t* slh_first;
} slist_head_t;

#define slist_head_initializer(head)	\
{NULL}

always_inline int
slist_empty(slist_head_t* head) {
	return head->slh_first == NULL;
}

#define slist_first(head)	((head)->slh_first)

#define slist_next(node)	((node)->sle_next)				\

#define slist_foreach(var, head, field)						\
	for ((var) = slist_first((head));						\
	    (var);												\
	    (var) = slist_next((head)))

#define slist_foreach_safe(var, head, field, tvar)			\
	for ((var) = slist_first((head));						\
	    (var) && ((tvar) = slist_next((var), field), 1);	\
	    (var) = (tvar))


#define	slist_foreach_prevptr(var, varp, head)		\
	for ((varp) = &slist_first((head));						\
	    ((var) = *(varp)) != NULL;							\
	    (varp) = &slist_next((var)))

#define slist_init(head) do {								\
slist_first(head) = NULL;									\
	} while(0)


always_inline void
slist_insert_after(slist_node_t* slistelm, slist_node_t* elm) {
	slist_next(elm) = slist_next(slistelm);
	slist_next(slistelm) = elm;
}

always_inline void 
slist_insert_head(slist_head_t* head, slist_node_t* elm) {
	slist_next(elm) = slist_first(head);
	slist_first(head) = elm;
}

always_inline void 
slist_remove_head(slist_head* head) {
	slist_first(head) = slist_next(slist_first(head));
}

always_inline void
slist_remove_after(slist_node_t* node) {
	slist_next(node) = slist_next(slist_next(node));
}

always_inline void
slist_remove(slist_head_t* head, slist_node_t* node) {
	if (slist_first(head) == node) {
		slist_remove_head(head);
	}
	else {
		slist_node_t* curelm = slist_first(head);
		while (slist_next(curelm) != node) {
			curelm = slist_next(curelm);
		}
		slist_remove_after(curelm);
	}
	trashit(elm->sle_next);
}


/** \brief Singly-linked Tail queue declarations.
 */
typedef struct stailq_head {
	slist_node_t*  stqh_first;
	slist_node_t** stqh_last;
} stailq_head_t;

always_inline int
stailq_empty(stailq_head_t* head) {
	return head->stqh_first == NULL;
}

#define stailq_head_initializer(head)				\
{NULL, &(head).stqh_first}

#define stailq_first(head)	((head)->stqh_first)
#define stailq_next(node)	((node)->slist_next)

always_inline slist_node_t *
stailq_last(stailq_head_t* head) {
	return stailq_empty(head) ? NULL : *(head->stqh_last);
}

always_inline void
stailq_init(stailq_head_t* head) {
	stailq_first(head) = NULL;
	head->stqh_last = &stailq_first(head);
}

#define	stailq_foreach(var, head)				\
	for((var) = stailq_first((head));					\
	   (var);											\
	   (var) = stailq_next((var)))


#define	stailq_foreach_safe(var, head, tvar)				\
	for ((var) = stailq_first((head));						\
	    (var) && ((tvar) = stailq_next((var)), 1);		\
	    (var) = (tvar))

always_inline void
stailq_concat(stailq_head_t* head1, stailq_head_t* head2) {
	if (!stailq_empty(head2)) {
		*(head1->stqh_last) = head2->stqh_first;
		head1->stqh_last = head2->stqh_last;
		stailq_init(head2);
	}
}

always_inline void
stailq_insert_after(staiq_head_t* head, slist_node_t* tqelm, slist_node_t* elm) {
	if ((stailq_next(elm) = stailq_next(tqelm)) == NULL) {
		head->stqh_last = &stailq_next(elm);
	}
	stailq_next(tqelm) = elm;
}

always_inline void
stailq_insert_head(stailq_head_t* head, slist_node_t* elm) {
	if ((stailq_next(elm) = stailq_next(head)) == NULL)	
		head->stqh_last = &stailq_next((elm);		
	stailq_first((head)) = (elm);
}

always_inline void
stailq_insert_tail(stailq_head_t* head, slist_node_t* elm) {
	stailq_next(elm) = NULL;				
	*(head)->stqh_last = elm;					
	head->stqh_last = &stailq_next(elm);			
}

always_inline void
stailq_remove_head(stailq_head_t* head) {
	if ((stailq_first(head) = stailq_next(stailq_first(head)) == NULL) {
		head->stqh_last = &stailq_first(head);
	}
}

always_inline void
stailq_remove_after(stailq_head_t* head, slist_node_t* node) {
	if ((stailq_next(node) = stailq_next(stailq_first(node)) == NULL) {
		head->stqh_last = &stailq_next(node);
	}
}

always_inline void
stailq_remove(stailq_head_t* head, slist_node_t* node) {
	if (stailq_first(head) == elm) {
		stailq_remove_head(head, field);					
	}														
	else {
		slist_node_t* curelm = stailq_first(head);			
		while(stailq_first(curelm) != elm)			
			curelm = stailq_next(curelm);			
		stailq_remove_after(head, curelm);			
	}														
}

typedef struct list_node{
	list_node_t* le_next;
	list_node_t** le_prev;
} list_node_t;

/** \brief list */
typedef struct list_head {
	list_node* lh_first;
}list_head_t;

always_inline int
list_empty(list_head_t* head) {
	return head->lh_first == NULL;
}

#define list_first(head)		((head)->lh_first)
#define list_next(node)			((node)->le_next)

#define list_foreach(var, head)		\
for((var) = list_first((head));		\
	(var);							\
	(var) = list_next((var)))

#define list_foreach_safe(var, head, tvar)	\
for((var) = list_first((head));				\
	(var)&&((tvar) =  list_next(var), 1);	\
	(var) = (tvar))

#define	list_init(head) do {									\
	list_first((head)) = NULL;									\
} while (0)

always_inline void
list_insert_after(list_node_t* listelm, list_node_t* elm) {
	if ((list_next(elm) = list_next(listelm, field)) != NULL)
		list_next(listelm).le_prev = & list_next(elm);							
	list_next(listelm) = elm;						
	elm->le_prev = &list_elm(listelm);		
}

always_inline void
list_insert_before(list_node_t* listelm, list_node_t* elm) {
	elm->le_prev = listelm->le_prev;			
	list_next(elm) = listelm;						
	*(listelm)->le_prev = elm;							
	(listelm)->le_prev = &list_next((elm));		
}

always_inline void
list_insert_head(list_head_t* list, list_node_t* elm) {
	if ((list_next(elm) = list_first(head)) != NULL)	
		list_first(head)->le_prev = &list_next(elm); 
		list_first(head) = elm;									
		elm->le_prev = &list_first(head);
}

always_inline void
list_remove(list_node_t* elm) {
	if (list_next(elm) != NULL)				
		list_next(elm)->le_prev = 
		(elm)->le_prev;						
		*(elm->le_prev) = list_next(elm);	
}

/** \brief tail queue */
typedef struct tailq_head {
	list_node_t* tqh_first;
	list_node_t** tqh_last;
} tailq_head_t;

#define tailq_head_initializer(head)	\
{NULL, &(head).tqh_first}

always_inline int
tailq_empty(tailq_head_t* head) {
	return head->tqh_first == NULL;
}

#define tailq_first(head)	((head)->tqh_first)
#define tailq_next(node)	((node)->le_next)
#define tailq_last(head)	(*(((tailq_head_t*)((head)->tqh_last))->tqh_last))
#define tailq_prev(elm)		(*(((tailq_head_t*)((elm)->tqe_prev))->tqh_last))

#define	tailq_foreach(var, head)						\
	for ((var) = tailq_first((head));					\
	    (var);											\
	    (var) = tailq_next((var)))

#define	tailq_foreach_safe(var, head, field, tvar)			\
	for ((var) = tailq_first((head));						\
	    (var) && ((tvar) = tailq_next((var)), 1);	\
	    (var) = (tvar))

#define	tailq_foreach_reverse(var, head)	\
	for ((var) = tailq_last((head));				\
	    (var);												\
	    (var) = tailq_prev((var)))

#define	tailq_foreach_reverse_safe(var, head, tvar)	\
	for ((var) = tailq_last((head), headname);			\
	    (var) && ((tvar) = tailq_prev((var)), 1);	\
	    (var) = (tvar))

always_inline void
tailq_init(tailq_head_t* head) {
	tailq_first(head) = NULL;
	(head)->tqh_last = &tailq_first(head);
}

always_inline void
tailq_insert_after(tailq_head_t* head, list_node_t* listelm, list_node_t* elm) {
	if ((tailq_next(elm) = tailq_next(listelm)) != NULL)
		tailq_next(elm)->tqe_prev = 
		& tailq_next(elm);					
	else {
		head->tqh_last = &tailq_next(elm);	
	}													
	tailq_next(listelm) = elm;				
	elm->tqe_prev = &tailq_next(listelm);		
}

always_inline void
tailq_insert_before(list_node_t* listelm, list_node_t* elm) {
	elm->tqe_prev = listelm->tqe_prev;		
	tailq_next(elm) = listelm;					
	*listelm->tqe_prev = elm;						
	listelm->tqe_prev = &tailq_next(elm);	
}

always_inline void
tailq_insert_head(list_head_t* head, list_node_t* elm) {
	if ((tailq_next(elm) = tailq_first(head)) != NULL)	
		tailq_first(head)->tqe_prev = 
		& tailq_next(elm);						
	else													
		head->tqh_last = &tailq_next(elm);		
		tailq_first(head) = elm;							
		elm->tqe_prev = &tailq_first(head);
}

always_inline void
tailq_insert_tail(list_head_t* head, list_node_t* elm) {
	tailq_next(elm) = NULL;						
	elm->tqe_prev = head->tqh_last;				
	*head->tqh_last = elm;								
	head->tqh_last = &tailq_next(elm);			
}

always_inline void
tailq_remove(list_head_t* head, list_node_t* elm) {
	if ((tailq_next(elm)) != NULL)			
		tailq_next(elm)->tqe_prev = 
		elm->tqe_prev;						
	else {
		head->tqh_last = elm->tqe_prev;
	}													
	*(elm)->tqe_prev = tailq_next(elm);	
}

#endif /** _QUEUE_INCLUDE_H_ */