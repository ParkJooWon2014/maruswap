# maruswap


This Maruswap. 


## server 

how to use Server ?? 

	make
	cd server
	./server 

how t



```

vi ./include/config 



#define CONFIG_SERVER_IP "10.10.1.13"
#define CONFIG_SERVER_PORT 50000
#define CONFIG_MULTI_IP "10.10.1.13"
#define MY_ADDR "10.10.1.12"

enum {
	NR_RECVER = 1,
	PRE_ALLOC_MEMBLOCK =  30,
};

```




1. first Change ''' MY_ADDR ''' to your Infiniband ip_addr. If you don't know ip, Use command 'ip addr'.Or you maybe forget 'modprobe ib_ipoib'

2.
