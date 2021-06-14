#ifndef _TCP_STREAM_INCLUDE_H_
#define _TCP_STREAM_INCLUDE_H_

#include"tcp.h"
#include"cmtTCP.h"

cmt_tcp_stream_t* create_tcp_stream(cmttcp_manager_per_cpu_t* m, cmt_socket_map_t* socket, int type,
	uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport);

void stream_destroy(cmttcp_manager_per_cpu_t* m, cmt_tcp_stream_t* stream);

#endif /** _TCP_STREAM_INCLUDE_H_ */