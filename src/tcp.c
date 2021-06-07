#include"tcp.h"
#include"cmtTCP.h"
#include"global.h"

always_inline uint16_t 
cmt_calculate_option(uint8_t flags) {
	uint16_t optlen = 0;

	if (flags & NTY_TCPHDR_SYN) {
		optlen += NTY_TCPOPT_MSS_LEN;
		optlen += NTY_TCPOPT_TIMESTAMP_LEN;
		optlen += 2;
		optlen += NTY_TCPOPT_WSCALE_LEN + 1;
	}
	else {
		optlen += NTY_TCPOPT_TIMESTAMP_LEN;
		optlen += 2;
	}
	assert(optlen % 4 == 0);
	return optlen;
}

always_inline uint16_t 
cmt_tcp_calculate_checksum(uint16_t* buf, uint16_t len, uint32_t saddr, uint32_t daddr)
{
	uint32_t sum;
	uint16_t* w;
	int nleft;

	sum = 0;
	nleft = len;
	w = buf;

	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	// add padding for odd length
	if (nleft)
		sum += *w & ntohs(0xFF00);

	// add pseudo header
	sum += (saddr & 0x0000FFFF) + (saddr >> 16);
	sum += (daddr & 0x0000FFFF) + (daddr >> 16);
	sum += htons(len);
	sum += htons(PROTO_TCP);

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	sum = ~sum;

	return (uint16_t)sum;
}

always_inline void
cmt_tcp_generate_timestamp(cmt_tcp_stream_t* cur_stream, uint8_t* tcpopt, uint32_t cur_ts) {
	uint32_t* ts = (uint32_t*)(tcpopt + 2);

	tcpopt[0] = TCP_OPT_TIMESTAMP;
	tcpopt[1] = CMT_TCPOPT_TIMESTAMPS_LEN;

	ts[0] = htonl(cur_ts);
	ts[1] = htonl(cur_stream->rcv->ts_recent);
}

static void 
cmt_tcp_generate_options(cmt_tcp_stream_t* cur_stream, uint32_t cur_ts,
	uint8_t flags, uint8_t* tcpopt, uint16_t optlen) {

	int i = 0;

	if (flags &	CMT_TCPHDR_SYN) {
		uint16_t mss = cur_stream->sndvar->mss;

		tcpopt[i++] = TCP_OPT_MSS;
		tcpopt[i++] = CMT_TCPOPT_MSS_LEN;
		tcpopt[i++] = mss >> 8;
		tcpopt[i++] = mss % 256;

		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;

		nty_tcp_generate_timestamp(cur_stream, tcpopt + i, cur_ts);
		i += CMT_TCPOPT_TIMESTAMPS_LEN;

		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_WSCALE;
		tcpopt[i++] = CMT_TCPOPT_WSCALE_LEN;

		tcpopt[i++] = cur_stream->sndvar->wscale_mine;

	}
	else {
		tcpopt[i++] = TCP_OPT_NOP;
		tcpopt[i++] = TCP_OPT_NOP;
		nty_tcp_generate_timestamp(cur_stream, tcpopt + i, cur_ts);
		i += CMT_TCPOPT_TIMESTAMPS_LEN;
	}

	assert(i == optlen);
}

always_inline uint16_t 
cmt_calculate_chksum(uint16_t* buf, uint16_t len, uint32_t saddr, uint32_t daddr) {
	uint32_t sum;
	uint16_t* w;
	int nleft;

	sum = 0;
	nleft = len;
	w = buf;

	while (nleft > 1)
	{
		sum += *w++;
		nleft -= 2;
	}

	// add padding for odd length
	if (nleft)
		sum += *w & ntohs(0xFF00);

	// add pseudo header
	sum += (saddr & 0x0000FFFF) + (saddr >> 16);
	sum += (daddr & 0x0000FFFF) + (daddr >> 16);
	sum += htons(len);
	sum += htons(PROTO_TCP);

	sum = (sum >> 16) + (sum & 0xFFFF);
	sum += (sum >> 16);

	sum = ~sum;

	return (uint16_t)sum;
}

always_inline cmt_sender_t* 
cmt_tcp_getsender(cmt_tcp_manager_per* tcp, nty_tcp_stream* cur_stream) {
	return tcp->sender;
}

