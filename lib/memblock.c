/*
 * Joowon
 *	--------
 * |		|
 * |		|
 * |		|
 * |		|
 *	--------
 *
 * */

#include <stdlib.h>
#include <stdio.h>
#include <infiniband/verbs.h>

#include <sys/mman.h>


#include "memblock.h"
#include "common.h"
#include "ib.h"


struct memblock *alloc_memblock(const short id)
{
	struct memblock *mb = (struct memblock*)malloc(sizeof(*mb));

	if(!mb){
		rdma_error("Unable to alloc  mb");
		return NULL;
	}
	
	INIT_LIST_HEAD(&mb->list);
	mb->memblock_id =id ;
	mb->buffer = NULL;
	mb->mr = NULL;

	mb->buffer= mmap(NULL, MEMBLOCK_SIZE, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS,
			0,0);

	if(!mb->buffer){
		rdma_error("Unable to mmap mb buffer");
		free(mb);
		return NULL;
	}

	return mb;
}

bool set_memblock(struct memblock *mb,struct ibv_pd *pd)
{

	mb->mr = ibv_reg_mr(pd, mb->buffer, MEMBLOCK_SIZE,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);

	if(!mb->mr){
		rdma_error("Unable to reg mb mr");
		return false;
	}
	return true;

}


void memblock_die(struct memblock *mb)
{

	ibv_dereg_mr(mb->mr);
	INIT_LIST_HEAD(&mb->list);
	munmap(mb->mr,MEMBLOCK_SIZE);
	free(mb);

}

bool add_memblock(struct rdma_memory_handler_t *rmh, const short id)
{
	if(rmh->memblocks[id]){
		return true;
	}

	struct memblock *mb = alloc_memblock(id);
	if(!mb){
		return false;
	}
	if(!set_memblock(mb,rmh->rdma->pd)){
		return false;
	}
	list_add_tail(&mb->list,&rmh->memblock_list);
	rmh->memblocks[id] = mb;

	return true;
}
