#ifndef CGRAPH_LOCKORDER_H
#define CGRAPH_LOCKORDER_H
#include "graph/graph.h"
#include "query/traverse.h"
#include <stdbool.h>

typedef struct {
    uint32_t lock_a;
    uint32_t lock_b;
    uint32_t func_id;  /* function where this order was observed */
} LockOrderPair;

typedef struct {
    LockOrderPair *pairs;
    uint32_t count;
    bool has_cycle;  /* potential deadlock detected */
} LockOrderResult;

LockOrderResult query_lock_order(const Graph *g);
void lock_order_result_free(LockOrderResult *r);

#endif
