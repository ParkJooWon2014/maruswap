

#ifndef __IB_H__
#define __IB_H__

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <pthread.h>

#include "atomic.h"
#include "memblock.h"
#include "rpc.h"
#include "config.h"

#define EXCEPT_REMOTE_ADDR  NULL
#define EXCEPT_AH  NULL

enum{
	MAX_RDMA_USER = 10,
	MAX_MEMBLOCK = (1 << 7),

	MCM_MAX_RECV_WR = (1UL <<12),
	MCM_MAX_SEND_WR = (1UL <<12),
	MCM_MAX_RECV_SGE = 4,
	MCM_MAX_SEND_SGE = 4,

	MAX_WORK_COMPLETION = MCM_MAX_RECV_WR,

	MCM_RDMA_RESOLVE_TIMEOUT_MS = 2000,

	UD_EXTRA = 40,

	MULTICAST_NR_BUFFER = (1UL << (30 -12)),
	MULTICAST_BUFFER_SIZE = (UD_EXTRA + PAGE_SIZE) * MULTICAST_NR_BUFFER,

	EXCEPT_QPN =0x0,
	EXCEPT_QKEY = 0x0,
	EXCEPT_IMM_DATA = 0x0,
	EXCEPT_REMOTE_RKEY =0x0,
	EXCEPT_RKEY = 0x0,

};


struct rdma_memory_handler_t{
	
	struct rdma_event_channel *event_channel;
	struct rdma_cm_id * rdma;

	bool *keep ;

	short nr_memblocks;
	struct memblock *memblocks[MAX_MEMBLOCK];
	struct list_head memblock_list;
	pthread_mutex_t memblock_lock;

	struct list_head list;
	struct multicast_memory_handler_t *multicast_memory_handler;

	void *rpc_buffer;
	struct ibv_mr *rpc_mr;

	pthread_t thread_id[NR_RECVER];

	pthread_barrier_t *barrier;
	atomic_t batch;

	struct list_head work_list;
};

struct multicast_memory_handler_t{

	struct rdma_event_channel * multicast_event_channel;
	struct rdma_cm_id *multicast;

	bool *keep;

	void * multicast_buffer;
	struct ibv_mr *multicast_mr;

	struct ibv_mr * rpc_mr ;

	struct ibv_ah *multicast_ah;
	uint32_t remote_qpn;
	uint32_t remote_qkey;

	struct rdma_memory_handler_t *rdma_memory_handler;

	bool *realloc;
	pthread_t thread_id[NR_RECVER];
	pthread_t process_thread[NR_WORKER];

	struct work_completion_t *work_completion;
	struct list_head commit_list;

	pthread_spinlock_t work_lock;
	pthread_spinlock_t lock;
	struct list_head work_list;
};

struct rdma_connection_manager_t {	

	struct rdma_event_channel *event_channel;
	struct rdma_cm_id * rdma;

	bool alive;

	struct list_head rdma_memhandler_list;	
	short nr_user;

	char *server_ip;
	short server_port;

	struct rdma_memory_handler_t *current_handler;

	short nr_rdma_memory_handler;

	pthread_barrier_t rdma_thread_barrier;

	pthread_spinlock_t lock;
};

struct multicast_connection_manager_t {

	struct rdma_event_channel *multicast_event_channel;
	struct rdma_cm_id * multicast;

	struct ibv_ah * ah;
	uint32_t remote_qpn;
	uint32_t remote_qkey;

	bool *alive;

	char *multicast_ip;
	bool realloc;

	struct multicast_memory_handler_t *multicast_memory_handler;
};

struct rdma_sender_t{

	struct rdma_cm_id *rdma;

	struct remote_info_t *remote_infos[MAX_MEMBLOCK];

	void *rpc_buffer;
	struct ibv_mr *rpc_mr;

	pthread_barrier_t * barrier;
};

struct multicast_sender_t{

	struct rdma_cm_id *multicast;

	void *buffer;
	struct ibv_mr *mr;
	
};

struct remote_info_t
{
	uintptr_t remote_addr;
	uint32_t rkey;
};



/*CODE*/
void init_multicast_connection_manager(void);
void init_rdma_connection_manager(void);

bool ib_rdma_server_connect(const unsigned short port);
bool ib_rdma_client_connect(const char *server_ip, const unsigned short server_port);
bool ib_connect_multicast(const char * multicast_ip,bool type);

bool ib_rdma_server_connect(const unsigned short port);
bool ib_rdma_client_connect(const char *server_ip, const unsigned short server_port);
bool ib_connect_multicast(const char * multicast_ip,bool type);

struct rdma_memory_handler_t *alloc_rdma_memory_handler(struct rdma_cm_event *event);
struct multicast_memory_handler_t *alloc_multicast_memory_handler(void);struct multicast_memory_handler_t *alloc_multicast_memory_handler(void);

bool set_up_handlers(struct rdma_memory_handler_t *rmh, 
		struct multicast_memory_handler_t *mmh);
void ib_putback_recv_work(struct ibv_qp *qp, struct recv_work *rw);

bool __ib_rdma_send(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size, u32 imm_data, bool inlin_data);
bool __ib_rdma_write(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size,
		void *remote_addr, uint32_t remote_rkey, bool inline_data);
bool __ib_rdma_read(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size,
		void* remote_addr, uint32_t remote_rkey, bool inline_data);
bool __ib_multicast_send_detail(struct rdma_cm_id *multicast, struct ibv_mr *mr, void *buffer, size_t size,
		uint32_t header,struct ibv_ah *ah, uint32_t remote_qpn, uint32_t remote_qkey);


bool __ib_multicast_send(struct rdma_cm_id *multicast, struct ibv_mr *mr, void *buffer, size_t size, u32 imm_data);
void rdma_connection_die(void);
void multicast_connection_die(void);

void set_current_rdma_handler(struct rdma_memory_handler_t *rmh);
	
void ib_pevent(void);
void ib_flow(void *src, void* dst, uint32_t offset);
struct memory_sender_t * alloc_memory_sender();
struct rdma_memory_handler_t *alloc_temp_rdma_memory_handler(void);

bool ib_multicast_inline_send(struct rdma_cm_id *id, u32 imm, void *buffer, size_t size,
		struct ibv_ah *ah, uint32_t remote_qpn, uint32_t remote_qkey);
/*CODE FOR TEST*/


bool ib_scan(struct memory_sender_t *ms);
void test_rdma_send(void);
void test_multicast_send(void);

void test_multicast_send_flow(void);
void test_rpc_open(void);
#endif
