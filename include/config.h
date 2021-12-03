

#ifndef __CONFIG_H__
#define __CONFIG_H__


#define CONFIG_SERVER_IP "10.10.1.12"
#define CONFIG_SERVER_PORT 50000
#define CONFIG_MULTI_IP "10.10.1.13"
#define MY_ADDR "10.10.1.17"

enum {
	NR_RECVER = 1,
	NR_WORKER = 1,

	PRE_ALLOC_MEMBLOCK = 30,
};

#endif 
