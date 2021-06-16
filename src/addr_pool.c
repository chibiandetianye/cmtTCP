#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<stdint.h>

#include"addr_pool.h"
#include"rss.h"
#include"queue.h"
#include"global.h"
#include"debug.h"

typedef struct addr_entry
{
	list_node_t* addr_link;
	struct sockaddr_in addr;
} addr_entry_t;

typedef struct addr_map
{
	addr_entry_t* addrmap[MAX_PORT];
} addr_map_t;

typedef struct addr_pool
{
	addr_entry_t* pool;		/* address pool */
	addr_map_t* mapper;		/* address map  */

	uint32_t addr_base;				/* in host order */
	int num_addr;					/* number of addresses in use */

	int num_entry;
	int num_free;
	int num_used;

#ifdef SPINLOCK_ADDR_POOL
	pthread_spinlock_t lock;
#else
	pthread_mutex_t lock;
#endif /** SPINLOCK_ADDR_LOCK */
	tailq_head_t* free_list;
	tailq_head_t* used_list;
} addr_pool_t;

#define get_list_node(n)	((list_node_t*)(n))
#define get_addr_node(n)	((addr_entry_t*)(n))

addr_pool_t*
create_address_pool(in_addr_t addr_base, int num_addr)
{
	struct addr_pool* ap;
	int num_entry;
	int i, j, cnt;
	in_addr_t addr;
	uint32_t addr_h;

	ap = (addr_pool_t*)calloc(1, sizeof(struct addr_pool));
	if (!ap)
		return NULL;

	/* initialize address pool */
	num_entry = num_addr * (MAX_PORT - MIN_PORT);
	ap->pool = (addr_entry_t*)calloc(num_entry, sizeof(addr_entry_t));
	if (unlikely(!ap->pool)) {
		trace_error(stderr, 
			"bad alloc: failed to get address pool from calloc\n");
		free(ap);
		return NULL;
	}

	/* initialize address map */
	ap->mapper = (struct addr_map*)calloc(num_addr, sizeof(struct addr_map));
	if (!ap->mapper) {
		trace_error(stderr,
			"bad alloc: failed to get mapper from calloc\n");
		free(ap->pool);
		free(ap);
		return NULL;
	}

	tailq_init(&ap->free_list);
	tailq_init(&ap->used_list);

#ifdef SPINLOCK_ADDR_POOL
	if (pthread_spin_init(&ap->lock, NULL)) {
#else
	if (pthread_mutex_init(&ap->lock, NULL)) {
#endif /* SPINLOCK_ADDR_POOL */
		trace_error(stderr,
			"failed to init mutex\n");
		free(ap->pool);
		free(ap);
		return NULL;
	}

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_lock(&ap->lock)
#else /* SPINLOCK_ADDR_POOL */
	pthread_mutex_lock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	ap->addr_base = ntohl(addr_base);
	ap->num_addr = num_addr;

	cnt = 0;
	for (i = 0; i < num_addr; i++) {
		addr_h = ap->addr_base + i;
		addr = htonl(addr_h);
		for (j = MIN_PORT; j < MAX_PORT; j++) {
			ap->pool[cnt].addr.sin_addr.s_addr = addr;
			ap->pool[cnt].addr.sin_port = htons(j);
			ap->mapper[i].addrmap[j] = &ap->pool[cnt];

			tailq_insert_tail(&ap->free_list, get_list_node(&ap->pool[cnt]));

			if ((++cnt) >= num_entry)
				break;
		}
	}
	ap->num_entry = cnt;
	ap->num_free = cnt;
	ap->num_used = 0;

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_unlock(&ap->lock);
#else
	pthread_mutex_unlock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	return ap;
}

addr_pool_t*
create_address_pool_per_core(int core, int num_queues,
	in_addr_t saddr_base, int num_addr, in_addr_t daddr, in_port_t dport)
{
	struct addr_pool* ap;
	int num_entry;
	int i, j, cnt;
	in_addr_t saddr;
	uint32_t saddr_h, daddr_h;
	uint16_t sport_h, dport_h;
	int rss_core;
//#if 0
//	uint8_t endian_check = (current_iomodule_func == &dpdk_module_func) ?
//		0 : 1;
//#else
//	uint8_t endian_check = FetchEndianType();
//#endif

	ap = (addr_pool_t*)calloc(1, sizeof(addr_pool_t));
	if (!ap) {
		trace_error(stderr,
			"bad alloc: failed to get address pool from calloc\n");
		return NULL;
	}

	/* initialize address pool */
	num_entry = (num_addr * (MAX_PORT - MIN_PORT)) / num_queues;
	ap->pool = (addr_entry_t*)calloc(num_entry, sizeof(addr_entry_t));
	if (!ap->pool) {
		trace_error(stderr,
			"bad alloc: failed to get mapper from calloc\n");
		free(ap);
		return NULL;
	}

	/* initialize address map */
	ap->mapper = (addr_map_t*)calloc(num_addr, sizeof(addr_map_t));
	if (!ap->mapper) {
		free(ap->pool);
		free(ap);
		return NULL;
	}

	tailq_init(&ap->free_list);
	tailq_init(&ap->used_list);

#ifdef SPINLOCK_ADDR_POOL
	if (pthread_spin_init(&ap->lock, NULL)) {
#else
	if (pthread_mutex_init(&ap->lock, NULL)) {
#endif /* SPINLOCK_ADDR_POOL */
		trace_error(stderr,
			"failed to init mutex\n");
		free(ap->pool);
		free(ap);
		return NULL;
	}

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_lock(&ap->lock)
#else /* SPINLOCK_ADDR_POOL */
	pthread_mutex_lock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	ap->addr_base = ntohl(saddr_base);
	ap->num_addr = num_addr;
	daddr_h = ntohl(daddr);
	dport_h = ntohs(dport);

	/* search address space to get RSS-friendly addresses */
	cnt = 0;
	for (i = 0; i < num_addr; i++) {
		saddr_h = ap->addr_base + i;
		saddr = htonl(saddr_h);
		for (j = MIN_PORT; j < MAX_PORT; j++) {
			if (cnt >= num_entry)
				break;

			sport_h = j;
			rss_core = GetRSSCPUCore(daddr_h, saddr_h, dport_h, sport_h, num_queues, endian_check);
			if (rss_core != core)
				continue;

			ap->pool[cnt].addr.sin_addr.s_addr = saddr;
			ap->pool[cnt].addr.sin_port = htons(sport_h);
			ap->mapper[i].addrmap[j] = &ap->pool[cnt];
			tailq_insert_tail(&ap->free_list, get_list_node(&ap->pool[cnt]));
			cnt++;
		}
	}

	ap->num_entry = cnt;
	ap->num_free = cnt;
	ap->num_used = 0;
	//fprintf(stderr, "CPU %d: Created %d address entries.\n", core, cnt);
	if (ap->num_entry < CONFIG.max_concurrency) {
		fprintf(stderr, "[WARINING] Available # addresses (%d) is smaller than"
			" the max concurrency (%d).\n",
			ap->num_entry, CONFIG.max_concurrency);
	}
#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_unlock(&ap->lock);
#else
	pthread_mutex_unlock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */
	return ap;
}

void
destroy_address_pool(addr_pool_t* ap)
{
	if (!ap)
		return;

	if (ap->pool) {
		free(ap->pool);
		ap->pool = NULL;
	}

	if (ap->mapper) {
		free(ap->mapper);
		ap->mapper = NULL;
	}

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_destroy(&ap->lock);
#else
	pthread_mutex_destroy(&ap->lock);
#endif
	free(ap);
}

int
fetch_address(addr_pool_t* ap, int core, int num_queues,
	const struct sockaddr_in* daddr, struct sockaddr_in* saddr)
{
	list_node_t* walk, * next;
	addr_entry_t* walk_value;
	int rss_core;
	int ret = -1;
#if 0
	uint8_t endian_check = (current_iomodule_func == &dpdk_module_func) ?
		0 : 1;
#else
	uint8_t endian_check = FetchEndianType();
#endif

	if (!ap || !daddr || !saddr)
		return -1;

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_lock(&ap->lock);
#else
	pthread_mutex_lock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	walk = tailq_first(&ap->free_list);
	while (walk) {
		next = tailq_next(walk, addr_link);
		walk_value = get_addr_node(walk);

		if (saddr->sin_addr.s_addr != INADDR_ANY &&
			walk_value->addr.sin_addr.s_addr != saddr->sin_addr.s_addr) {
			walk = next;
			continue;
		}

		if (saddr->sin_port != INPORT_ANY &&
			walk_value->addr.sin_port != saddr->sin_port) {
			walk = next;
			continue;
		}

		rss_core = GetRSSCPUCore(ntohl(walk_value->addr.sin_addr.s_addr),
			ntohl(daddr->sin_addr.s_addr), ntohs(walk_value->addr.sin_port),
			ntohs(daddr->sin_port), num_queues, endian_check);

		if (core == rss_core)
			break;

		walk = next;
	}

	if (walk) {
		walk_value = walk;
		*saddr = walk_value->addr;
		tailq_remove(&ap->free_list, walk);
		tailq_insert_tail(&ap->used_list, walk);
		ap->num_free--;
		ap->num_used++;
		ret = 0;
	}

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_unlock(&ap->lock);
#else
	pthread_mutex_unlock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */
	return ret;
}

int
fetch_address_per_core(addr_pool_t* ap, int core, int num_queues,
	const struct sockaddr_in* daddr, struct sockaddr_in* saddr)
{
	list_node_t* walk;
	adddr_entry_t* walk_value;
	int ret = -1;

	if (!ap || !daddr || !saddr)
		return -1;

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_lock(&ap->lock);
#else
	pthread_mutex_lock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	/* we don't need to calculate RSSCPUCore if mtcp_init_rss is called */
	walk = tailq_first(&ap->free_list);
	if (walk) {
		walk_value = get_addr_node(walk);
		*saddr = walk_value->addr;
		tailq_remove(&ap->free_list, walk);
		tailq_insert_tail(&ap->used_list, walk);
		ap->num_free--;
		ap->num_used++;
		ret = 0;
	}

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_unlock(&ap->lock);
#else
	pthread_mutex_unlock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	return ret;
}

int
free_address(addr_pool_t* ap, const struct sockaddr_in* addr)
{
	addr_entry_t* walk, * next;
	list_node_t* walk_value;

	int ret = -1;

	if (!ap || !addr)
		return -1;

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_lock(&ap->lock);
#else
	pthread_mutex_lock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	if (ap->mapper) {
		walk_value = get_addr_node(walk);
		uint32_t addr_h = ntohl(addr->sin_addr.s_addr);
		uint16_t port_h = ntohs(addr->sin_port);
		int index = addr_h - ap->addr_base;

		if (index >= 0 && index < ap->num_addr) {
			walk = ap->mapper[addr_h - ap->addr_base].addrmap[port_h];
		}
		else {
			walk = NULL;
		}

	}
	else {
		walk = tailq_first(&ap->used_list);
		walk_value = get_addr_node(walk);
		while (walk) {
			next = tailq_next(walk);
			if (addr->sin_port == walk_value->addr.sin_port &&
				addr->sin_addr.s_addr == walk_value->addr.sin_addr.s_addr) {
				break;
			}

			walk = next;
		}

	}

	if (walk) {
		tailq_remove(&ap->used_list, walk);
		tailq_insert_tail(&ap->free_list, walk);
		ap->num_free++;
		ap->num_used--;
		ret = 0;
	}

#ifdef SPINLOCK_ADDR_POOL
	pthread_spin_unlock(&ap->lock);
#else
	pthread_mutex_unlock(&ap->lock);
#endif /** SPINLOCK_ADDR_POOL */

	return ret;
}
