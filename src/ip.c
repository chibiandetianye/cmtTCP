#include <string.h>
#include <netinet/ip.h>

#include"ip.h"
#include"cmtTCP.h"

int
process_IPv4_packet(uint32_t cur_ts,
	const int ifidx, unsigned char* pkt_data, int len)
{
	/* check and process IPv4 packets */
	struct iphdr* iph = (struct iphdr*)(pkt_data + sizeof(struct ethhdr));
	int ip_len = ntohs(iph->tot_len);
	int rc = -1;

	/* drop the packet shorter than ip header */
	if (ip_len < sizeof(struct iphdr))
		return ERROR;

	if (mtcp->iom->dev_ioctl != NULL)
		rc = mtcp->iom->dev_ioctl(mtcp->ctx, ifidx, PKT_RX_IP_CSUM, iph);
	if (rc == -1 && ip_fast_csum(iph, iph->ihl))
		return -1;

#if !PROMISCUOUS_MODE
	/* if not promiscuous mode, drop if the destination is not myself */
	if (iph->daddr != config.eths[ifidx].ip_addr)
		//DumpIPPacketToFile(stderr, iph, ip_len);
		return 0;
#endif

	// see if the version is correct
	if (iph->version != 0x4) {
		mtcp->iom->release_pkt(mtcp->ctx, ifidx, pkt_data, len);
		return FALSE;
	}

	switch (iph->protocol) {
	case IPPROTO_TCP:
		return process_TCP_packet(cur_ts, ifidx, iph, ip_len);
	case IPPROTO_ICMP:
		return process_ICMP_packet(iph, ip_len);
	default:
		/* currently drop other protocols */
		return FALSE;
	}
	return FALSE;
}

inline int
get_output_interface(uint32_t daddr, uint8_t* is_external)
{
	int nif = -1;
	int i;
	int prefix = 0;

	*is_external = 0;
	/* Longest prefix matching */
	for (i = 0; i < CONFIG.routes; i++) {
		if ((daddr & CONFIG.rtable[i].mask) == CONFIG.rtable[i].masked) {
			if (CONFIG.rtable[i].prefix > prefix) {
				nif = CONFIG.rtable[i].nif;
				prefix = CONFIG.rtable[i].prefix;
			}
			else if (CONFIG.gateway) {
				*is_external = 1;
				nif = (CONFIG.gateway)->nif;
			}
			break;
		}
	}

	if (nif < 0) {
		uint8_t* da = (uint8_t*)&daddr;
		TRACE_ERROR("[WARNING] No route to %u.%u.%u.%u\n",
			da[0], da[1], da[2], da[3]);
		assert(0);
	}

	return nif;
}

uint8_t*
IP_output_standalone(struct mtcp_manager* mtcp, uint8_t protocol,
	uint16_t ip_id, uint32_t saddr, uint32_t daddr, uint16_t payloadlen)
{
	struct iphdr* iph;
	int nif;
	unsigned char* haddr, is_external;
	int rc = -1;

	nif = get_output_interface(daddr, &is_external);
	if (nif < 0)
		return NULL;

	haddr = get_destination_HWaddr(daddr, is_external);
	if (!haddr) {
#if 0
		uint8_t* da = (uint8_t*)&daddr;
		TRACE_INFO("[WARNING] The destination IP %u.%u.%u.%u "
			"is not in ARP table!\n",
			da[0], da[1], da[2], da[3]);
#endif
		requestARP(mtcp, (is_external) ? ((CONFIG.gateway)->daddr) : daddr,
			nif, mtcp->cur_ts);
		return NULL;
	}

	iph = (struct iphdr*)ethernet_output(mtcp,
		ETH_P_IP, nif, haddr, payloadlen + IP_HEADER_LEN);
	if (!iph) {
		return NULL;
	}

	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + payloadlen);
	iph->id = htons(ip_id);
	iph->frag_off = htons(IP_DF);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = protocol;
	iph->saddr = saddr;
	iph->daddr = daddr;
	iph->check = 0;

#ifndef DISABLE_HWCSUM	
	if (mtcp->iom->dev_ioctl != NULL) {
		switch (iph->protocol) {
		case IPPROTO_TCP:
			rc = mtcp->iom->dev_ioctl(mtcp->ctx, nif, PKT_TX_TCPIP_CSUM_PEEK, iph);
			break;
		case IPPROTO_ICMP:
			rc = mtcp->iom->dev_ioctl(mtcp->ctx, nif, PKT_TX_IP_CSUM, iph);
			break;
		}
	}
	/* otherwise calculate IP checksum in S/W */
	if (rc == -1)
		iph->check = ip_fast_csum(iph, iph->ihl);
#else
	UNUSED(rc);
	iph->check = ip_fast_csum(iph, iph->ihl);
#endif

	return (uint8_t*)(iph + 1);
}
/*----------------------------------------------------------------------------*/
uint8_t*
IP_output(struct mtcp_manager* mtcp, tcp_stream* stream, uint16_t tcplen)
{
	struct iphdr* iph;
	int nif;
	unsigned char* haddr, is_external = 0;
	int rc = -1;

	if (stream->sndvar->nif_out >= 0) {
		nif = stream->sndvar->nif_out;
	}
	else {
		nif = get_output_interface(stream->daddr, &is_external);
		stream->sndvar->nif_out = nif;
		stream->is_external = is_external;
	}

	haddr = get_destination_HWaddr(stream->daddr, stream->is_external);
	if (!haddr) {
#if 0
		uint8_t* da = (uint8_t*)&stream->daddr;
		TRACE_INFO("[WARNING] The destination IP %u.%u.%u.%u "
			"is not in ARP table!\n",
			da[0], da[1], da[2], da[3]);
#endif
		/* if not found in the arp table, send arp request and return NULL */
		/* tcp will retry sending the packet later */
		RequestARP(mtcp, (stream->is_external) ? (CONFIG.gateway)->daddr : stream->daddr,
			stream->sndvar->nif_out, mtcp->cur_ts);
		return NULL;
	}

	iph = (struct iphdr*)EthernetOutput(mtcp, ETH_P_IP,
		stream->sndvar->nif_out, haddr, tcplen + IP_HEADER_LEN);
	if (!iph) {
		return NULL;
	}

	iph->ihl = IP_HEADER_LEN >> 2;
	iph->version = 4;
	iph->tos = 0;
	iph->tot_len = htons(IP_HEADER_LEN + tcplen);
	iph->id = htons(stream->sndvar->ip_id++);
	iph->frag_off = htons(0x4000);	// no fragmentation
	iph->ttl = 64;
	iph->protocol = IPPROTO_TCP;
	iph->saddr = stream->saddr;
	iph->daddr = stream->daddr;
	iph->check = 0;

#ifndef DISABLE_HWCSUM
	/* offload IP checkum if possible */
	if (mtcp->iom->dev_ioctl != NULL) {
		switch (iph->protocol) {
		case IPPROTO_TCP:
			rc = mtcp->iom->dev_ioctl(mtcp->ctx, nif, PKT_TX_TCPIP_CSUM_PEEK, iph);
			break;
		case IPPROTO_ICMP:
			rc = mtcp->iom->dev_ioctl(mtcp->ctx, nif, PKT_TX_IP_CSUM, iph);
			break;
		}
	}
	/* otherwise calculate IP checksum in S/W */
	if (rc == -1)
		iph->check = ip_fast_csum(iph, iph->ihl);
#else
	UNUSED(rc);
	iph->check = ip_fast_csum(iph, iph->ihl);
#endif
	return (uint8_t*)(iph + 1);
}

