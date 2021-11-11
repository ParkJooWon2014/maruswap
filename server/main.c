

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


#include "common.h"

#include <ib.h>
#include <rpc.h>
#include <memblock.h>
#include <config.h>





bool init_server(const unsigned short server_port,
		const char *multi_ip)
{
	init_rdma_connection_manager();
	init_multicast_connection_manager();

	if(!ib_rdma_server_connect(server_port)){
		debug("Unalbe to rdma server connect\n");
		return false;
	}

	printf("RDMA  connect is success\n");
	
	if(!ib_connect_multicast(multi_ip,SERVER)){
		debug("Unalbe to multicast connect\n");
		return false;
	}
	printf("MULTICAST connect is success\n");
	return true;
}

void server_process(void)
{
	ib_pevent();
}

int main(int argc, char * const argv[])
{
	int opt;
//	char *server_ip = CONFIG_SERVER_IP;
	unsigned short server_port = CONFIG_SERVER_PORT;
	char * multi_ip = CONFIG_MULTI_IP;	

	while ((opt = getopt(argc, argv, "p:")) != -1) {
		switch(opt) {
		case 'p':
			server_port = atoi(optarg);
			break;
		default:
			printf("Unknown parameter %c\n", opt);
			exit(EXIT_FAILURE);
		} 
	}

	if(!init_server(server_port,multi_ip)){
		debug("Unable to init client\n");
		return EXIT_FAILURE;
	}
	server_process();

	return 0;
}
