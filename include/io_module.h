#ifndef _IO_MODULE_INCLUDE_H_
#define _IO_MODULE_INCLUDE_H_

#include<stdint.h>
#include<gmp.h>

extern cmt_thread_context_t;

/**	\brief contains template for the various 10Gps pkt I/O library that can be adopted
	-------------------------------------------------------------------------------------------
	@param load_module() used to set system-wide i/o module initialization
	@param init_handle() used to initialize the driver library. It also use the context to 
	                     create /initialize a private packet I/O data structure
	@param link_devices() used to add links to tcp stack
	@param release_ptr() release the packet if it does not need to process
	@param get_wptr() retrieve the next empty pkt buffer for the application for packet writing.
						Returns ptr to pkt_buffer
	@param send_pkts() transmit batch of packet via interface idx(=nif)
	@param get_rptr() retrieve next pkt	for application for packet read
					  return ptr to pkt buffer
	@param recv_pkts() recieve batch of packets from the interface, ifidx.
						return no. of packets that are read from the iface
	@param select() for blocking i/o
	@param destory_handle free up resources allocated during init_handle(). Normally called during
					processing termination
	@dev_ioctl() contains submodules for select drivers
*/
typedef struct io_module_func {
	void (*load_module)	(void);
	void (*init_handle)	(cmt_thread_context_t* ctx);
	uint8_t* (*link_devices) (cmt_thread_context_t* ctx);
	void (*release_pkt) (cmt_thread_context_t* ctx, int ifidx, unsigned char* pkt_data, int len);
	int8_t* (*get_wptr) (cmt_thread_context_t* ctx, int ifidx, uint16_t len);
	int32_t (*send_pkts) (cmt_thread_context_t* ctx, int nif);
	int8_t* (*get_rptr) (cmt_thread_context_t* ctx, int ifidx, int index, uint16_t* len);
	int32_t(*recv_pkts) (cmt_thread_context_t* ctx, int ifidx);
	int32_t(*select) (cmt_thread_context_t* ctx);
	void (*destory_handle) (cmt_thread_context_t* ctx);
	int32_t(*dev_ioctl) (cmt_thread_context_t* ctx, int nif, int cmd, void* arg);
} io_module_func_t;

#endif /** _IO_MODULE_INCLUDE_H_ */