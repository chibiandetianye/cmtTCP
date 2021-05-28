//
// Created by chimt on 2021/5/28.
//

#ifndef CMT_RING_NETGRAH_H
#define CMT_RING_NETGRAH_H

#include<stdint.h>


#include"queue.h"

typedef enum sched_stat {
    POLLING,
    INTERRUPT,
    DISABLE
} sched_stat;

#define NETGRAPH_NAME_MAX 512
#define DEFAULT_WORKING_FLOW 4

typedef struct net_node_base {
    list_node_t * next;
    char name[NETGRAPH_NAME_MAX];

} net_node_base_t;

typedef struct netgraph {
    char name[NETGRAPH_NAME_MAX];

    uint32_t num_cores;
    uint32_t num_working_flow;

    net_node_base_t ** graph;
}netgraph_t;

netgraph_t*
netgraph_create();

#endif //CMT_RING_NETGRAH_H

