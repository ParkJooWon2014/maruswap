


#ifndef __RPC_H_
#define __RPC_H_

#include "common.h"
#include <stdio.h>
#include <infiniband/verbs.h>
#include "list_head.h"
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

enum{

	RELEASE_BUFFER = 0x0,
	READY_BUFFER = 0x3,

	OFFSET_BIT = 28,
	OPCODE_BIT = (32 - OFFSET_BIT),

	RPC_NR_BUFFER = (1UL << 10),
	RPC_MAX_EACH_SIZE = 64,
	RPC_BUFFER_SIZE = RPC_MAX_EACH_SIZE* RPC_NR_BUFFER,

	RDMA_OPCODE_OPEN = 0x1,
	RDMA_OPCODE_CLOSE = 0x2,
	RDMA_OPCODE_TRANSITION = 0x4,
	RDMA_OPCODE_CHECK = 0x5,
	RDMA_OPCODE_COMMIT = 0x3,

	MULTICAST_OPCODE_FLOW = 0x1,
	MULTICAST_OPCODE_CHECK = 0x2,
	MULTICAST_OPCODE_COMMIT =0x3,
	MULTICAST_OPCODE_NONE = 0xf,
};

struct recv_work {
	
	unsigned long id;
	size_t size;
	void *buffer;

	struct ibv_sge sg_list;
	struct ibv_recv_wr wr;

	unsigned long private;
	struct list_head list;
	struct ibv_wc wc;

	bool convey;
};

struct send_work {
	volatile bool done;

	struct ibv_sge sg_list;
	struct ibv_send_wr wr;
};

struct work_completion_t{
	struct ibv_wc wc;
	struct list_head list;
};

struct rdma_commt_t{
	uint32_t header;
};


struct rdma_open_t{
	uintptr_t remote_addr;
	uint32_t rkey;
};

void init_binder(void);
int __get_binder(void);
void __put_binder(int index);
void *set_buffer(char *buffer);
bool set_header(uint32_t *header, const int opcode, size_t offset);
bool interpret_header(const uint32_t header, int *opcode, int *nr_buffer, uint32_t *offset);

void rdma_event_handler(struct rdma_cm_event *event);
void stop_handler(void);


void *set_ready_buffer(char *buffer);
void *set_release_buffer(char *buffer);
bool set_header(uint32_t *header, const int opcode, size_t offset);
bool interpret_header(const uint32_t header, int *opcode, int *nr_buffer, uint32_t *offset);
bool ib_rpc(struct rdma_cm_id *id, int opcode, void *req, size_t req_size, void *res, size_t res_size, struct ibv_mr *mr);
bool ib_multicast_rpc(struct rdma_cm_id *id, int opcode, void *req, 
		size_t req_size, void *res, size_t res_size, struct ibv_mr *mr,
		struct ibv_ah *ah, uint32_t remote_qpn, uint32_t remote_qkey);

static inline unsigned int ib_get_opcode(struct ibv_wc *wc)
{
	return (wc->imm_data & 0xf0000000) >> 28;
}

static inline void __ib_convey_page(void *src, void *dst)
{
	memcpy(dst,src,PAGE_SIZE);
}
#endif
