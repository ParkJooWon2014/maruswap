

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "common.h"

#include <ib.h>
#include <rpc.h>
#include <memblock.h>
#include <config.h>



bool init_client(const char *server_ip, const unsigned short server_port)
{
	init_rdma_connection_manager();

	if(!ib_rdma_client_connect(server_ip,server_port)){
		debug("Unalbe to rdma client_connect\n");
		return false;
	}
	printf("[RDMA] : Connection is Success\n");
	return true;
}
bool init_multicast(const char *multi_ip)
{
	init_multicast_connection_manager();
	if(!ib_connect_multicast(multi_ip,CLIENT)){
		debug("Unalbe to multicast connect\n");
		return false;
	}
	printf("[MULTICAST] : Connection is Success\n");
	return true;
}

int main(int argc, char * const argv[])
{
	int opt;
	char *server_ip = CONFIG_SERVER_IP;
	unsigned short server_port = CONFIG_SERVER_PORT;
	char * multi_ip = CONFIG_MULTI_IP;	
	//short nr_connection = CONFIG_NR_CONNECTION;

	while ((opt = getopt(argc, argv, "s:p:n:")) != -1) {
		switch(opt) {
		case 's':
			server_ip = strdup(optarg);
			break;
		case 'p':
			server_port = atoi(optarg);
			break;
		default:
			printf("Unknown parameter %c\n", opt);
			exit(EXIT_FAILURE);
		}
	}

	if(!init_client(server_ip,server_port)){
		debug("Unable to init client\n");
		return EXIT_FAILURE;
	}
	
	if(!init_multicast(multi_ip)){
		debug("Unable to init multicast");
		return EXIT_FAILURE;
	}
	
/*
	if(!init_client("10.10.1.17",server_port)){
		debug("Unable to init client\n");
		return EXIT_FAILURE;
	}
*/	
	sleep(2);

	test_rpc_open();

//	test_multicast_send_flow();
	
	//	sleep(10);
/*
	struct memory_sender_t *ms = alloc_memory_sender();

	uint32_t opcode = 0 ; 
	opcode = (RDMA_OPCODE_OPEN << 28);

	uint32_t offset  = 0x0; //(soffset >> 12) ;
	opcode |= offset;
	
	struct rdma_open_t *req = malloc(sizeof(*req)*2);
	struct rdma_open_t *res = req + 1;

	struct ibv_mr * mr = ibv_reg_mr(ms->rdma->pd,req,sizeof(*req)*2,
			IBV_ACCESS_LOCAL_WRITE |
			IBV_ACCESS_REMOTE_WRITE |
			IBV_ACCESS_REMOTE_READ |
			IBV_ACCESS_REMOTE_ATOMIC);

	ib_rpc(ms->rdma,opcode,req,sizeof(*req),res,sizeof(*req),mr);

	printf("[RPC] : remote addr : %lx rkey : %x\n",res->remote_addr,res->rkey);
	memcpy(ms->remote_info,res,sizeof(*req));

	ib_scan(ms);

*/

//	test_multicast_send_flow();
	return 0;
}
