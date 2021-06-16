#ifndef _ETH_INCLUDE_H_
#define _ETH_INCLUDE_H_

#include<stdint.h>

/**	\brief Receive data packets from the network card, 
	process them at the data link layer and send them 
	to the upper layer for protocol analysis
	--------------------------------------------------
	@param ifidx Network card interface number
	@param cur_ts Current time stamps
	@param pkt_data Data to be process
	@param len length of pkt_data
	@return 0 if sucess -1 if failed
*/
int
process_packet(const int ifidx, uint32_t cur_ts, 
	unsigned char* pkt_data, int len);

#define MAX_SEND_PCK_CHUNK 64

/**	\brief Obtain the sent data packet and perform 
	data link layer processing
	-----------------------------------------------------
	@param h_proto 
	@param nif
	@param dst_haddr
	@param iplen
	@return 
*/
uint8_t*
ethernet_output(uint16_t h_proto,
	int nif, unsigned char* dst_haddr, uint16_t iplen);


#endif /** _ETH_INCLUDE_H_ */