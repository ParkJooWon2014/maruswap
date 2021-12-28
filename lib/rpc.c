/*Joowon*/



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#include "common.h"
#include "rpc.h"
#include "atomic.h"

void ib_putback_recv_work(struct ibv_qp *qp, struct recv_work *rw);

enum {
    NR_BINDERS = 256,
};

struct binder {
    atomic_t flags;
    volatile void *private;
};

struct binder binders[NR_BINDERS];

void init_binder(void)
{
    for (int i = 0; i < NR_BINDERS; i++) {
        atomic_set(&binders[i].flags, 0);
    }
}

int __get_binder(void)
{
    int index;
    int old;
    do {
        index = rand() % NR_BINDERS;
        old = atomic_cmpxchg(&binders[index].flags, 0, 1);
    } while (old != 0);

    return index;
}

void __put_binder(int index)
{
    assert(atomic_read(&binders[index].flags) == 2);
    atomic_set(&binders[index].flags, 0);
}

void *set_ready_buffer(char *buffer)
{
	*buffer = (*buffer | READY_BUFFER);
	return buffer;
}

void *set_release_buffer(char *buffer)
{
	*buffer = (*buffer & RELEASE_BUFFER);
	return buffer;
}

bool set_header(uint32_t *header, const int opcode, size_t offset)
{
	*header = 0x0;
	offset = (offset >> 12);
	*header = (opcode << 28);
	*header |= (offset & 0xfffffff);
	return true;
}

bool interpret_header(const uint32_t header, int *opcode, int *nr_buffer, uint32_t *offset)
{
  *opcode = (header >> OFFSET_BIT);
    if(*opcode >= 32 || *opcode < 0){
        printf("Unable to opcode");
        return false;
    }
    size_t off = ((header & 0xfffffff));
	*nr_buffer = ((off >> (18)) & 0xfff);
	*offset = off & 0x3ffff; 
	//- (*nr_buffer << 18);
	//size_t offsett = off - (*nr_buffer << 18)
	//*offset &= 0x1ffff;
	//debug(" %x %x %lx\n",*nr_buffer, *offset, off&0x1ffff);
    return true;
}

bool ib_rpc(struct rdma_cm_id *id, int opcode, void *req, size_t req_size, void *res, size_t res_size, struct ibv_mr *mr)
{
	volatile unsigned int done = 0;
	
	int binder = __get_binder();
	struct ibv_sge sge = {
		.addr = (uintptr_t)req,
		.length = req_size,
		.lkey = 0,
	};
	struct ibv_send_wr wr = {
		.opcode = IBV_WR_SEND_WITH_IMM,
		.send_flags = 0,
		.sg_list = &sge,
		.num_sge = 1,
		.imm_data = (opcode | binder),
		.wr_id = (uintptr_t)&done,
	};
	struct ibv_send_wr *bad_wr = NULL;
	struct recv_work *rw;

	if (!mr) {
		wr.send_flags |= IBV_SEND_INLINE;
	} else {
		sge.lkey = mr->lkey;
	}

	wr.send_flags |= IBV_SEND_SIGNALED;

	ibv_post_send(id->qp, &wr, &bad_wr);
	assert(!bad_wr);
	
	while (atomic_read(&binders[binder].flags) != 2) {
		struct ibv_wc wc;
		int bi;
		int nr_completed;

		nr_completed = ibv_poll_cq(id->recv_cq, 1, &wc);
		if (nr_completed == 0) {
			continue;
		} else if (nr_completed < 0) {
			return false;
		}

		bi = (wc.imm_data & 0xff);

		binders[bi].private = (void *)wc.wr_id;
		atomic_set(&binders[bi].flags, 2);
	}

	rw = (void *)binders[binder].private;
	memcpy(res, rw->buffer, res_size);

	ib_putback_recv_work(id->qp, rw);

	__put_binder(binder);

	while (mr && !done) {
		struct ibv_wc wc;
		int nr_completed;
		nr_completed = ibv_poll_cq(id->send_cq, 1, &wc);
		if (nr_completed == 0) {
			continue;
		} else if (nr_completed < 0) {
			break;
		}
		if (wc.status != IBV_WC_SUCCESS) {
			printf("work completion status %s\n",
					ibv_wc_status_str(wc.status));
			return false;
		}
		*((unsigned int *)wc.wr_id) = 1;
	}
	
	return true;
}


bool ib_multicast_rpc(struct rdma_cm_id *id, int opcode, void *req, 
		size_t req_size, void *res, size_t res_size, struct ibv_mr *mr,
		struct ibv_ah *ah, uint32_t remote_qpn, uint32_t remote_qkey)
{


	struct recv_work *rw ;
	int binder = __get_binder();
	struct ibv_sge sge = {
		.addr = (uintptr_t)req,
		.length = req_size,
		.lkey = 0,
	};
	struct ibv_send_wr wr = {
		.opcode = IBV_WR_SEND_WITH_IMM,
		.send_flags = 0,
		.sg_list = &sge,
		.num_sge = 1,
		.imm_data = (opcode | binder),
		.wr.ud.ah = ah,
		.wr.ud.remote_qpn = remote_qpn,
		.wr.ud.remote_qkey = remote_qkey,
	};
	struct ibv_send_wr *bad_wr = NULL;

	if (!mr) {
		wr.send_flags |= IBV_SEND_INLINE;
	} else {
		sge.lkey = mr->lkey;
	}
	
	ibv_post_send(id->qp, &wr, &bad_wr);
	assert(!bad_wr);
	
	// test_For
	//

	
	while (atomic_read(&binders[binder].flags) != 2) {
		struct ibv_wc wc;
		int bi;
		int nr_completed;

		nr_completed = ibv_poll_cq(id->recv_cq, 1, &wc);
		if (nr_completed == 0) {
			continue;
		} else if (nr_completed < 0) {
			return false;
		}

		bi = (wc.imm_data & 0xff);

		binders[bi].private = (void *)wc.wr_id;
		atomic_set(&binders[bi].flags, 2);
	}
	
	rw = (void *)binders[binder].private;

	ib_putback_recv_work(id->qp, rw);
	__put_binder(binder);

	return true;
}
