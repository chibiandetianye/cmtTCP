#ifndef _ICMP_INCLUDE_H_
#define _ICMP_INCLUDE_H_

#include<stdint.h>

#include"global.h"

/** \brief structure of icmph packet format */
typedef struct icmphdr {
	uint8_t  icmp_type;
	uint8_t  icmp_code;
	uint16_t icmp_checksum;
	union {
		struct {
			uint16_t icmp_id;
			uint16_t icmp_sequence;
		} echo;                     // ECHO | ECHOREPLY
		struct {
			uint16_t unused;
			uint16_t nhop_mtu;
		} dest;                     // DEST_UNREACH
	} un;
}_packed icmphdr;

/* getters and setters for ICMP fields */
#define icmp_echo_get_id(icmph)          (icmph->un.echo.icmp_id)
#define icmp_echo_get_seq(icmph)         (icmph->un.echo.icmp_sequence)
#define icmp_dest_unreach_get_mtu(icmph) (icmph->un.dest.nhop_mtu)

#define icmp_echo_set_id(icmph, id)      (icmph->un.echo.icmp_id = id)
#define icmp_echo_set_seq(icmph, seq)    (icmph->un.echo.icmp_sequence = seq)

/**	\brief request a icmp packet 
	@param saddr source ip address
	@param daddr destination ip address
	@param icmp_id id of icmp packet
	@param icmp_seq sequence of icmp packet
	@param icmpd option data
	@param len length of option data
*/
void
request_ICMP(uint32_t saddr, uint32_t daddr,
	uint16_t icmp_id, uint16_t icmp_seq,
	uint8_t* icmpd, uint16_t len);

/**	\brief process the icmp packet 
	@param iph ip packet
	@len 
*/
int
process_ICMP_packet(struct iphdr* iph, int len);

/* ICMP types */
#define ICMP_ECHOREPLY      0   /* Echo Reply               */
#define ICMP_DEST_UNREACH   3   /* Destination Unreachable  */
#define ICMP_SOURCE_QUENCH  4   /* Source Quench            */
#define ICMP_REDIRECT       5   /* Redirect (change route)  */
#define ICMP_ECHO           8   /* Echo Request             */
#define ICMP_TIME_EXCEEDED  11  /* Time Exceeded            */
#define ICMP_PARAMETERPROB  12  /* Parameter Problem        */
#define ICMP_TIMESTAMP      13  /* Timestamp Request        */
#define ICMP_TIMESTAMPREPLY 14  /* Timestamp Reply          */
#define ICMP_INFO_REQUEST   15  /* Information Request      */
#define ICMP_INFO_REPLY     16  /* Information Reply        */
#define ICMP_ADDRESS        17  /* Address Mask Request     */
#define ICMP_ADDRESSREPLY   18  /* Address Mask Reply       */


#endif /** _ICMP_INCLUDE_H_ */