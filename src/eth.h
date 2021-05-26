#ifndef _ETH_INCLUDE_H_
#define _ETH_INCLUDE_H_

#include<stdint.h>

int
ProcessPacket(mtcp_manager_t mtcp, const int ifidx,
	uint32_t cur_ts, unsigned char* pkt_data, int len);

#define MAX_SEND_PCK_CHUNK 64

uint8_t*
EthernetOutput(struct mtcp_manager* mtcp, uint16_t h_proto,
	int nif, unsigned char* dst_haddr, uint16_t iplen);


#endif /** _ETH_INCLUDE_H_ */