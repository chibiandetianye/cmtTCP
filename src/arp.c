#include<stdint.h>
#include<pthread.h>
/** for user space rcu */
#include<urcu/arch.h>
#include<urcu-pointer.h>
#include<urcu.h>
#include<urcu-qsbr.h>

#include"arp.h"
#include"queue.h"
#include"config.h"
#include"cmtTCP.h"
#include"rss.h"
#include"global.h"
#include"cmt_errno.h"
#include"atomic.h"
#include"cmt_memory_pool.h"

#define ARP_PAD_LEN 18			/* arp pad length to fit 64B packet size */
#define ARP_TIMEOUT_SEC 1		/* 1 second arp timeout */

struct arphdr
{
	uint16_t ar_hrd;			/* hardware address format */
	uint16_t ar_pro;			/* protocol address format */
	uint8_t ar_hln;				/* hardware address length */
	uint8_t ar_pln;				/* protocol address length */
	uint16_t ar_op;				/* arp opcode */

	uint8_t ar_sha[ETH_ALEN];	/* sender hardware address */
	uint32_t ar_sip;			/* sender ip address */
	uint8_t ar_tha[ETH_ALEN];	/* targe hardware address */
	uint32_t ar_tip;			/* target ip address */

	uint8_t pad[ARP_PAD_LEN];
} _packed;

/** \brief record arp request queue
*/
typedef struct arp_queue_entry {
	volatile slist_node_t* arp_link;	/* next arp request queue element */
	uint32_t ip; /*	destination ip requested */
	int nif_out;	/* no. nic interface out */
	uint32_t ts_out;	/* time recorded at first request */

	volatile uint8_t flag _cache_aligned; /* whether mark expire */
} arp_queue_entry_t;

enum arp_hrd_format
{
	arp_hrd_ethernet = 1
};

enum arp_opcode
{
	arp_op_request = 1,
	arp_op_reply = 2,
};

/** \brief structure of managing the request list */
typedef struct arp_manager
{
	arp_queue_entry_t* working_list;
	//pthread_spinlock_t lock;
} arp_manager_t;

extern cmttcp_config_t config;

arp_manager_t g_arpm;

/** \brief init arp table and arp manager */
int
init_ARP_table()
{
	config.arp.entries = 0;
	size_t i;

	cmt_pool_t* pool_ = get_cmt_pool();
	config.arp.entry = (arp_entry_t*)
		cmt_pcalloc(pool_, sizeof(arp_entry_t) * MAX_ARPENTRY);
		
	if (unlikely(config.arp.entry == NULL)) {
		perror("calloc");
		cmt_errno = ENONMEM;
		return -1;
	}

	g_arpm.working_list = cmt_palloc(pool, sizeof(arp_queue_entry_t));
	if (unlikely(g_arpm.working_list)) {
		cmt_errno = ENONMEM;
		free(config.arp.entry);
		return -1;
	}
	g_arpm.working_list->arp_link = NULL;
	
	return 0;
}

/** get hardware address through ip address */
unsigned char*
get_HW_addr(uint32_t ip)
{
	int i;
	unsigned char* haddr = NULL;
	for (i = 0; i < config.eths_num; i++) {
		if (ip == config.eths[i].ip_addr) {
			haddr = config.eths[i].haddr;
			break;
		}
	}

	return haddr;
}

/** get destination hardware in arp table through destination address */
unsigned char*
get_destination_HW_addr(uint32_t dip, uint8_t is_gateway)
{
	unsigned char* d_haddr = NULL;
	int prefix = 0;
	int i;

	if (is_gateway == 1 && config.arp.gateway)
		d_haddr = (config.arp.gateway)->haddr;
	else {
		/* Longest prefix matching */
		for (i = 0; i < config.arp.entries; i++) {
			if (config.arp.entry[i].prefix == 1) {
				if (config.arp.entry[i].ip == dip) {
					d_haddr = config.arp.entry[i].haddr;
					break;
				}
			}
			else {
				if ((dip & config.arp.entry[i].ip_mask) ==
					config.arp.entry[i].ip_masked) {

					if (config.arp.entry[i].prefix > prefix) {
						d_haddr = config.arp.entry[i].haddr;
						prefix = config.arp.entry[i].prefix;
					}
				}
			}
		}
	}

	return d_haddr;
}

/** \brief send a arp message */
static int
ARP_output(int nif, int opcode, uint32_t dst_ip, 
	unsigned char* dst_haddr, unsigned char* target_haddr)
{
	if (!dst_haddr)
		return -1;

	/* Allocate a buffer */
	struct arphdr* arph = (struct arphdr*)EthernetOutput(
		ETH_P_ARP, nif, dst_haddr, sizeof(struct arphdr));
	if (!arph) {
		return -1;
	}
	/* Fill arp header */
	arph->ar_hrd = htons(arp_hrd_ethernet);
	arph->ar_pro = htons(ETH_P_IP);
	arph->ar_hln = ETH_ALEN;
	arph->ar_pln = 4;
	arph->ar_op = htons(opcode);

	/* Fill arp body */
	int edix = config.nif_to_eidx[nif];
	arph->ar_sip = config.eths[edix].ip_addr;
	arph->ar_tip = dst_ip;

	memcpy(arph->ar_sha, config.eths[edix].haddr, arph->ar_hln);
	if (target_haddr) {
		memcpy(arph->ar_tha, target_haddr, arph->ar_hln);
	}
	else {
		memcpy(arph->ar_tha, dst_haddr, arph->ar_hln);
	}
	memset(arph->pad, 0, ARP_PAD_LEN);

#if DBGMSG
	DumpARPPacket(mtcp, arph);
#endif

	return 0;
}

/** \brief register the arp entry */
int
register_ARP_entry(uint32_t ip, const unsigned char* haddr)
{
	int idx = FAAcs(&config.arp.entry_nums, 1);

	config.arp.entry[idx].prefix = 32;
	config.arp.entry[idx].ip = ip;
	memcpy(config.arp.entry[idx].haddr, haddr, ETH_ALEN);
	config.arp.entry[idx].ip_mask = -1;
	config.arp.entry[idx].ip_masked = ip;

	if (config.gateway && ((config.gateway)->daddr &
		config.arp.entry[idx].ip_mask) ==
		config.arp.entry[idx].ip_masked) {
		config.arp.gateway = &config.arp.entry[idx];
		//TRACE_CONFIG("ARP Gateway SET!\n");
	}

	barrier();
	config.arp.entry[idx].flag = 1;
	barrier();

	//TRACE_CONFIG("Learned new arp entry.\n");
	PrintARPTable();

	return 0;
}
/** \brief set a request in arpm wokring list */
int
request_ARP(uint32_t ip, int nif, uint32_t cur_ts)
{
	arp_queue_entry_t* ent;
	unsigned char haddr[ETH_ALEN];
	unsigned char taddr[ETH_ALEN];
	int success;
	arp_queue_entry_t* head = g_arpm.working_list;
	arp_queue_entry_t* old_head;
	cmt_pool* pool_;

	/* if the arp request is in progress, return */
	rcu_register_thread();
	rcu_read_lock();
	for (ent = rcu_derefence(head->arp_link)); 
		ent;
		ent = rcu_derefence(tailq_next((ent))) {
		if (ent->ip == ip && ent->flag) {
			rcu_read_unlock();
			rcu_quiescent_state();
			return;
		}
	}
	rcu_read_unlock();
	rcu_quiescent_state();
	rcu_unregister_thread();

	pool = get_cmt_pool();
	ent = (struct arp_queue_entry*)cmt_pcalloc(pool, sizeof(struct arp_queue_entry));
	if (unlikely(ent == NULL)) {
		cmt_errno = ENONMEM;
		return -1;
	}
	ent->ip = ip;
	ent->nif_out = nif;
	ent->ts_out = cur_ts;
	ent->flag = 1;
	
	do {
		old_head = head->arp_link;
		ent->arp_link = old_head;
		success = CASra(head->arp_link, old_head, ent);
	} while (success != 0)


	/* else, broadcast arp request */
	memset(haddr, 0xFF, ETH_ALEN);
	memset(taddr, 0x00, ETH_ALEN);
	ARP_output(nif, arp_op_request, ip, haddr, taddr);
	return 0;
}

/**	\brief process arp packet */
static int
process_ARP_request(mtcp_manager_t mtcp,
	struct arphdr* arph, int nif, uint32_t cur_ts)
{
	unsigned char* temp;

	/* register the arp entry if not exist */
	temp = get_destination_HW_addr(arph->ar_sip, 0);
	if (!temp) {
		RegisterARPEntry(arph->ar_sip, arph->ar_sha);
	}

	/* send arp reply */
	ARP_output(nif, arp_op_reply, arph->ar_sip, arph->ar_sha, NULL);

	return 0;
}

/**	\brief process arp reply */
static int
process_ARP_reply(struct arphdr* arph, uint32_t cur_ts)
{
	unsigned char* temp;
	struct arp_queue_entry* ent;
	struct arp_queue_entry* old, next;
	int success;

	/* register the arp entry if not exist */
	temp = get_destination_HW_addr(arph->ar_sip, 0);
	if (!temp) {
		register_ARP_entry(arph->ar_sip, arph->ar_sha);
	}

	for (ent = g_arpm.working_list; ent->arp_link; ent = ent->arp_link) {
		old = ent->arp_link;
		next = ent->arp_link->arp_link;
		if (ent->arp_link->ip == arph->ar_sip) {
			barrier();
			ent->arp_link->flag = 0;
			barrier();
			if ((success(CASra(ent->arp_link, old, next)) < 0) {
				return 0;
			}
			synchronize_rcu();
			free(old);
		}
	}

	return 0;
}

/** process arp packet */
int
process_ARP_packet(uint32_t cur_ts, const int ifidx, 
	unsigned char* pkt_data, int len)
{
	struct arphdr* arph = (struct arphdr*)(pkt_data + sizeof(struct ethhdr));
	int i, nif;
	int to_me = false;

	/* process the arp messages destined to me */
	for (i = 0; i < CONFIG.eths_num; i++) {
		if (arph->ar_tip == CONFIG.eths[i].ip_addr) {
			to_me = true;
		}
	}

	if (!to_me)
		return 0;

#if DBGMSG
	DumpARPPacket(mtcp, arph);
#endif

	switch (ntohs(arph->ar_op)) {
	case arp_op_request:
		nif = config.eths[ifidx].ifindex; // use the port index as argument
	    process_ARP_request(arph, nif, cur_ts);
		break;

	case arp_op_reply:
		process_ARP_reply(arph, cur_ts);
		break;

	default:
		break;
	}

	return 0;
}

/*----------------------------------------------------------------------------*/
/* ARPTimer: wakes up every milisecond and check the ARP timeout              */
/*           timeout is set to 1 second                                       */
/*----------------------------------------------------------------------------*/
void
ARP_timer(uint32_t cur_ts)
{
	arp_queue_entry_t* ent, * ent_tmp;

	/* if the arp requet is timed out, retransmit */
	pthread_mutex_lock(&g_arpm.lock);
	TAILQ_FOREACH_SAFE(ent, &g_arpm.list, arp_link, ent_tmp) {
		if (TCP_SEQ_GT(cur_ts, ent->ts_out + SEC_TO_TS(ARP_TIMEOUT_SEC))) {
			struct in_addr ina;
			ina.s_addr = ent->ip;
			TRACE_INFO("[CPU%2d] ARP request for %s timed out.\n",
				mtcp->ctx->cpu, inet_ntoa(ina));
			TAILQ_REMOVE(&g_arpm.list, ent, arp_link);
			free(ent);
		}
	}
	pthread_mutex_unlock(&g_arpm.lock);
}
/*----------------------------------------------------------------------------*/
void
PrintARPTable()
{
	int i;

	/* print out process start information */
	TRACE_CONFIG("ARP Table:\n");
	for (i = 0; i < CONFIG.arp.entries; i++) {

		uint8_t* da = (uint8_t*)&CONFIG.arp.entry[i].ip;

		TRACE_CONFIG("IP addr: %u.%u.%u.%u, "
			"dst_hwaddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
			da[0], da[1], da[2], da[3],
			CONFIG.arp.entry[i].haddr[0],
			CONFIG.arp.entry[i].haddr[1],
			CONFIG.arp.entry[i].haddr[2],
			CONFIG.arp.entry[i].haddr[3],
			CONFIG.arp.entry[i].haddr[4],
			CONFIG.arp.entry[i].haddr[5]);
	}
	if (CONFIG.arp.entries == 0)
		TRACE_CONFIG("(blank)\n");

	TRACE_CONFIG("----------------------------------------------------------"
		"-----------------------\n");
}
/*----------------------------------------------------------------------------*/
void
DumpARPPacket(mtcp_manager_t mtcp, struct arphdr* arph)
{
	uint8_t* t;

	thread_printf(mtcp, mtcp->log_fp, "ARP header: \n");
	thread_printf(mtcp, mtcp->log_fp, "Hardware type: %d (len: %d), "
		"protocol type: %d (len: %d), opcode: %d\n",
		ntohs(arph->ar_hrd), arph->ar_hln,
		ntohs(arph->ar_pro), arph->ar_pln, ntohs(arph->ar_op));
	t = (uint8_t*)&arph->ar_sip;
	thread_printf(mtcp, mtcp->log_fp, "Sender IP: %u.%u.%u.%u, "
		"haddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		t[0], t[1], t[2], t[3],
		arph->ar_sha[0], arph->ar_sha[1], arph->ar_sha[2],
		arph->ar_sha[3], arph->ar_sha[4], arph->ar_sha[5]);
	t = (uint8_t*)&arph->ar_tip;
	thread_printf(mtcp, mtcp->log_fp, "Target IP: %u.%u.%u.%u, "
		"haddr: %02X:%02X:%02X:%02X:%02X:%02X\n",
		t[0], t[1], t[2], t[3],
		arph->ar_tha[0], arph->ar_tha[1], arph->ar_tha[2],
		arph->ar_tha[3], arph->ar_tha[4], arph->ar_tha[5]);
}
/*----------------------------------------------------------------------------*/


