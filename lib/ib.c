

#include <infiniband/verbs.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sched.h>

#include<sys/time.h>


#include "common.h"
#include "ib.h"
#include "rpc.h"
#include "memblock.h"

struct rdma_connection_manager_t rcm ;
struct multicast_connection_manager_t mcm;


void* multicast_memory_thread(void *context);

static bool ib_process_event(struct rdma_event_channel *ec,int expect,
		void (*event_handler)(struct rdma_cm_event *event))
{
	int ret = 0 ;
	struct rdma_cm_event *event ;

	ret = rdma_get_cm_event(ec, &event);
	if(ret){
		rdma_error("Unalbe to get cm event");
		return false;
	}
	if(expect != -1){
		if(expect != event->event){
			debug("Event %s != expect %s\n",rdma_event_str(event->event),rdma_event_str(expect));
			rdma_ack_cm_event(event);
			return false;
		}
	}

	if(event_handler){
		event_handler(event);
	}

	rdma_ack_cm_event(event);
	
	return true;

}

static void process_event_multicast_set(struct rdma_cm_event *event)
{
	mcm.ah = ibv_create_ah(mcm.multicast->pd,&event->param.ud.ah_attr);
	if(!mcm.ah){
		rdma_error("Unable to creat ah");
		return ;
	}
	mcm.remote_qpn = event->param.ud.qp_num;
	mcm.remote_qkey = event->param.ud.qkey;

}
void set_rdma_connection_manager(char *server_ip,unsigned short port)
{
	rcm.server_ip = server_ip;
	rcm.server_port = port;
}

void set_multicast_connection_manager(char * multicast_ip)
{
	mcm.multicast_ip = multicast_ip;
}
void init_rdma_connection_manager(void)
{
	INIT_LIST_HEAD(&rcm.rdma_memhandler_list);
	rcm.alive = true;
	rcm.nr_user = 0;
	rcm.event_channel = NULL;
	rcm.rdma = NULL;
	init_binder();

}



void init_multicast_connection_manager(void)
{
	mcm.alive =&rcm.alive;
	mcm.realloc = false;
}

static bool ib_create_rdma_qp(struct rdma_cm_id *id)
{

	struct ibv_qp_init_attr attrs = {
		.qp_type = IBV_QPT_RC,
		.send_cq = NULL,
		.recv_cq = NULL,
		.srq = NULL,
		.cap=  {
			.max_send_wr = MCM_MAX_SEND_WR,
			.max_recv_wr = MCM_MAX_RECV_WR,
			.max_send_sge = MCM_MAX_SEND_SGE,
			.max_recv_sge = MCM_MAX_RECV_SGE,
			.max_inline_data = 64,
		}	
	};

	return rdma_create_qp(id,NULL,&attrs);
}

void set_current_rdma_handler(struct rdma_memory_handler_t *rmh)
{
	rcm.current_handler = rmh;
}

bool ib_rdma_server_connect(const unsigned short port)
{	

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(port),
	};

	rcm.event_channel = rdma_create_event_channel();
	if(!rcm.event_channel){
		rdma_error("Unable to create rdma_event_channel\n");
		return false;
	}
	if(rdma_create_id(rcm.event_channel, &rcm.rdma , NULL, RDMA_PS_IB)){
		rdma_error("Unable to create rdma create id\n");
		goto out_error;
	}

	if(rdma_bind_addr(rcm.rdma,(struct sockaddr*)&addr)){
		rdma_error("Unable to bind");
		goto out_error_bind;
	}

	if(rdma_listen(rcm.rdma,MAX_RDMA_USER)){
		rdma_error("Unable to listen");
		goto out_error_bind;
	}

	debug("Listen on %d\n", ntohs(rdma_get_src_port(rcm.rdma)));


	return true;

out_error_bind:
	rdma_destroy_id(rcm.rdma);
	rcm.rdma = NULL;

out_error:

	rdma_destroy_event_channel(rcm.event_channel);
	rcm.event_channel = NULL;
	return false;

}

bool ib_rdma_client_connect(const char *server_ip, const unsigned short server_port)
{

	struct sockaddr_in addr = {
		.sin_family = AF_INET,
		.sin_port = htons(server_port),
		.sin_addr = {
			.s_addr = inet_addr(server_ip),
		},
	};

	struct ibv_qp_init_attr attrs = {
		.qp_type = IBV_QPT_RC,
		.send_cq = NULL,
		.recv_cq = NULL,
		.srq = NULL,
		.cap=  {
			.max_send_wr = MCM_MAX_SEND_WR,
			.max_recv_wr = MCM_MAX_RECV_WR,
			.max_send_sge = MCM_MAX_SEND_SGE,
			.max_recv_sge = MCM_MAX_RECV_SGE,
			.max_inline_data = 0,
		}
	};

	struct rdma_conn_param param = {
		.flow_control = 1,
		.responder_resources = 16,
		.initiator_depth = 16,
		.retry_count = 7,
		.rnr_retry_count = 7,
		.private_data = NULL,
		.private_data_len = 0,
	};

	rcm.event_channel = rdma_create_event_channel();
	if(!rcm.event_channel){
		rdma_error("Unable to create rdma_event_channel");
		return false;
	}
	if(rdma_create_id(rcm.event_channel, &rcm.rdma , NULL, RDMA_PS_IB)){
		rdma_error("Unable to create rdma create id");
		goto out_error;
	}
	if(rdma_resolve_addr(rcm.rdma,NULL,(struct sockaddr*)&addr,10)){
		rdma_error("Unable to resolve server addr");
		goto out_error_resolve;
	}
	if(!ib_process_event(rcm.event_channel,RDMA_CM_EVENT_ADDR_RESOLVED,NULL)){
		rdma_error("Unable to resolve server addr (event)");
		goto out_error_resolve;
	}
	if(rdma_resolve_route(rcm.rdma,2000)){
		rdma_error("Unable to resolve route");
		goto out_error_resolve;
	}
	if(!ib_process_event(rcm.event_channel,RDMA_CM_EVENT_ROUTE_RESOLVED,NULL)){
		rdma_error("Unable to resolve server addr (event)");
		goto out_error_resolve;
	}
	if(rdma_create_qp(rcm.rdma,NULL,&attrs)){
		rdma_error("Unable to create rdma qp");
		goto out_error_resolve;
	}
	if (rdma_connect(rcm.rdma, &param)) {
		rdma_error("Unable to connect");
		goto out_error_resolve;
	}
	if (!ib_process_event(rcm.event_channel,RDMA_CM_EVENT_ESTABLISHED, NULL)) {
		rdma_error("Unable to connection  to the server (event)");
		goto out_error_resolve;
	}

	return true;

out_error_resolve:
	rdma_destroy_qp(rcm.rdma);
	rdma_destroy_id(rcm.rdma);
	rcm.rdma = NULL;
	debug("[RDMA] : Destory rdma id\n");
out_error:
	rdma_destroy_event_channel(rcm.event_channel);
	rcm.event_channel = NULL;
	debug("[RDMA] : Destory rdma event channel\n");
	return false;
}

bool ib_rdma_server_reconnect(void)
{
	return ib_rdma_server_connect(rcm.server_port);
}


struct rdma_memory_handler_t *alloc_rdma_memory_handler(struct rdma_cm_event *event)
{

	if(ib_create_rdma_qp(event->id)){
		rdma_error("Unalbe to create qp");
		goto out_error_accept;
	}

	if(rdma_accept(event->id,NULL)){
		rdma_error("Unable to accept rdma");
		goto out_error_accept;
	}

	struct rdma_memory_handler_t * rmh = (struct rdma_memory_handler_t*)malloc(sizeof(*rmh));
	
	rmh->rdma = event->id;
	rmh->event_channel = rcm.event_channel;

	for(int memblock_count = 0; memblock_count < MAX_MEMBLOCK; memblock_count++){
		rmh->memblocks[memblock_count] = NULL;
	}

	INIT_LIST_HEAD(&rmh->memblock_list);
	atomic_set(&rmh->batch,0);
	pthread_mutex_init(&rmh->memblock_lock, NULL);
	pthread_mutex_lock(&rmh->memblock_lock);

	rmh->nr_memblocks = 0;
	rmh->memblocks[rmh->nr_memblocks] = alloc_memblock(rmh->nr_memblocks);
	set_memblock(rmh->memblocks[rmh->nr_memblocks],rmh->rdma->pd);
	list_add_tail(&rmh->memblocks[rmh->nr_memblocks]->list,&rmh->memblock_list);

	pthread_mutex_unlock(&rmh->memblock_lock);

	rmh->nr_memblocks += 1;
	rmh->keep = &rcm.alive;
	rmh->barrier = &rcm.rdma_thread_barrier;

	rmh->rpc_buffer = mmap(NULL,RPC_BUFFER_SIZE,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,0,0);

	if(!rmh->rpc_buffer){
		rdma_error("Unable to mmap multicast buffer");
		goto out_error_buffer;
	}

	rmh->rpc_mr = ibv_reg_mr(rmh->rdma->pd,rmh->rpc_buffer,
			RPC_BUFFER_SIZE,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);

	if(!rmh->rpc_buffer){
		rdma_error("Unable to mmap multicast buffer");
		goto out_error_mr;
	}


	for (int i = 0; i < MCM_MAX_RECV_WR ; i++) {

		struct recv_work *rw = malloc(sizeof(*rw));
		struct ibv_recv_wr *bad_wr = NULL;
		void *b = rmh->rpc_buffer + i * (RPC_MAX_EACH_SIZE);

		*rw = (struct recv_work) {
			.id = i,
				.size = RPC_MAX_EACH_SIZE,
				.buffer = b,
				.sg_list = {
					.addr = (uintptr_t)b,
					.length = RPC_MAX_EACH_SIZE,
					.lkey = rmh->rpc_mr->lkey,
				},
				.wr = {
					.next = NULL,
					.wr_id = (uintptr_t)rw,
					.sg_list = &rw->sg_list,
					.num_sge = 1,
				},
				.convey = false,
		};

		if (ibv_post_recv(rmh->rdma->qp, &rw->wr, &bad_wr)) {
			rdma_error("Unable to post recv wr\n");
			goto out_error_mr;
		}
		assert(!bad_wr);

	}

	list_add_tail(&rmh->list,&rcm.rdma_memhandler_list);	

	rcm.nr_user += 1;

	pthread_mutex_lock(&rmh->memblock_lock);

	for(int i = 0; i < PRE_ALLOC_MEMBLOCK; i++){
		if(!rmh->memblocks[i]){
			add_memblock(rmh,i);
		}
	}

	pthread_mutex_unlock(&rmh->memblock_lock);
	return rmh;

out_error_mr:

	munmap(rmh->rpc_buffer,RPC_BUFFER_SIZE);
	rmh->rpc_buffer = NULL;
	ibv_dereg_mr(rmh->rpc_mr);
	rmh->rpc_mr = NULL;


out_error_accept:
	rdma_reject(rcm.rdma,NULL,0);

out_error_buffer :

	return NULL;
}

struct multicast_memory_handler_t *alloc_multicast_memory_handler(void)
{

	struct multicast_memory_handler_t *mmh;
	mmh = (struct multicast_memory_handler_t *)malloc(sizeof(*mmh));

	mmh->multicast = mcm.multicast;
	mmh->multicast_event_channel = mcm.multicast_event_channel;
	mmh->remote_qpn = mcm.remote_qpn;
	mmh->remote_qkey = mcm.remote_qkey;
	mmh->multicast_ah = mcm.ah;
	INIT_LIST_HEAD(&mmh->commit_list);
	pthread_spin_init(&mmh->lock,0);
	mmh->multicast_buffer = mmap(NULL,MULTICAST_BUFFER_SIZE,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,0,0);

	if(!mmh->multicast_buffer){
		rdma_error("Unable to mmap multicast buffer");
		goto out_error_buffer;
	}

	mmh->multicast_mr = ibv_reg_mr(mmh->multicast->pd,mmh->multicast_buffer,
			MULTICAST_BUFFER_SIZE,
			IBV_ACCESS_LOCAL_WRITE);

	if(!mmh->multicast_mr){
		rdma_error("Unable to reg multicast mr");
		goto out_error_mr;
	}

	void * rpc_buffer  = mmap(NULL,PAGE_SIZE,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,0,0);
	if(!rpc_buffer){
		rdma_error("Unable to mmap multicast rpc buffer");
		goto out_error_buffer;
	}

	mmh->rpc_mr = ibv_reg_mr(mmh->multicast->pd,rpc_buffer,
			PAGE_SIZE,
			IBV_ACCESS_LOCAL_WRITE);
	if(!mmh->rpc_mr){
		rdma_error("Unable to mmap multicast rpc mr");
		goto out_error_buffer;
	}

	mmh->keep = mcm.alive;
	mmh->realloc = &mcm.realloc;

	for (int i = 0; i < MCM_MAX_RECV_WR ; i++) {

		struct recv_work *rw = malloc(sizeof(*rw));
		struct ibv_recv_wr *bad_wr = NULL;
		void *b = mmh->multicast_buffer + i * (UD_EXTRA + PAGE_SIZE);
		*rw = (struct recv_work) {
			.id = i,
				.size = PAGE_SIZE + UD_EXTRA,
				.buffer = b,
				.sg_list = {
					.addr = (uintptr_t)b,
					.length = PAGE_SIZE + UD_EXTRA,
					.lkey = mmh->multicast_mr->lkey,
				},
				.wr = {
					.next = NULL,
					.wr_id = (uintptr_t)rw,
					.sg_list = &rw->sg_list,
					.num_sge = 1,
				},
			.convey = false,
		};

		if (ibv_post_recv(mmh->multicast->qp, &rw->wr, &bad_wr)) {
			rdma_error("Unable to post recv wr\n");
			goto out_error_mr;
		}
		assert(!bad_wr);

	}

//	mmh->work_completion = (struct work_completion_t*)malloc(sizeof(struct work_completion_t));	
	
	mcm.multicast_memory_handler = mmh;
	return mmh;


out_error_mr:

	munmap(mmh->multicast_buffer,MULTICAST_BUFFER_SIZE);
	mmh->multicast_buffer = NULL;
	mmh->multicast_mr = NULL;

out_error_buffer :

	return NULL;
}

bool ib_connect_multicast(const char * multicast_ip ,bool type)
{
	struct rdma_addrinfo *mcast_rai = NULL;
	struct rdma_addrinfo hints; 	
	memset(&hints, 0, sizeof(hints));

	struct ibv_qp_init_attr attrs = {};
	bzero(&attrs,sizeof(attrs));
	attrs.qp_type = IBV_QPT_UD;
	attrs.send_cq = NULL;
	attrs.recv_cq = NULL;
	attrs.cap.max_send_wr = MCM_MAX_SEND_WR;
	attrs.cap.max_recv_wr = MCM_MAX_RECV_WR;
	attrs.cap.max_send_sge = MCM_MAX_SEND_SGE;
	attrs.cap.max_recv_sge = MCM_MAX_RECV_SGE;

    hints.ai_flags = RAI_PASSIVE;

	mcm.multicast_event_channel = rdma_create_event_channel();
	if(!mcm.multicast_event_channel){
		rdma_error("Unable to create the event channel");
		return false;
	}
	if(rdma_create_id(mcm.multicast_event_channel,&mcm.multicast,NULL,RDMA_PS_UDP)){
		rdma_error("Unable to create rdma multicast");
		goto out_error;
	}

	hints.ai_port_space = RDMA_PS_UDP;
	hints.ai_flags = 0;

	if(rdma_getaddrinfo(multicast_ip, NULL, &hints, &mcast_rai)){
		rdma_error("Unable to get multicast addr");
		goto out_error;
	}

	if (type == CLIENT){
		if (rdma_resolve_addr(mcm.multicast, NULL,
					mcast_rai->ai_dst_addr, MCM_RDMA_RESOLVE_TIMEOUT_MS)) {
			rdma_error("Unable to resolve server address");
			goto out_error;
		}
		if (!ib_process_event(mcm.multicast_event_channel, RDMA_CM_EVENT_ADDR_RESOLVED, NULL)) {
			rdma_error("Unable to resolve server address (event)");
			goto out_error;
		}
	}else{

		struct rdma_addrinfo *bind_rai = NULL;

		bzero(&hints,sizeof(hints));
		hints.ai_port_space = RDMA_PS_UDP;

		if(rdma_getaddrinfo(MY_ADDR, NULL, &hints, &bind_rai)){
			rdma_error("Unable to get multicast addr");
			goto out_error;
		}
		if(rdma_bind_addr(mcm.multicast,bind_rai->ai_dst_addr)){
			rdma_error("Unable to bind multicast addr\n");
			goto out_error;
		}	
	
	}

	if(rdma_create_qp(mcm.multicast,NULL,&attrs)){
		rdma_error("Unable to create qp");
		goto out_error;
	}
	if(rdma_join_multicast(mcm.multicast,mcast_rai->ai_dst_addr,NULL)){
		rdma_error("Unable to join multicast");
		goto out_error;
	}
	if(!ib_process_event(mcm.multicast_event_channel,RDMA_CM_EVENT_MULTICAST_JOIN,
				process_event_multicast_set)){
		rdma_error("Unable to join mulicast (event)");
		goto out_error;
	}
/*
	if(type == CLIENT){
		struct multicast_memory_handler_t *mmh = alloc_multicast_memory_handler();
		pthread_create(mmh->thread_id,NULL,multicast_memory_thread,mmh);	
	}
*/
	return true;

out_error:
	rdma_destroy_id(mcm.multicast);
	rdma_destroy_event_channel(mcm.multicast_event_channel);
	return false;
}
bool set_up_handlers(struct rdma_memory_handler_t *rmh, 
		struct multicast_memory_handler_t *mmh)
{
	if(!rmh || !mmh){
		rdma_error("Unable to set up these handlers");
		return false;
	}
	rmh->multicast_memory_handler = mmh;
	mmh->rdma_memory_handler = rmh;

	return true;
}


static bool __ib_send(struct ibv_qp *qp, int opcode,uint32_t lkey, 
		void * buffer, size_t size, u32 imm_data, 
		void* remote_addr, uint32_t remote_rkey,
		bool inline_data)
{

	volatile unsigned long done = 0;

	struct ibv_sge sge = {
		.addr = (uintptr_t)buffer,
		.length = size,
		.lkey = lkey,
	};

	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)&done,
		.sg_list = &sge,
		.num_sge = 1 ,
		.opcode = opcode,
		.send_flags = IBV_SEND_SIGNALED,
		.imm_data = imm_data,
	};
	
	if(opcode != IBV_WR_SEND && opcode  != IBV_WR_SEND_WITH_IMM){
		wr.wr.rdma.remote_addr = (uintptr_t)remote_addr;
		wr.wr.rdma.rkey = remote_rkey;
	}

	if(inline_data){
		wr.send_flags |= IBV_SEND_INLINE;
	}	

	struct ibv_send_wr *bad_wr = NULL;
	ibv_post_send(qp,&wr,&bad_wr);
	assert(!bad_wr);

	while(!done){
		struct ibv_wc wc;
		int nr_completed;
		unsigned long *pdone;
		nr_completed = ibv_poll_cq(qp->send_cq,1,&wc);
		if(nr_completed<=0){
			continue;
		}
		if(wc.status != IBV_WC_SUCCESS){
			debug( "work completion status %s\n",
					ibv_wc_status_str(wc.status));
			return false;
		}
		pdone = (long unsigned int *)wc.wr_id;
		*pdone = 1;
	}


	return true;
}

static bool __ib_multi_send(struct ibv_qp *qp, int opcode,uint32_t lkey, 
		void * buffer, size_t size, u32 imm_data, 
		struct ibv_ah*ah, uint32_t remote_qpn, uint32_t remote_qkey,
		bool inline_data)
{

	volatile unsigned long done = 0;

	struct ibv_sge sge = {
		.addr = (uintptr_t)buffer,
		.length = size,
		.lkey = lkey,
	};

	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)&done,
		.sg_list = &sge,
		.num_sge = 1 ,
		.opcode = opcode,
		.send_flags =  IBV_SEND_SIGNALED,	
		.imm_data = imm_data,
		.wr.ud.ah = ah,
		.wr.ud.remote_qpn = remote_qpn,
		.wr.ud.remote_qkey = remote_qkey,
	};
	


	struct ibv_send_wr *bad_wr = NULL;
	ibv_post_send(qp,&wr,&bad_wr);
	assert(!bad_wr);

	while(!done){
		struct ibv_wc wc;
		int nr_completed;
		unsigned long *pdone;
		nr_completed = ibv_poll_cq(qp->send_cq,1,&wc);
		if(nr_completed<=0){
			continue;
		}
		if(wc.status != IBV_WC_SUCCESS){
			debug( "work completion status %s\n",
					ibv_wc_status_str(wc.status));
			return false;
		}
		pdone = (long unsigned int *)wc.wr_id;
		
		*pdone = 1;
	}

	return true;
}

bool __ib_rdma_send(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size, u32 imm_data ,bool inline_data)
{
	return __ib_send(rdma->qp,IBV_WR_SEND_WITH_IMM,mr->lkey,buffer,size,imm_data,
			EXCEPT_REMOTE_ADDR,EXCEPT_RKEY,inline_data);
} 

bool __ib_rdma_write(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size,
		void *remote_addr, uint32_t remote_rkey,bool inline_data)
{
	return __ib_send(rdma->qp,IBV_WR_RDMA_WRITE,mr->lkey,buffer,size,EXCEPT_IMM_DATA,remote_addr,remote_rkey,
			inline_data);

}

bool __ib_rdma_read(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size,
		void* remote_addr, uint32_t remote_rkey,bool inline_data)
{
	return __ib_send(rdma->qp,IBV_WR_RDMA_READ,mr->lkey,buffer,size,EXCEPT_IMM_DATA,remote_addr,remote_rkey,
			inline_data);
}

bool __ib_multicast_send_detail(struct rdma_cm_id *multicast, struct ibv_mr *mr, void *buffer, size_t size,
		uint32_t header,struct ibv_ah *ah, uint32_t remote_qpn, uint32_t remote_qkey)
{
	return __ib_multi_send(multicast->qp,IBV_WR_SEND_WITH_IMM,mr->lkey,buffer,size,
			header,ah,remote_qpn,remote_qkey,false);
}

bool __ib_multicast_send(struct rdma_cm_id *multicast, struct ibv_mr *mr, void *buffer, size_t size, u32 imm_data)
{
	return __ib_multi_send(multicast->qp,IBV_WR_SEND_WITH_IMM,mr->lkey,buffer,size,
			imm_data,mcm.ah,mcm.remote_qpn, mcm.remote_qkey,false);
}
/*
bool __ib_rdma_read(struct rdma_cm_id *rdma, struct ibv_mr *mr, void *buffer, size_t size,
		void* remote_addr, uint32_t remote_rkey,bool inline_data)
{
	return __ib_scan(rdma->qp,IBV_WR_RDMA_READ,mr->lkey,buffer,size,EXCEPT_IMM_DATA,remote_addr,remote_rkey,
			inline_data);
}
*/

bool ib_multicast_inline_send(struct rdma_cm_id *id, u32 imm, void *buffer, size_t size,
		struct ibv_ah *ah, uint32_t remote_qpn, uint32_t remote_qkey)
{
	volatile unsigned int done = 0;

	struct ibv_sge sge = {
		.addr = (uintptr_t)buffer,
		.length = size,
	};
	struct ibv_send_wr wr = {
		.wr_id = (uintptr_t)&done,
		.opcode = IBV_WR_SEND_WITH_IMM,
		.send_flags = IBV_SEND_INLINE,
		.sg_list = &sge,
		.num_sge = 1,
		.imm_data = imm,
		.wr.ud.ah = ah,
		.wr.ud.remote_qpn = remote_qpn,
		.wr.ud.remote_qkey =remote_qkey,
	};
	struct ibv_send_wr *bad_wr = NULL;

	assert(size <= 64);
	
	ibv_post_send(id->qp, &wr, &bad_wr);
	assert(!bad_wr);
	return true;
}

void ib_putback_recv_work(struct ibv_qp *qp, struct recv_work *rw)
{
	struct ibv_recv_wr *bad_wr = NULL;

	ibv_post_recv(qp, &rw->wr, &bad_wr);
	assert(!bad_wr);
}

static void rdma_memory_handler_die(struct rdma_memory_handler_t * rmh)
{

	for(int i = 0; i < MAX_MEMBLOCK; i++){
		if(rmh->memblocks[i]){
			memblock_die(rmh->memblocks[i]);
			rmh->memblocks[i] = NULL;
		}
	}

	ibv_dereg_mr(rmh->rpc_mr);	
	if(rmh->rdma){
		rdma_destroy_id(rmh->rdma);
	}
	if(rmh->event_channel){
		rdma_destroy_event_channel(rmh->event_channel);
	}

	rmh->event_channel = NULL;
	rmh->rdma = NULL;

	munmap(rmh->rpc_buffer,RPC_BUFFER_SIZE);
	free(rmh);

}


static void multicast_memory_handler_die(struct multicast_memory_handler_t * mmh)
{

	if(mmh->multicast_mr){
		ibv_dereg_mr(mmh->multicast_mr);
		mmh->multicast_mr = NULL;
	}
	if(mmh->multicast_buffer){
		munmap(mmh->multicast_buffer,MULTICAST_BUFFER_SIZE);
		mmh->multicast_buffer = NULL;
	}
	if(mmh->multicast){
		rdma_destroy_id(mmh->multicast);
		mmh->multicast = NULL;
	}
	if(mmh->multicast_event_channel){
		rdma_destroy_event_channel(mmh->multicast_event_channel);
		mmh->multicast_event_channel = NULL;
	}

	free(mmh);

}

void rdma_connection_die(void)
{

	struct rdma_memory_handler_t *rmh, *temp;
	list_for_each_entry_safe(rmh,temp,&rcm.rdma_memhandler_list,list){
		list_del_init(&rmh->list);
		rdma_memory_handler_die(rmh);
	}

	if(rcm.rdma)
		rdma_destroy_id(rcm.rdma);
	if(rcm.event_channel)
	rdma_destroy_event_channel(rcm.event_channel);	
	
	rcm.event_channel = NULL;
	rcm.rdma = NULL;

	rcm.alive = false;
}

void multicast_connection_die(void)
{
	rdma_destroy_event_channel(mcm.multicast_event_channel);

	rdma_destroy_id(mcm.multicast);
	
	mcm.multicast_event_channel = NULL;
	
	mcm.multicast = NULL;

	rcm.alive = false;
	
	multicast_memory_handler_die(mcm.multicast_memory_handler);

}

void ib_pevent(void)
{
	while(rcm.alive)
		ib_process_event(rcm.event_channel,-1,rdma_event_handler);
}
void ib_flow(void *src, void* dst, uint32_t offset)
{
	memcpy(dst+8,src,PAGE_SIZE);
}

int ib_fault_handling()
{
	return 0;
}

/*
 * TEST CODE
 * CODE FOR TEST
 * 
 * */
/*
void test_rdma_send(void)
{
	int rnum;

	char *lnum = (char*)malloc(RPC_MAX_EACH_SIZE);
	struct ibv_mr *mr = ibv_reg_mr(
			rcm.rdma->pd,lnum,RPC_MAX_EACH_SIZE,	
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);

	uint32_t header = (RDMA_OPCODE_CHECK << 28);
	for(int i = 0 ; i < 10 ; i ++){
		rnum = rand();	
		snprintf(lnum,RPC_MAX_EACH_SIZE,"%d",rnum);
		if(!__ib_rdma_send(rcm.rdma,mr,lnum,RPC_MAX_EACH_SIZE,header,false)){
			return;
		}
		printf("CHECK MESS[%d] : %s\n",i,lnum);
	}

	ibv_dereg_mr(mr);
	free(lnum);
}

void test_multicast_send(void)
{
	int rnum;

	char *lnum = (char*)malloc(RPC_MAX_EACH_SIZE);
	struct ibv_mr *mr = ibv_reg_mr(
			mcm.multicast->pd,lnum,RPC_MAX_EACH_SIZE,	
			IBV_ACCESS_LOCAL_WRITE);
	uint32_t header = (MULTICAST_OPCODE_CHECK << 28);
	
	for(int i = 0 ; i < 10 ; i ++){
		rnum = rand();	
		snprintf(lnum,RPC_MAX_EACH_SIZE,"%d",rnum);
		if(!__ib_multicast_send(mcm.multicast,mr,lnum,RPC_MAX_EACH_SIZE,header)){
			return;
		}
		//printf("CHECK MESS[%d] : %s\n",i,lnum);
	}

	ibv_dereg_mr(mr);
	free(lnum);
}

void test_multicast_send_flow(void)
{
	int rnum;
	printf("PAGE SIZE is %d\n",PAGE_SIZE);
	char *lnum = (char*)malloc(PAGE_SIZE);
	struct ibv_mr *mr = ibv_reg_mr(
			mcm.multicast->pd,lnum,PAGE_SIZE,	
			IBV_ACCESS_LOCAL_WRITE);
	uint32_t header =((MULTICAST_OPCODE_FLOW << 28) | (1UL << 13));
	for(int i = 0 ; i < 10 ; i ++){
		rnum = rand();	
		snprintf(lnum,100,"%d",rnum);
		if(!__ib_multicast_send(mcm.multicast,mr,lnum,PAGE_SIZE,header)){
			return;
		}
		printf("CHECK MESS[%d] : %s\n",i,lnum);
	}

	ibv_dereg_mr(mr);
	free(lnum);
}

void test_rpc_open(void)
{
	struct rdma_open_t list[10];
	struct ibv_mr *rpc_mr = NULL;
	void *rpc_buffer = NULL;

	rpc_buffer = mmap(NULL,RPC_BUFFER_SIZE,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,0,0);

	if(!rpc_buffer){
		rdma_error("Unable to mmap multicast buffer");
	}

	rpc_mr = ibv_reg_mr(rcm.rdma->pd,rpc_buffer,
			RPC_BUFFER_SIZE,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);

	if(!rpc_buffer){
		rdma_error("Unable to mmap multicast buffer");
	}

	for (int i = 0; i < MCM_MAX_RECV_WR ; i++) {

		struct recv_work *rw = malloc(sizeof(*rw));
		struct ibv_recv_wr *bad_wr = NULL;
		void *b = rpc_buffer + i * (RPC_MAX_EACH_SIZE);

		*rw = (struct recv_work) {
			.id = i,
				.size = RPC_MAX_EACH_SIZE,
				.buffer = b,
				.sg_list = {
					.addr = (uintptr_t)b,
					.length = RPC_MAX_EACH_SIZE,
					.lkey = rpc_mr->lkey,
				},
				.wr = {
					.next = NULL,
					.wr_id = (uintptr_t)rw,
					.sg_list = &rw->sg_list,
					.num_sge = 1,
				},
		};

		if (ibv_post_recv(rcm.rdma->qp, &rw->wr, &bad_wr)) {
			rdma_error("Unable to post recv wr\n");
		}
		assert(!bad_wr);
	}
	struct rdma_open_t *req = malloc(sizeof(*req)*2);
	struct rdma_open_t *res = req + 1;

	struct ibv_mr * mr = ibv_reg_mr(rcm.rdma->pd,req,sizeof(*req)*2,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);


	for(int i = 0 ; i < 3 ; i++){
		uint32_t opcode = 0 ; 
		opcode = (RDMA_OPCODE_OPEN << 28);

		size_t soffset = ((1UL << (30)) * i);
		uint32_t offset  = (soffset >> 12) ;
		opcode |= offset;
		ib_rpc(rcm.rdma,opcode,req,sizeof(*req),res,sizeof(*req),NULL);		
		memcpy(list+i,res,sizeof(*req));
		printf("[RPC] : remote addr : %lx rkey : %x\n",list[i].remote_addr,list[i].rkey);
		sleep(1);
	}


	void* scan_buffer = mmap(NULL,PAGE_SIZE,
			PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS,0,0);

	if(!scan_buffer){
		rdma_error("Unable to mmap multicast buffer");
	}

	void* scan_mr = ibv_reg_mr(rcm.rdma->pd,scan_buffer,
			PAGE_SIZE,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);

	if(!scan_mr){
		rdma_error("Unable to mmap multicast buffer");
	}



	int rnum;
	char *lnum = (char*)malloc(PAGE_SIZE);
	struct ibv_mr *lmr = ibv_reg_mr(
			mcm.multicast->pd,lnum,PAGE_SIZE,	
			IBV_ACCESS_LOCAL_WRITE);

	struct timespec  begin, end;
	struct timespec write_begin,write_end;
	struct timespec read_begin, read_end;
	double interval_time = 0.0;
	double interval_write_time = 0.0;
	double interval_read_time = 0.0;

	uint64_t nr_block;
	int send_count = 0;

	clock_gettime(CLOCK_REALTIME, &begin);
	static size_t batch = 0 ;
	
	for(int i = 0 ; i < (1UL << 18)  ; i ++){	
		size_t offset = (1UL << 12) * i ;
		nr_block = (offset >>30);
		uint32_t header = ((MULTICAST_OPCODE_FLOW << 28) | (offset>>12));
		offset -= (nr_block << 30) ;

		rnum = rand();	
		snprintf(lnum,PAGE_SIZE,"%d",rnum);
//		memset(lnum,rnum,PAGE_SIZE);
	
		batch++;
		clock_gettime(CLOCK_REALTIME,&write_begin);
		if(!__ib_multicast_send(mcm.multicast,lmr,lnum,PAGE_SIZE,header)){
			rdma_error("Oh my god");
			return;
		}
		clock_gettime(CLOCK_REALTIME,&write_end);
		interval_write_time += ((write_end.tv_sec - write_begin.tv_sec) + (write_end.tv_nsec - write_begin.tv_nsec)/ 1000000000.0);

		if(batch % 100 == 0){
			uint32_t opcode = 0 ; 
			opcode |= (RDMA_OPCODE_COMMIT << 28);
			ib_rpc(rcm.rdma,opcode,req,sizeof(*req),res,sizeof(*req),mr);
//			ib_multicast_rpc(mcm.multicast,opcode,lnum,1,lnum,1,lmr,
//					mcm.ah,mcm.remote_qpn,mcm.remote_qkey);
			
			for(int i = send_count; i < batch; i++){
				clock_gettime(CLOCK_REALTIME,&read_begin);
				if(!__ib_rdma_read(rcm.rdma,scan_mr,scan_buffer,PAGE_SIZE,
							(void*)list[nr_block].remote_addr + (1UL<<12)*i ,list[nr_block].rkey,false)){
					break;
				}
		//		debug("%s\n",(char*)scan_buffer);
				clock_gettime(CLOCK_REALTIME,&read_end);	
				interval_read_time += ((read_end.tv_sec - read_begin.tv_sec) + (read_end.tv_nsec - read_begin.tv_nsec)/ 1000000000.0);
			}
			send_count = batch;
		}


		//printf("CHECK MESS[%d] : %s\n",i,lnum);
		if(!__ib_rdma_read_scan(rcm.rdma,scan_mr,scan_buffer,(PAGE_SIZE+8),
					(void*)list[nr_block].remote_addr + offset + (offset >>12)*8 ,list[nr_block].rkey,false)){
			break;
		}
		if(strncmp(lnum,scan_buffer+8,100)){
			printf(" %s : %s\n",lnum,(char*)(scan_buffer)+1);
		}

//		debug("Inter val Time (Micro): %lf\n", (double)interval_time/1000000);
	}

    clock_gettime(CLOCK_REALTIME, &end);
	debug("batch is %ld\n",batch);
	interval_time = ((end.tv_sec - begin.tv_sec) + (end.tv_nsec - begin.tv_nsec)/ 1000000000.0) ;
	debug("[micro]write average is %lf total(sec):  %lf\n",((double)interval_write_time*1000000.0/ (1UL<<18)),(double)interval_write_time);	
	debug("[micro]read average is %lf total(sec):  %lf\n",((double)interval_read_time*1000000.0/ (1UL<<18)),(double)interval_read_time);	
	debug("[micro]average is %lf total(sec):  %lf\n",((double)interval_time*1000000.0/ (1UL<<18)),(double)interval_time);	
//	debug("count is %ld\n",batch);
	ibv_dereg_mr(lmr);
	free(lnum);
	ibv_dereg_mr(mr);
	free(req);
	ibv_dereg_mr(rpc_mr);
	munmap(rpc_buffer,RPC_BUFFER_SIZE);
}
*/
