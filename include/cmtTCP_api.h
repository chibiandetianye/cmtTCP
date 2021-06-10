#ifndef _CMTTCP_API_INCLUDE_H_
#define _CMTTCP_APT_INCLUDE_H_

#include<sys/types.h>
#include<sys/socket.h>

int cmt_socket(int domain, int type, int protocol);

int cmt_bind(int sockid, const struct sockaddr* addr, socklen_t addrlen);

int cmt_listen(int sockid, int backlog);

int cmt_accept(int sockid, struct sockaddr* addr, socklen_t* addrlen);

ssize_t cmt_recv(int sockid, char* buf, size_t len, int flags);

ssize_t cmt_send(int sockid, const char* buf, size_t len);

ssize_t cmt_close(int sockid);

void cmt_tcp_init(void);


#endif /** _CMTTCP_API_INCLUDE_H_ */