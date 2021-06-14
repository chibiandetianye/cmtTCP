#include<stdio.h>
#include<stdint.h>

#include"tcp_stream.h"
#include"debug.h"

cmt_tcp_stream_t*
create_tcp_stream(cmttcp_manager_per_cpu_t* m, cmt_socket_map_t* socket, int type,
		uint32_t saddr, uint16_t sport, uint32_t daddr, uint16_t dport) {
	cmt_tcp_stream_t* stream = NULL;

	stream = get_stream(m->stream_manager);
	if (unlikely(stream == NULL)) {
		trace_error(stderr, m->ctx->cpu, m->ctx->tid, "bad alloc in stream pool");
		return NULL;
	}

	stream->sndvar = snd_get(m->send_manager);
	if (unlikely(stream->sndvar == NULL)) {
		free_stream(m->stream_manager, stream);
		trace_error(stderr, m->ctx->cpu, m->ctx->tid, "bad alloc in send pool")
		return NULL;
	}

	stream->recvvar = recv_get(m->recv_manager);
	if (unlikely(stream->recvvar == NULL)) {
		free_snd(m->send_manager, stream->sndvar);
		free_stream(m->stream_manager, stream);
		trace_error(stderr, m->ctx->cpu, m->ctx->tid, "bad alloc in recv pool");
		return NULL;
	}

	stream->saddr = saddr;
	stream->sport = sport;
	stream->daddr = daddr;
	stream->dport = dport;

	int ret = stream_ht_insert(m->tcp_flow_table, stream);

	stream->stream_type = type;
	stream->state = NTY_TCP_LISTEN;
	stream->on_rto_idx = -1;

	stream->snd->ip_id = 0;
	stream->snd->mss = TCP_DEFAULT_MSS;
	stream->snd->wscale_mine = TCP_DEFAULT_WSCALE;
	stream->snd->wscale_peer = 0;
	stream->snd->nif_out = 0;

	stream->snd->iss = rand_r(&next_seed) % TCP_MAX_SEQ;
	stream->rcv->irs = 0;

	stream->snd_nxt = stream->snd->iss;
	stream->snd->snd_una = stream->snd->iss;
	stream->snd->snd_wnd = CMT_SEND_BUFFER_SIZE;

	stream->rcv_nxt = 0;
	stream->rcv->rcv_wnd = TCP_INITIAL_WINDOW;
	stream->rcv->snd_wl1 = stream->rcv->irs - 1;

	stream->snd->rto = TCP_INITIAL_RTO;

	uint8_t* sa = (uint8_t*)&stream->saddr;
	uint8_t* da = (uint8_t*)&stream->daddr;

	printf("CREATED NEW TCP STREAM %d: "
		"%u.%u.%u.%u(%d) -> %u.%u.%u.%u(%d) (ISS: %u)\n", stream->id,
		sa[0], sa[1], sa[2], sa[3], ntohs(stream->sport),
		da[0], da[1], da[2], da[3], ntohs(stream->dport),
		stream->snd->iss);

	return stream;

}


void 
stream_destroy(cmttcp_manager_per_cpu_t* m, cmt_tcp_stream_t* stream) {

}