#ifndef _IP_INCLUDE_H_
#define _IP_INCLUDE_H_

#include<stdint.h>

int pocess_IPv4_packet(uint32_t cur_ts,
	const int ifidx, unsigned char* pkt_data, int len);

void forward_IPv4_packet(int nif_in, char* buf, int len);

uint8_t* IP_output_standalone(uint8_t protocol,uint16_t ip_id, 
	uint32_t saddr, uint32_t daddr, uint16_t tcplen);

uint8_t* IPOutput(tcp_stream* stream, uint16_t tcplen);


#endif /** _IP_INCLUDE_H_ */