#ifndef _ADDR_POOL_INCLUDE_H_
#define _ADDR_POOL_INCLUDE_H

#include<netinet/in.h>

#include"queue.h"

#define MIN_PORT (1025)
#define MAX_PORT (65535 + 1)

extern addr_pool_t;


/* \brief Create address pool for given address range.                        
  ----------------------------------------------------------------------------
   @param addr_base the base address in network order.                        
   @param num_addr number of addresses to use as source IP                    
   @return address pool
*/

addr_pool_t* create_address_pool(in_addr_t addr_base, int num_addr);

/*	\brief Create address pool only for the given core number.                        
   ----------------------------------------------------------------------------
   All addresses and port numbers should be in network order. each has only 
   a address pool
   ----------------------------------------------------------------------------
   @param core Need to create the core of the address pool

*/
addr_pool_t* create_address_pool_per_core(int core, int num_queues,
	in_addr_t saddr_base, int num_addr, in_addr_t daddr, in_port_t dport);

/**	\brief Destory address pool
	----------------------------------------------------------------------------
	@param pointer to address pool needed to destroy
*/
void destroy_address_pool(addr_pool_t* ap);

/**	\brief Fetch address
	----------------------------------------------------------------------------
	@param ap pointer to address pool
	@param core get the address on the thread of the current cpu
	@param num_queues nums of cpu threads
	@param daddr address of destination
	@param saddr address of source
	@return 0 if cucessd of -1 if failed
*/
int fetch_address(addr_pool_t* ap, int core, int num_queues,
	const struct sockaddr_in* daddr, struct sockaddr_in* saddr);

int fetch_address_per_core(addr_pool_t* ap, int core, int num_queues,
	const struct sockaddr_in* daddr, struct sockaddr_in* saddr);

/**	\brief free address
*/
int free_address(addr_pool_t* ap, const struct sockaddr_in* addr);



#endif /** _ADDR_POOL_INCLUDE_H_ */