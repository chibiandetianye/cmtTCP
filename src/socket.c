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

