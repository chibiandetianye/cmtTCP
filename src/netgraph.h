#ifndef _NETGRAPH_INCLUDE_H_
#define _NETGRAPH_INCLUDE_H_

#include"queue.h"

typedef struct netgraph {
	list_node_t* next_graph;
	const char* name; /** unique type name */

} netgraph_t;