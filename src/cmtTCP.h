#ifndef _CMTTCP_INCLUDE_H_
#define _CMTTCP_INCLUDE_H_

#include"queue.h"

#ifndef CORE_NUMS
#define CORE_NUMS 4
#endif /** CORE_NUMS */

#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif /** ETH_ALEN */

typedef struct eth_table {
	char dev_name[128];
	int ifindex;
	int stat_print;
	unsigned char haddr[ETH_ALEN];
	uint32_t netmask;
	//	unsigned char dst_haddr[ETH_ALEN];
	uint32_t ip_addr;
}eth_table_t;

typedef struct arp_table {
	tailq_head_t* entry;
	tailq_head_t* gateway;
} arp_table_t;


typedef struct cmttcp_config {
	/** network interface config */
	eth_table_t* eths;
	int* nif_to_eidx; //mapping physics port indexes to that of the configure port-list;
	int eth_nums;

	/** route config */

	/** arp config */
	arp_table_t* arp;

	int core_nums;
	int working_core_nums;
	int rss_core_nums;

} cmttcp_config_t;

typedef struct cmttcp_manager {

} cmttcp_manager_t;

#endif /** _CMTTCP_INCLUDE_H_ */