#include<stdio.h>

#include"socket.h"
#include"cmtTCP.h"
#include"global.h"
#include"tcp.h"
#include"atomic.h"
#include"cmt_errno.h"

extern cmttcp_manager_t* get_cmttcp_manager();

cmt_socket_map_t* cmt_allocate_socket(int socktype) {
	cmttcp_manager_t* tcpm = get_cmttcp_manager();
	cmt_socket_t* dummy_node = tcpm->cmt_socket_map_list.dummy;
	cmt_socket_t* socket = NULL;
	if (unlikely(tcpm == NULL)) {
		return NULL;
	}

	do {
		socket = dummy.free_smap_link;
		if (unlikely(socket == NULL)) {
			printf("The connection socket are at maximum\n");
			return NULL;
		}
		success = CASra(dummy.free_smap_link, socket, socket->free_smap_link);
	} while (unlikely(success != 0));

	socket->socket_type = socktype;
	socket->opts = 0;
	socket->stream = NULL;
	socket->epoll = 0;
	socket->events = 0;

	memset(&socket->s_addr, 0, sizeof(struct sockaddr_in));
	memset(&socket->ep_data, 0, sizeof(nty_epoll_data));

	return socket;
}

void 
cmt_socket_free(int sockid) {
	cmttcp_manager_t* tcpm = get_cmttcp_manager();
	cmt_socket_map_t* socket = &tcpm->sock_map[sockid];

	if (unlikely(socket->sock_type == CMT_TCP_SOCK_UNUSED)) {
		return;
	}

	socket->socket_type = CMT_TCP_SOCK_UNUSED;
	socket->epoll = 0;
	socket->events = 0;

	do {
		cmt_socket_t* old = dummy.free_smap_link;
		socket->free_smap_link = old;
		int success = CASra(dummy.free_smap_link, old, socket);
	} while (unlikely(success != 0));

}

cmt_socket_map_t* 
cmt_get_socket(int sockid) {
	if (sockid < 0 || sockid > CMT_MAX_CONCURRENCY) {
		cmt_errno = EBADF;
		return NULL;
	}

	cmttcp_manager_t* tcpm = get_cmttcp_manager();
	return &tcpm->sock_map[sockid];
}

cmt_socket_table_t* 
cmt_socket_allocate_fdtable(void) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_socket_table_t* sock_table = (cmt_socket_table_t*)cmt_pcalloc(pool, sizeof(cmt_socket_table_t));
	if (unlikely(socket_table == NULL)) {
		printf("%s[%d] failed\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	size_t size = sizeof(cmt_socket_t*) * CMT_SOCKFD_NR;
	sock_table->sockfds = cmt_palloc(pool, size);
	if (unlikely(sock_table->sockfds == NULL)) {
		printf("%s[%d] failed\n", __FUNCTION__, __LINE__);
		cmt_pfree(pool, sock_table, sizeof(cmt_socket_table_t));
		return NULL;
	}

	sock_table->max_fds = (CMT_SOCKFD_NR % CMT_BITS_PER_BYTES) ? CMT_SOCKFD_NR / CMT_BITS_PER_BYTES + 1 : 
		CMT_SOCKFD_NR / CMT_BITS_PER_BYTES;
	sock_table->open_fds = (uint8_t*)cmt_pcalloc(pool, sock_table->max_fds);
	if (unlikely(sock_table->open_fds == NULL)) {
		printf("%s[%d] failed\n", __FUNCTION__, __LINE__);
		cmt_pfree(pool, sock_tale->sockfds, size);
		cmt_pfree(pool, sock_table, sizeof(cmt_socket_table_t));
		return NULL;
	}

	return sock_table;
}

void
cmt_socket_free_fdtable(cmt_socket_table_t* fdtable) {
	cmt_pool_t* pool = get_cmt_pool();
	cmt_pfree(pool, fdtable->open_fds, sock_table->max_fds);
	cmt_pfree(pool, fdtable->sokckfds, sizeof(cmt_socket_t*) * CMT_SOCKFD_NR);
	cmt_pfree(pool, fdtable, sizeof(cmt_socket_table);
}

cmt_socket_table_t* 
cmt_socket_get_fdtable() {
	cmttcp_manager_t* tcpm = get_cmttcp_manager();
	return tcpm->fdtable;
}

cmt_socket_table_t* 
cmt_socket_init_fdtable() {
	return cmt_socket_allocate_fdtable();
}

int
cmt_socket_find_id(unsigned char* fds, int start, size_t max_fds) {
	size_t i = 0;
	size_t len = (max_fds - start) * (sizeof(char)) / sizeof(uint64_t);
	size_t remained = (max_fds - start) * (sizeof(char)) - len * sizeof(uint64_t);
	size_t startfds = (uint64_t*)(fds + start);
	for (i = 0; i < len; ++i) {
		uint64_t x = ~startfds[i];
		if (fds[i] != 0) {
			return i * sizeof(uint64_t) * CMT_BITS_PER_BYTES +
				start * CMT_BITS_PER_BYTES + log2_first_set(x);
		}
	}
	i = i * sizeof(uint64_t) + start;

	if (i == max_fds) {
		return -1;
	}
	for (; i < max_fds; ++i) {
		if (fds[i] != 0xFF) {
			break;
		}
	}

	if (i == max_fds) return -1;
	int j = 0;
	char bytes = fds[i];
	while (bytes % 2) {
		bytes /= 2;
		j++;
	}

	return i * CMT_BITS_PER_BYTES + j;
}

char 
cmt_socket_unuse_id(unsigned char* fds, size_t idx) {

	int i = idx / NTY_BITS_PER_BYTE;
	int j = idx % NTY_BITS_PER_BYTE;

	char byte = 0x01 << j;
	fds[i] &= ~byte;

	return fds[i];
}

int 
cmt_socket_set_start(size_t idx) {
	return idx / NTY_BITS_PER_BYTE;
}

char 
cmt_socket_use_id(unsigned char* fds, size_t idx) {

	int i = idx / NTY_BITS_PER_BYTE;
	int j = idx % NTY_BITS_PER_BYTE;

	char byte = 0x01 << j;

	fds[i] |= byte;

	return fds[i];
}


