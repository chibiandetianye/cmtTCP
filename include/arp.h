#ifndef _ARP_INCLUDE_H_
#define _ARP_INCLUDE_H_

#include<stdint.h>

#define MAX_ARPENTRY 1024

/** \brief initialize the coniguration of arp table */
int init_ARP_table();

/** \brief get local host hardware address through ip address 
	from global configuration
	-----------------------------------------------------------------------------
	@param ip ip address
*/
unsigned char* get_HW_addr(uint32_t ip);

/** \brief get destination host hardware address through ip address in arp table
	-----------------------------------------------------------------------------
	@param dip destination host ip address
	@param is_gateway whether the tag is gate way
*/
unsigned char* get_destination_HW_addr(uint32_t dip, uint8_t is_gateway);

/**	\brief request a arp packet 
	-----------------------------------------------------------------------------
	@param ip destination host ip address
	@param nif no. nic interface
	@param cur_ts curent timestamp
*/
void request_ARP(uint32_t ip, int nif, uint32_t cur_ts);

/**	\brief process a arp packet
	-----------------------------------------------------------------------------
	@param cur_ts current timestamp
	@param ifidx no. nic interface
	@param pkt_data packet
	@param the length of len
*/
int process_ARP_packet(uint32_t cur_ts,
	const int ifidx, unsigned char* pkt_data, int len);

/**
*/
void ARP_timer(uint32_t cur_ts);

void print_ARP_table();


#endif /** _ARP_INCLUDE_H_ */
