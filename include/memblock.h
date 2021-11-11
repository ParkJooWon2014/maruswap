
#ifndef __MEMBLOCK_H__
#define __MEMBLOCK_H__

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "list_head.h"
#include "ib.h"

struct rdma_memory_handler_t;
enum{
//	MEMBLOCK_EXTRA =  sizeof(char)*(1UL << (30-12)),
	MEMBLOCK_REAL_SIZE = (1UL << 30),
	MEMBLOCK_SIZE = (MEMBLOCK_REAL_SIZE ), // + MEMBLOCK_EXTRA),

};

struct memblock
{
	short memblock_id ;

	void *buffer;
	struct ibv_mr *mr;

	struct list_head list;	
};

void memblock_die(struct memblock *mb);
struct memblock *alloc_memblock(const short id);
bool set_memblock(struct memblock *mb,struct ibv_pd *pd);

bool add_memblock(struct rdma_memory_handler_t *rmh, const short id);

#endif 
