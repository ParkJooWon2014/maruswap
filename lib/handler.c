

#include "common.h"
#include "rpc.h"
#include "ib.h"
#include "atomic.h"

#include <stdlib.h>
#include <unistd.h>

/*
 *  RPC 
 *  --------------------  ------------------  -------------------
 *  |31 ~ 28 : opcode(4) | 27 ~ 8 offset(20) | 7 - 0 : binder(8) |
 *  --------------------  ------------------  -------------------
 *  GET_PAGE_RPC
 *
 *  |31 ~ 28 : opcode(4) | 27 ~ 8 |
 *
 * multicast header :
 *  ---------------------  -------------------
 *  | 31 ~ 28 : opcode(4) | 27 ~ 0 offset(28) |
 *  ---------------------  --------------------
 * */

static void __process_rdma_rpc_open(struct rdma_memory_handler_t *rmh, struct ibv_wc *wc)
{
	uint32_t header = wc->imm_data;
	int nr_block = -1;
	uint32_t offset = 0;
	int opcode = -1 ;
	struct rdma_open_t *res = (struct rdma_open_t*)rmh->rpc_buffer;

	pthread_mutex_lock(&rmh->memblock_lock);
	bzero(res,sizeof(*res));

	if(!interpret_header(header, &opcode, &nr_block, &offset)){
		rdma_error("Unable to interpret header");
		return;
	}
	
	if(!rmh->memblocks[nr_block]){
		add_memblock(rmh,nr_block);
	}

	res->remote_addr = (uintptr_t)rmh->memblocks[nr_block]->mr->addr;
	res->rkey = (uintptr_t)rmh->memblocks[nr_block]->mr->rkey;

	__ib_rdma_send(rmh->rdma,rmh->rpc_mr,rmh->rpc_buffer,sizeof(*res),header,true);
	
	pthread_mutex_unlock(&rmh->memblock_lock);
	printf("[RMDA] : {OPEN} remote_addr : %lx, rkey = %x \n ",res->remote_addr,res->rkey);

}

/* NOT COMPLETE........*/
static void __process_rdma_rpc_get_page(struct rdma_memory_handler_t *rmh, struct ibv_wc *wc)
{
	uint32_t header = wc->imm_data;
	int nr_block = -1;
	uint32_t offset = 0;
	int opcode = -1 ;
	void* page = NULL;
	struct recv_work *rw = NULL;
	bool check = false;
	struct multicast_memory_handler_t *mmh = rmh->multicast_memory_handler;

	if(!interpret_header(header, &opcode, &nr_block, &offset)){
		rdma_error("Unable to interpret");
		return;
	}

	while(!check){
		pthread_spin_lock(&mmh->lock);
		page = rmh->rpc_buffer + __RPC_BUFFER_SIZE;
		list_for_each_entry(rw,&mmh->commit_list,list){
			struct ibv_wc *wwc = &rw->wc;
			if((wwc->imm_data & 0xfffffff) == offset){
				memcpy(page,rw->buffer + UD_EXTRA,PAGE_SIZE);
				check = true;
			}
		}
		pthread_spin_unlock(&mmh->lock);
	}

	__ib_rdma_send(rmh->rdma,rmh->rpc_mr,page,PAGE_SIZE,header,true);	
}


static void __process_rdma_rpc_close(struct rdma_memory_handler_t *rdma, struct ibv_wc *wc)
{

}
static void __process_rdma_rpc_transition(struct rdma_memory_handler_t *rdma, struct ibv_wc *wc)
{

}

static void __process_rdma_rpc_check(struct rdma_memory_handler_t *rmh ,struct ibv_wc *wc)
{
	struct recv_work *rw = (struct recv_work*)wc->wr_id;
	printf("[RDMA] : check Messege :  %s recv work id :%ld\n",(char*)(rw->buffer) ,rw->id);
}

static bool ib_convey_page(struct rdma_memory_handler_t *rmh, struct ibv_wc *wc)
{
	void *src_buffer = NULL;
	void *dst_buffer = NULL;

	uint32_t header = wc->imm_data;
	int nr_block = -1;
	uint32_t offset = 0;
	int opcode = -1 ;
	struct recv_work *rw = (struct recv_work*)wc->wr_id;

	if(!interpret_header(header, &opcode, &nr_block, &offset)){
		rdma_error("Unable to interpret");
		return false;
	}

	if(!rmh->memblocks[nr_block]){
		if(!add_memblock(rmh,nr_block)){
			debug("Cannot add memblock\n");
			return false;
		}
		printf("add block is completed!\n");
	}
	// barrier 
	src_buffer = rw->buffer + UD_EXTRA;
	dst_buffer = rmh->memblocks[nr_block]->buffer + (offset << 12);
	__ib_convey_page(src_buffer,dst_buffer);
	rw->convey = true;
	atomic_inc(&rmh->batch);

	return true;
}

static void __process_rdma_rpc_commit(struct rdma_memory_handler_t *rmh, struct ibv_wc *_wc)
{
	struct multicast_memory_handler_t *mmh = rmh->multicast_memory_handler; 
	struct recv_work* rw = NULL;
	struct recv_work *safe = NULL;
	bool qcommit = ((_wc->imm_data >>28) == RDMA_OPCODE_QCOMMIT);
	u32 count = ((_wc->imm_data & 0xfffff00) >> 8);
//	static unsigned long long count = 0;
	
	if(list_empty(&mmh->commit_list)){	
		__ib_rdma_send(rmh->rdma,rmh->rpc_mr,rmh->rpc_buffer,1,_wc->imm_data,false);
		return;
	}
	
	if(_wc->wr_id==0){
		debug("FFUCKK\n");
		assert(1);
	}

	if(qcommit){
		while(atomic_read(&rmh->batch) < count);
	}else{
		while(atomic_read(&rmh->batch) < CONFIG_BATCH);
		if(atomic_read(&rmh->batch) > CONFIG_BATCH)
			debug("EROOR : config is %d\n",atomic_read(&rmh->batch));
	}

	pthread_spin_lock(&mmh->lock);
	list_for_each_entry_safe(rw,safe,&mmh->commit_list,list){
		struct ibv_wc *wc = &rw->wc;
		if(!rw->convey){
			if(!ib_convey_page(rmh,wc)){
				rdma_error("Unable to convey Page");
				pthread_mutex_unlock(&rmh->memblock_lock);
				break;
			}
		}		
		rw->convey = false;
		ib_putback_recv_work(mmh->multicast->qp,(struct recv_work*)wc->wr_id);
		list_del_init(&rw->list);
	}

	atomic_set(&rmh->batch,0);
	pthread_spin_unlock(&mmh->lock);
//	debug("[ %lld] %s\n",count,__func__);
	__ib_rdma_send(rmh->rdma,rmh->rpc_mr,rmh->rpc_buffer,1,_wc->imm_data,true);
}

static void ib_process_rdma_completion(struct rdma_memory_handler_t *rmh, struct ibv_wc *wc)
{
	int opcode = ib_get_opcode(wc);	
	
	switch(opcode){
		case RDMA_OPCODE_OPEN:
			__process_rdma_rpc_open(rmh,wc);
			break;
		case RDMA_OPCODE_CLOSE:
			__process_rdma_rpc_close(rmh,wc);
			break;
		case RDMA_OPCODE_TRANSITION:
			__process_rdma_rpc_transition(rmh,wc);
			break;
		case RDMA_OPCODE_CHECK:
			__process_rdma_rpc_check(rmh,wc);
			break;
		case RDMA_OPCODE_COMMIT:
		case RDMA_OPCODE_QCOMMIT:	
			__process_rdma_rpc_commit(rmh,wc);
			break;
		case RDMA_OPCODE_GET:
			 __process_rdma_rpc_get_page(rmh,wc);
		default:
			debug("Unknwon opcode rdma rpc process : %x\n",opcode);
	}

}

static void __process_multicast_rpc_commit(struct multicast_memory_handler_t *mmh, struct ibv_wc *_wc)
{
	struct rdma_memory_handler_t *rmh = mmh->rdma_memory_handler; 
	struct recv_work *rw = NULL;
	struct recv_work *safe = NULL;

	if(list_empty(&mmh->commit_list)){	
		__ib_rdma_send(rmh->rdma,rmh->rpc_mr,rmh->rpc_buffer,1,_wc->imm_data,false);
		return;
	}
	
	list_for_each_entry(rw,&mmh->commit_list,list){
		struct ibv_wc *wc = &rw->wc;
		if(!ib_convey_page(rmh,wc)){
			rdma_error("Unable to convey Page");
			break;
		}
	}
	__ib_multicast_send_detail(mmh->multicast,rmh->rpc_mr,rmh->rpc_mr->addr,1,_wc->imm_data,mmh->multicast_ah
			,mmh->remote_qpn,mmh->remote_qkey);
	
	list_for_each_entry_safe(rw,safe,&mmh->commit_list,list){	
		list_del(&rw->list);
		ib_putback_recv_work(mmh->multicast->qp,(struct recv_work*)rw->wc.wr_id);
	}
}

static void __process_multicast_rpc_flow(struct multicast_memory_handler_t *mmh ,struct ibv_wc *wc)
{
//	int ret = 0;

	struct recv_work *rw = (struct recv_work *)wc->wr_id;
	struct rdma_memory_handler_t * rmh = mmh->rdma_memory_handler;
	memcpy(&rw->wc,wc,sizeof(struct ibv_wc));
	//rw->wc = *wc;
	list_add_tail(&rw->list,&mmh->commit_list);
	
	pthread_spin_lock(&mmh->lock);
	if(!rw->convey){
		ib_convey_page(rmh,wc);
	}
	pthread_spin_unlock(&mmh->lock);

}


static void __process_multicast_rpc_check(struct multicast_memory_handler_t *mmh ,struct ibv_wc *wc)
{
	struct recv_work *rw = (struct recv_work*)wc->wr_id;
	printf("[MULTICAST] : check Messege :  %s recv work id :%ld\n",(char*)(rw->buffer + UD_EXTRA) ,rw->id);
}

static void ib_process_multicast_completion(struct multicast_memory_handler_t *mmh, struct ibv_wc *wc)
{
	int opcode = ib_get_opcode(wc);

	switch(opcode){
		case MULTICAST_OPCODE_FLOW:
			__process_multicast_rpc_flow(mmh,wc);
			break;
		case MULTICAST_OPCODE_CHECK:
			__process_multicast_rpc_check(mmh,wc);
			break;
		case MULTICAST_OPCODE_COMMIT:
			__process_multicast_rpc_commit(mmh,wc);
			break;
		case MULTICAST_OPCODE_NONE :
			break;
		default:
			debug("Unknwon opcode rdma rpc process : %x\n",opcode);
	}

}

static void* rdma_memory_thread(void *context)
{
	
	struct rdma_memory_handler_t* rmh = context;
	struct ibv_cq *recv_cq  = rmh->rdma->recv_cq;
	
	while(*rmh->keep){
		
		int nr_completed;
		struct ibv_wc wc;

		nr_completed = ibv_poll_cq(recv_cq,1,&wc);
		if(nr_completed == 0) continue;
		else if(nr_completed < 0) break;
		
		if(wc.status != IBV_WC_SUCCESS){
			debug("rdma work completion status %s\n",
					ibv_wc_status_str(wc.status));
			break;
		}

		ib_process_rdma_completion(rmh,&wc);
		ib_putback_recv_work(rmh->rdma->qp,(struct recv_work *)wc.wr_id);
	}
	rdma_connection_die();
	return NULL;
}

void* multicast_memory_thread(void *context)
{
	debug("Multicast memoct thread wakeup\n");

	struct multicast_memory_handler_t* mmh = context;
	struct ibv_cq *recv_cq  = mmh->multicast->recv_cq;	
	*mmh->realloc = true;
	
	while(*mmh->keep){
		
		int nr_completed;
		struct ibv_wc wc;

		nr_completed = ibv_poll_cq(recv_cq,1,&wc);
		if(nr_completed == 0) continue;
		else if(nr_completed < 0) break;

		if(wc.status != IBV_WC_SUCCESS){
			debug("multicast work completion status %s\n",
					ibv_wc_status_str(wc.status));
			break;
		}
		ib_process_multicast_completion(mmh,&wc);
	}

	multicast_connection_die();
	return NULL;
}


bool init_handlers(struct rdma_cm_event *event)
{
	struct rdma_memory_handler_t* rmh = 
		alloc_rdma_memory_handler(event);

	if(!rmh){
		rdma_error("Unable to alloc rdma memory handler");
		return false;
	}
	
	struct multicast_memory_handler_t *mmh =
		alloc_multicast_memory_handler();

	if(!mmh){
		rdma_error("Unable to alloc multicast memory handler");
		return false;
	}

	if(!set_up_handlers(rmh,mmh)){
		rdma_error("Unable to set up handlers");
		return false;
	}
	for(int i = 0 ; i < NR_RECVER ;i ++){
		pthread_create(&rmh->thread_id[i],NULL,rdma_memory_thread,rmh);
		pthread_create(&mmh->thread_id[i],NULL,multicast_memory_thread,mmh);
	}
	return true;
}

void stop_handlers(void)
{
	rdma_connection_die();
	multicast_connection_die();
}
void rdma_event_handler(struct rdma_cm_event *event)
{
	switch (event->event) {
		case RDMA_CM_EVENT_CONNECT_REQUEST:
			if(!init_handlers(event))
				stop_handlers();
			break;
		case RDMA_CM_EVENT_ESTABLISHED:
			debug("Connection established\n");
			break;
		case RDMA_CM_EVENT_DISCONNECTED:
			debug("Connection disconnected\n");
			//stop_handlers();
			break;
		case RDMA_CM_EVENT_REJECTED:
			debug("Connection rejected\n");
			break;
		default:
			printf("Unknown event %d\n", event->event);
			break;
	}
}
