#include<stdio.h>
#include<stdint.h>
#include<stdlib.h>
#include<unistd.h>
#include<assert.h>

#include<linux/if_ether.h>
#include<linux/tcp.h>
#include<netinet/ip.h>

#include"eth.h"
#include"cmtTCP.h"
#include"debug.h"

int 
process_packet(const int ifidx, uint32_t cur_ts, unsigned char* pkt_data,
	int len) {
	struct ethhdr* ethh = (struct ethhdr*)pkt_data;
	unsigned short ip_proto = ntohs(ethh->h_proto);
	int ret;

	if (ip_proto == ETH_P_IP) {
		/* process ipv4 packet */
		ret = process_IPv4_packet(cur_ts, ifidx, pkt_data, len);

	}
	else if (ip_proto == ETH_P_ARP) {
		process_ARP_packet(cur_ts, ifidx, pkt_data, len);
		return 0;

	}
	else {
		//DumpPacket(mtcp, (char *)pkt_data, len, "??", ifidx);
		mtcp->iom->release_pkt(mtcp->ctx, ifidx, pkt_data, len);
		return 0;
	}

	return ret;
}

uint8_t*
ethernet_output(uint16_t h_proto,
	int nif, unsigned char* dst_haddr, uint16_t iplen) {
	uint8_t* buf;
	struct ethhdr* ethh;
	int i, eidx;

	/*
	 * -sanity check-
	 * return early if no interface is set (if routing entry does not exist)
	 */
	if (nif < 0) {
		trace_message(stdout, "No interface set!\n");
		return NULL;
	}

	eidx = config.nif_to_eidx[nif];
	if (eidx < 0) {
		trace_message(stdout"No interface selected!\n");
		return NULL;
	}
	buf = mtcp->iom->get_wptr(mtcp->ctx, eidx, iplen + ETHERNET_HEADER_LEN);
	if (!buf) {
		//TRACE_DBG("Failed to get available write buffer\n");
		return NULL;
	}
	//memset(buf, 0, ETHERNET_HEADER_LEN + iplen);

	ethh = (struct ethhdr*)buf;
	for (i = 0; i < ETH_ALEN; i++) {
		ethh->h_source[i] = config.eths[eidx].haddr[i];
		ethh->h_dest[i] = dst_haddr[i];
	}
	ethh->h_proto = htons(h_proto);

	return (uint8_t*)(ethh + 1);
}

