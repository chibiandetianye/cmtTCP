#ifndef _IP_INCLUDE_H_
#define _IP_INCLUDE_H_

#include<stdint.h>

#include"global.h"

struct iphdr {
	unsigned char ihl : 4,
		version : 4;
	unsigned char tos;
	unsigned short tot_len;
	unsigned short id;
	unsigned short flag_off;
	unsigned char ttl;
	unsigned char protocol;
	unsigned short check;
	unsigned int saddr;
	unsigned int daddr;
} _packed;

/**	\brief process a ipv4 packet 
	--------------------------------------------------------
	@param cur_ts current timestamps
	@param ifidx no. nic interface
	@param pkt_data packet data which used to be processing
	@param len length of packet data
*/
int process_IPv4_packet(uint32_t cur_ts,
	const int ifidx, unsigned char* pkt_data, int len);


void forward_IPv4_packet(int nif_in, char* buf, int len);

/** \brief request a ip packet from other netwokr layer
	-------------------------------------------------------------
	@param protocol the protocol of data type of the payload part
	@param ip_id id of ip packet 
	@param saddr source ip address
	@param daddr destination ip address
	@param tcplen length of tcp packet
*/
uint8_t* IP_output_standalone(uint8_t protocol,uint16_t ip_id, 
	uint32_t saddr, uint32_t daddr, uint16_t tcplen);

uint8_t* IP_output(tcp_stream* stream, uint16_t tcplen);


#endif /** _IP_INCLUDE_H_ */