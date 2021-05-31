#ifndef _SOCKET_INCLUDE_H_
#define _SOCKET_INCLUDE_H_

#include<stdint.h>
#include<sys/socket.h>

#include"tcp.h"

typedef struct cmt_socket_map {
	int id;
	int socket_type;
	uint32_t opts;

	struct sockaddr_in s_addr;

	union {
		cmt_tcp_stream_t* stream;
		cmt_tcp_listener_t* listener;
	};

	uint32_t epoll;
	uint32_t events;
	uint64_t ep_data;


} cmt_socket_map_t;

enum cmt_socket_opts {
	CMT_TCP_NONBLOCK = 0x01,
	CMT_TCP_ADDR_BIND = 0x02,
};

cmt_socket_map_t* cmt_allocate_socket(int socktype, int need_lock);
void cmt_free_socket(int sockid, int need_lock);
cmt_socket_map_t* cmt_get_socket(int sockid);

typedef struct cmt_socket {
	int id;
	int socktype;

	uint32_t opts;
	struct sockaddr_in s_addr;

	union {
		cmt_tcp_stream_t* stream;
		cmt_tcp_stream_t* listener;
		void* ep;
	};
	struct cmt_socket_table* socktable;
} cmt_socket_t;

typedef struct cmt_socket_table {
	size_t max_fds;
	int cur_idx;
	cmt_socket_t** sockfds;
	unsigned char* open_fds;
} cmt_socket_table_t;

cmt_socket_t* cmt_socket_allocate(int socktype);

void cmt_socket_free(int sockid);

cmt_socket_t* cmt_socket_get(int sockid);

cmt_socket_table_t* cmt_socket_init_fdtable(void);

int cmt_socket_close_listening(int sockid);

int cmt_socket_close_stream(int sockid);

#endif /** _SOCKET_INCLUDE_H_ */