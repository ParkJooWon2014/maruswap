
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

/*
 * TEST CODE
 * CODE FOR TEST
 * 
 * */
extern struct rdma_connection_manager_t rcm ;
extern struct multicast_connection_manager_t mcm;

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

	char *lnum = (char*)malloc(RPC_MAX_EACH_SIZE);
	struct ibv_mr *mr = ibv_reg_mr(
			mcm.multicast->pd,lnum,RPC_MAX_EACH_SIZE,	
			IBV_ACCESS_LOCAL_WRITE);
	uint32_t header =( (MULTICAST_OPCODE_FLOW << 28) | (1UL <<23));
	for(int i = 0 ; i < 10 ; i ++){
		rnum = rand();	
		snprintf(lnum,RPC_MAX_EACH_SIZE,"%d",rnum);
		if(!__ib_multicast_send(mcm.multicast,mr,lnum,RPC_MAX_EACH_SIZE,header)){
			return;
		}
		printf("CHECK MESS[%d] : %s\n",i,lnum);
	}

	ibv_dereg_mr(mr);
	free(lnum);
}

void test_rpc_open(void)
{
	struct rdma_open_t list[30];
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


	for(int i = 0 ; i < 30 ; i++){
		uint32_t opcode = 0 ; 
		opcode = (RDMA_OPCODE_OPEN << 28);

		size_t soffset = ((1UL << (30)) * i);
		uint32_t offset  = (soffset >> 12) ;
		opcode |= offset;
		ib_rpc(rcm.rdma,opcode,req,sizeof(*req),res,sizeof(*req),mr);		
		memcpy(list+i,res,sizeof(*req));
		printf("[RPC] : remote addr : %lx rkey : %x\n",list[i].remote_addr,list[i].rkey);
		//sleep(1);
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
	int numlist[1UL << 18];

	uint64_t nr_block;
	int send_count = 0;
	
	clock_gettime(CLOCK_REALTIME, &begin);
	volatile size_t batch = 0 ;
	for(u64 a = 0 ;a < 1; a++){
		for(int i = 0 ; i < (1UL << 18)  ; i ++){
			u64 roffset = ((1UL << 12) * i |  (a << 30));
			//u32 offset = (1<< 12) *i;
			nr_block = a;
			uint32_t header = ((MULTICAST_OPCODE_FLOW << 28) | ((roffset>>12) & 0xfffffff));

			rnum = rand();	
			numlist[i %(1ul << 18)] = rnum;

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

			if(batch % CONFIG_BATCH == 0){
				uint32_t opcode = 0 ; 
				opcode |= (RDMA_OPCODE_COMMIT << 28);
				debug("_____--=====\n");
				ib_rpc(rcm.rdma,opcode,req,sizeof(*req),res,sizeof(*req),mr);
				
				for(int ia = send_count; ia < batch; ia++){
					clock_gettime(CLOCK_REALTIME,&read_begin);
					if(!__ib_rdma_read(rcm.rdma,scan_mr,scan_buffer,PAGE_SIZE,
								(void*)list[nr_block].remote_addr + ia * (1UL <<12) ,list[nr_block].rkey,false)){
						break;
					}
					//debug("%s\n",(char*)scan_buffer);
					if(numlist[ia %(1UL <<18)] != atoi(scan_buffer)){
						debug("nr_block: %ld  addr :%x\n",nr_block,ia);
						debug("%s\n",(char*)scan_buffer);
						debug("==============FUCK===============\n");
						return;
					}
					clock_gettime(CLOCK_REALTIME,&read_end);	
					interval_read_time += ((read_end.tv_sec - read_begin.tv_sec) + (read_end.tv_nsec - read_begin.tv_nsec)/ 1000000000.0);
				} 
				send_count = batch;
		//		printf("batch_count is %d\n",batch_count++);
			}
			send_count = batch = 0;
		}
		/*
		//printf("CHECK MESS[%d] : %s\n",i,lnum);
		if(!__ib_rdma_read_scan(rcm.rdma,scan_mr,scan_buffer,(PAGE_SIZE+8),
		(void*)list[nr_block].remote_addr + offset + (offset >>12)*8 ,list[nr_block].rkey,false)){
		break;
		}
		if(strncmp(lnum,scan_buffer+8,100)){
		printf(" %s : %s\n",lnum,(char*)(scan_buffer)+1);
		}

		U*/
		//		debug("Inter val Time (Micro): %lf\n", (double)interval_time/1000000);
     batch = 0;
	 send_count = 0;
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
	//		return;
}
