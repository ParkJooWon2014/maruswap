# maruswap


This Maruswap. 


## Far memory server (far memory node)

To build and run the far memory server do:

```
vi ./include/config 
``` 


```

#define CONFIG_SERVER_IP "10.10.1.13".  // Destination to msg server
#define CONFIG_SERVER_PORT 50000	//  server Port
#define CONFIG_MULTI_IP "10.10.1.13"
#define MY_ADDR "10.10.1.12"

enum {
	NR_THREAD  = 1,
	PRE_ALLOC_MEMBLOCK =  30,
};

```

First you should modified those config. ``` MULTICAST_IP ```  and ``` MY_ADDR ``` should be modified.  
-``` MULTICAST_IP ```  is IP to join multicast group. If you join multicast , You should same IP. ```MY_ADDR``` is your InfiniBand IP using ```ib_ipoib``` . Before build maruswap, check ```lsmod | grep ib_ipoib```. 

-``` PRE_ALLOC_MEMBLOCK``` decides number of memblock allocated when make connection to client. And build and use it!.

	make
	cd server
	./server -p $(serverport)

## maruswap (Clinet node)


