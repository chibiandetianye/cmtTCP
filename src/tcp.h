#ifndef _TCP_INCLUDE_H_
#define _TCP_INCLUDE_H_

#include<stdint.h>

#include"ring.h"
#include"tcp_stream_hash.h"

typedef enum cmt_tcp_state {
	CMT_TCP_CLOSE = 0,
	CMT_TCP_LISTEN = 1,
	CMT_TCP_SYN_SENT = 2,
	CMT_TCP_SYN_RCVD = 3,
	CMT_TCP_ESTABLISHED = 4,
	CMT_TCP_CLOSE_WAIT = 5,
	CMT_TCP_FIN_WAIT_1 = 6,
	CMT_TCP_FIN_CLOSING = 7,
	CMT_TCP_LAST_ACK = 8,
	CMT_TCP_FIN_WAIT_2 = 9,
	CMT_TCP_TIME_WAIT = 10,
} cmt_tcp_state_t;

#define CMT_TCPHDR_FIN		0x01
#define CMT_TCPHDR_SYN		0X02
#define CMT_TCPHDR_RST		0x04
#define CMT_TCPHDR_PSH		0x08
#define CMT_TCPHDR_ACK		0x10
#define CMT_TCPHDR_URG		0x20
#define CMT_TCPHDR_ECE		0x40
#define CMT_TCPHDR_CWR		0x80

#define CMT_TCPOPT_MSS_LEN			4
#define CMT_TCPOPT_WSCALE_LEN		3
#define CMT_TCPOPT_SACK_PERMIT_LEN	2
#define CMT_TCPOPT_SACK_LEN			10
#define CMT_TCPOPT_TIMESTAMPS_LEN	10

#define TCP_DEFAULT_MSS			1460
#define TCP_DEFAULT_WSCALE		7
#define TCP_INITIAL_WINDOW		14600
#define TCP_MAX_WINDOW			65535

#define CMT_SEND_BUFFER_SIZE	8192
#define CMT_RECV_BUFFER_SIZE	8192
#define CMT_TCP_TIMEWAIT		0
#define CMT_TCP_TMEOUT			30

#define TCP_MAX_RTX					16
#define TCP_MAX_SYN_RETRY			7
#define TCP_MAX_BACKOFF				7

#define TCP_SEQ_LT(a,b) 		((int32_t)((a)-(b)) < 0)
#define TCP_SEQ_LEQ(a,b)		((int32_t)((a)-(b)) <= 0)
#define TCP_SEQ_GT(a,b) 		((int32_t)((a)-(b)) > 0)
#define TCP_SEQ_GEQ(a,b)		((int32_t)((a)-(b)) >= 0)
#define TCP_SEQ_BETWEEN(a,b,c)	(TCP_SEQ_GEQ(a,b) && TCP_SEQ_LEQ(a,c))

#define HZ						1000
#define TIME_TICK				(1000000/HZ)		// in us
#define TIMEVAL_TO_TS(t)		(uint32_t)((t)->tv_sec * HZ + ((t)->tv_usec / TIME_TICK))

#define TS_TO_USEC(t)			((t) * TIME_TICK)
#define TS_TO_MSEC(t)			(TS_TO_USEC(t) / 1000)
#define MSEC_TO_USEC(t)			((t) * 1000)
#define USEC_TO_SEC(t)			((t) / 1000000)

#define TCP_INITIAL_RTO 		(MSEC_TO_USEC(500) / TIME_TICK)

enum tcp_option {
	TCP_OPT_END				= 0,
	TCP_OPT_NOP				= 1,
	TCP_OPT_MSS				= 2,
	TCP_OPT_WSCALE			= 3,
	TCP_OPT_SACK_PERMIT		= 4,
	TCP_OPT_SACK			= 5,
	TCP_OPT_TIMESTAMP		= 8
};

enum tcp_close_reason {
	TCP_NOT_CLOSED			= 0,
	TCP_ACTIVE_CLOSE		= 1,
	TCP_PASSIVE_CLOSE		= 2,
	TCP_CONN_FAIL			= 3,
	TCP_CONN_LOST			= 4,
	TCP_RESET				= 5,
	TCP_NO_MEM				= 6,
	TCP_NOT_ACCEPTED		= 7,
	TCP_TIMEOUT				= 8
};

enum ack_opt {
	ACK_OPT_NOW,
	ACK_OPT_AGGREGATE,
	ACK_OPT_WACK,
};

enum sock_type {
	CMT_TCP_SOCK_UNUSED,
	CMT_TCP_SOCK_STREAM,
	CMT_TCP_SOCK_LISTNER,
	CMT_TCP_SOCK_PROXY,
	CMT_TCP_SOCK_EPOLL,
	CMT_TCP_SOCK_PIPE,
};

typedef struct cmt_tcp_timestamp {
	uint32_t ts_val;
	uint32_t ts_ref;
} cmt_tcp_timestamp_t;

typedef struct cmt_tcp_stat {
	uint32_t tdp_ack_cnt;
	uint32_t tdp_ack_bytes;
	uint32_t ack_upd_cnt;
	uint32_t ack_upd_bytes;
	uint32_t rto_cnt;
	uint32_t rto_bytes;
} cmt_tcp_stat_t;

typedef struct cmt_tcp_recv {
	/** receiver variable */
	uint32_t rcv_wnd; /** recieve window (unscaled) */
	uint32_t irs; /** initial receiving sequence */
	uint32_t snd_wl1; /** segment seq number for last window update */
	uint32_t snd_wl2; /** segment ack number for last window update */

	/** variable for fast retransmission */
	uint8_t dup_acks; /** number of duplicated acks*/
	uint32_t last_ack_seq; /** highest ack seq */
	
	/** timestamp */
	uint32_t ts_recent;	/** recent peer timestamp */
	uint32_t ts_lastack_rcvd; /** last ack rcvd time */
	uint32_t ts_last_ts_upd; /** last peer ts update time */
	uint32_t ts_tw_expire;  /** timestamp for timewait expire */

	/** RTT estimation variable */
	uint32_t srtt; /** smoothed round trip time */
	uint32_t mdev; /** medium deviation */
	uint32_t mdev_max; /** maximal mdev for the last rtt period */
	uint32_t rttvar; /** smoothed mdev_max */
	uint32_t rtt_seq; /** sequence number to used */

	cmt_ring_t* rcvbuf;

	cmt_tcp_stream_t* h_next;

} cmt_tcp_recv_t;

typedef struct cmt_tcp_send {
	hash_bucket_t* h_next;

	/** ip-level information */
	uint16_t ip_id; 

	uint16_t mss; /** maximum segment size */
	uint16_t eff_mss; /** effecient segment size (excluding tcp option) */

	uint8_t wscale_mine; /** my window scale (adertising window) */
	uint8_t wscale_peer; /** peer's window scale (advertising window) */
	int8_t nif_out; /** cached output network interface */
	unsigned char* d_haddr; /** cached destination MAC address */

	/** send sequence variable */
	uint32_t snd_una; /** send unacknowledged */
	uint32_t snd_wnd; /** send window (unscaled) */
	uint32_t peer_wnd; /** client window size */
	uint32_t iss;	/** intial sending sequence */
	uint32_t fss; /** final sending sequence */

	/** retransmission timeout variables */
	uint8_t nrtx; /** number of retransmission */
	uint8_t max_nrtx; /** max number of retransmission */
	uint32_t rto; /** retransmission  timeout */
	uint32_t rs_rto; /** timestamp for retransmision  timeout */

	/** congestion control variable */
	uint32_t cwnd;		/** congestion window */
	uint32_t ssthresh;  /** slow start threshold */

	/** timestamp */
	uint32_t ts_lastack_sent; /** last ack sent time */

	uint8_t is_wack : 1, /** is ack for window advertisement */
		ack_cnt : 8;  /** number of acks to send. max 64*/
	
	uint8_t on_control_list; 
	uint8_t on_send_list;
	unit8_t on_ack_list;
	uint8_t on_sendq;
	unit8_t on_ackq;
	unit8_t on_closeq;
	uint8_t on_resetq;

	uint8_t on_closeq_int : 1,
		on_resetq_int : 1,
		is_fin_sent : 1,
		is_fin_ackd : 1;

	cmt_ring_t* sndbuf;

} cmt_tcp_send_t;

typedef struct cmt_tcp_stream {
	cmt_socket_t* socket;
	hash_bucket_t* next; //next tcp stream in hash table


	uint32_t id : 24,
		stream_type : 8;

	uint32_t saddr; /** in network order */
	uint32_t daddr; /** in network order */
	uint16_t sport; /** in network order */
	uint16_t dport; /** in network order */

	uint8_t state; /** tcp state */
	uint8_t close_reason; /** close reason */
	uint8_t on_hash_table; 
	uint8_t on_timewait_list;
	uint8_t ht_idx;
	uint8_t closed;
	uint8_t is_bound_addr;
	uint8_t need_wnd_adv;
	int16_t on_rto_idx;

	uint16_t on_timeout_list : 1,
		on_rcv_br_list : 1,
		on_snd_br_list : 1,
		saw_timestamp : 1, /** whether peer sends timestamp */
		sack_permit : 1, /** whether peer permits SACK */
		control_list_waiting : 1,
		have_reset : 1,
		is_external : 1,	/** the peer node is locate outside lan */
		wait_for_acks : 1; /** if true, the sender should wait for acks to catch up before sending again */

	uint32_t snd_nxt; /** send next */
	uint32_t rcv_nxt; /** receive next */
	
	cmt_tcp_send_t* sndvar;
	cmt_tcp_recv_t* rcvvar;
} cmt_tcp_stream_t;

typedef struct cmt_tcp_listener {
	int sockid;

	cmt_tcp_listener* ht_next;
} cmt_tcp_listener_t;

typedef struct cmt_tcp_manager {

} cmt_tcp_manager_t;


#endif /** _TCP_INCLUDE_H_ */