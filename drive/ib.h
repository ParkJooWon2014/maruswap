#if !defined(__MARUSWAP_RDMA_H__)
#define __MARUSWAP_IB_H__

#include <rdma/ib_verbs.h>
#include <rdma/rdma_cm.h>

#include <linux/inet.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/mm_types.h>
#include <linux/gfp.h>
#include <linux/pagemap.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>

#define rdma_opne_t rdma_info_t

#define multi_info(msg, args...) do {\
	pr_info( "{MULTICAST] : "msg,  ## args);\
}while(0);

#define multi_err( msg, args...) do {\
	pr_err( "{MULTICAST] : %d : ERROR : "msg,  __LINE__, ## args);\
}while(0);

#define MAIN "MAIN"
#define SUB "SUB"

enum {
	MAX_MEMBLOCK = 128,

	NR_SERVER = 2,
	BUFFER_SIZE = (1UL <<  5),

	CONFIG_BATCH = (1UL << 10),
	CONFIG_NR_STAGE_BUFFER = CONFIG_BATCH,

	CONNECTION_TIMEOUT_MS = 60000,
	COMMIT_TIMEOUT_MS = 6000,//CONFIG_BATCH*5000,

	MARUSWAP_MAX_SEND_WR = (1UL << 12),
	MARUSWAP_MAX_RECV_WR = (1UL << 12),
	MARUSWAP_MAX_SEND_SGE = 64,
	MARUSWAP_MAX_RECV_SGE = 64,

	OPCODE_OPEN = 0x1,
	OPCODE_COMMIT = 0x3,
	OPCODE_QCOMMIT = 0x7,

	MULTICAST_OPCODE_FLOW = 0x1,
	NR_RECV = 32,
	RPC_MAX_EACH_SIZE = (2*PAGE_SIZE) / NR_RECV,

	SHARE_XARRAY = MARUSWAP_MAX_SEND_WR,
	STAGE_BUFFER_SIZE = (PAGE_SIZE*CONFIG_BATCH),

};

struct stage_buffer_t{
	void *buffer;
	volatile int index;

	struct xarray check_list;
	spinlock_t lock;

	int count;
};

struct recv_work {

	unsigned long id;
	size_t size;
	void *buffer;

	struct ib_sge sg_list;
	struct ib_recv_wr wr;

	unsigned long private;

};

struct rdma_info_t{
	uintptr_t remote_addr;
	uint32_t rkey;
};

struct rdma_commit_t{
	uint32_t header;
};

struct ud_package_t {

	struct ib_ah *ah;
	struct ib_qp *qp;
	u32 remote_qpn;
	u32 remote_qkey;
};


struct multicast_ctrl_t{
	struct rdma_cm_id *multicast;

	struct ib_qp *qp;
	struct ib_cq *snd_cq;
	struct ib_cq *rcv_cq;
	struct ib_pd *pd;
	struct ib_mr *mr;

	struct ud_package_t *ud_package;

	struct rdma_ctrl_t *rdma_ctrl[NR_SERVER];

	spinlock_t lock;
	struct completion cm_done;

	struct xarray *check_list;
	int cm_error;

	union {
		struct sockaddr multicast_addr;
		struct sockaddr_in multicast_addr_in;
	};

	union {
		struct sockaddr src_addr;
		struct sockaddr_in src_addr_in;
	};
};

struct rdma_ctrl_t{

	bool alive;

	short id; 
	struct rdma_cm_id *rdma;

	struct ib_qp *qp;
	struct ib_device *dev;
	struct ib_cq *snd_cq;
	struct ib_cq *rcv_cq;
	struct ib_pd *pd;
	struct ib_mr *mr;
	void * buffer;
	u64 dma_buffer;

	u64 req_buffer;

	void *rcv_buffer;

	struct rdma_info_t rdma_info[MAX_MEMBLOCK];
	int cm_error;

	atomic_t *nr_rdma_ctrl;

	struct completion *done;
	struct completion *wait;
	struct completion cm_done;

	atomic_t *batch;
	struct mutex *lock;
	spinlock_t *spinlock;
	struct semaphore *sema;

	struct xarray *check_list;
	struct xarray *read_list;

	union {
		struct sockaddr dst_addr;
		struct sockaddr_in dst_addr_in;
	};

	union {
		struct sockaddr src_addr;
		struct sockaddr_in src_addr_in;
	};

	struct stage_buffer_t *stage_buffer;

	struct list_head list;
};

struct ctrl_t{

	struct rdma_ctrl_t *main_rdma_ctrl;

	struct rdma_ctrl_t rdma_ctrl[NR_SERVER];
	struct completion done;

	struct multicast_ctrl_t multicast_ctrl;

	atomic_t batch;
	struct completion wait;
	atomic_t nr_rdma_ctrl;

	struct xarray check_list;
	struct xarray read_list;
	
	struct mutex lock;
	spinlock_t spinlock;
	struct semaphore sema;
	struct list_head rdma_ctrl_list;

	struct stage_buffer_t stage_buffer;
};

struct ctrl_t ctrl ;

inline void init_ctrl(void)
{
	memset(&ctrl,0,sizeof(ctrl));
	init_completion(&ctrl.done);
	init_completion(&ctrl.wait);
	xa_init(&ctrl.check_list);
	xa_init(&ctrl.read_list);
	mutex_init(&ctrl.lock);
	spin_lock_init(&ctrl.spinlock);
	INIT_LIST_HEAD(&ctrl.rdma_ctrl_list);
	atomic_set(&ctrl.nr_rdma_ctrl,0);
	atomic_set(&ctrl.batch,0);
	xa_init(&ctrl.stage_buffer.check_list);
}

inline void set_rdma(struct rdma_ctrl_t *rdma_ctrl)
{
	rdma_ctrl->nr_rdma_ctrl = &ctrl.nr_rdma_ctrl;
	rdma_ctrl->alive = false;
	rdma_ctrl->done = &ctrl.done;
	rdma_ctrl->wait = &ctrl.wait;
	rdma_ctrl->check_list = &ctrl.check_list;
	rdma_ctrl->read_list = &ctrl.read_list;
	rdma_ctrl->batch = &ctrl.batch;
	rdma_ctrl->lock = &ctrl.lock;
	rdma_ctrl->spinlock = &ctrl.spinlock;
	rdma_ctrl->sema = &ctrl.sema;
	rdma_ctrl->stage_buffer = &ctrl.stage_buffer;
	list_add(&rdma_ctrl->list,&ctrl.rdma_ctrl_list);
}
inline u32 get_offset(const u64 roffset){
	return ((roffset >> PAGE_SHIFT) & 0xfffffff);
}
inline u32 set_header(int opcode, const u32 offset){
	return (((opcode << 28) | (offset & 0xfffffff)) & 0xffffffff);
}
inline struct rdma_info_t *get_rdma_info(struct rdma_ctrl_t *rmt, int num){
	return (rmt->rdma_info + num);
}
inline void set_main_rdma_ctrl(struct rdma_ctrl_t *rdma_ctrl)
{
	mutex_trylock(&ctrl.lock);
	ctrl.main_rdma_ctrl = rdma_ctrl;
	mutex_unlock(&ctrl.lock);
}
inline struct rdma_ctrl_t * get_main_rdma_ctrl(void)
{
	return ctrl.main_rdma_ctrl;
}
inline struct rdma_ctrl_t *get_sub_rdma_ctrl(void)
{
	struct rdma_ctrl_t * sub = NULL;
	if(list_empty(&ctrl.rdma_ctrl_list))
		return sub;
	list_for_each_entry(sub,&ctrl.rdma_ctrl_list,list){
		if(sub != get_main_rdma_ctrl()){
			break;
		}
	}
	return sub;
}
inline struct multicast_ctrl_t *get_multicast_ctrl(void)
{
	return &ctrl.multicast_ctrl;
}
inline int get_num_memblock(const u32 offset)
{
	return ((offset >>18) & 0x3ff);
}
inline struct rdma_commit_t *get_commit_t(struct rdma_ctrl_t *rdma_ctrl)
{
	return ((struct rdma_commit_t*)rdma_ctrl->buffer);
}
inline struct rdma_info_t * get_req_rdma_info(struct rdma_ctrl_t* rdma_ctrl)
{
	return (struct rdma_info_t *)(rdma_ctrl->buffer + sizeof(struct rdma_commit_t));
}

inline struct rdma_info_t * get_res_rdma_info(struct rdma_ctrl_t* rdma_ctrl)
{
	return (struct rdma_info_t *)(rdma_ctrl->buffer);
}
inline int check_rdma_info(int nr_memblock)
{
	struct rdma_ctrl_t * rdma_ctrl = NULL;
	int  check = 0;
	list_for_each_entry(rdma_ctrl,&ctrl.rdma_ctrl_list,list){	
		if(rdma_ctrl->rdma_info[nr_memblock].rkey == 0)
			check = 1;
	}
	return check;
}

inline char * id_to_msg(int id)
{
	if(id == 0){
		return MAIN;
	}
	else {
		return SUB;
	}
}
inline struct ctrl_t *get_ctrl(void){
	return &ctrl;
}

inline int stage_buffer_full(struct stage_buffer_t *stage_buffer)
{
	return (stage_buffer->count >= CONFIG_BATCH);
}

inline void stage_buffer_count_add(struct stage_buffer_t *stage_buffer)
{
	stage_buffer->count += 1;
}
inline void stage_buffer_count_reset(struct stage_buffer_t * stage_buffer)
{
	stage_buffer->count=0;
}

int maruswap_multicast_write(struct page *page, u32 offset);
int maruswap_rdma_read(struct page *page, u32 offset);


#endif
