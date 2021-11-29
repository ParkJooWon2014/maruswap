# Maruswap


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
-```MULTICAST_IP ```  is IP to join multicast group. If you join multicast , You should same IP. ```MY_ADDR``` is your InfiniBand IP using ```ib_ipoib``` . Before build maruswap, check ```lsmod | grep ib_ipoib```. 

-```PRE_ALLOC_MEMBLOCK``` decides number of memblock allocated when make connection to client. And build and use it!. defualt server port is 50000.

	make
	cd server
	./server -p $(serverport)


## MaRuswap driver (Clinet node)

Build do like this

``` 
cd drive
make 
```
Before you install this modules, the servers have to prepare ```./server -p ``` to connect server.

```
sudo insmod sib.ko fip="@first_server_ip" sip="@sub_server_ip" myip="@Client_ip" sport=@first_and_sub_server_port_num mip="multicast_ip"
sudo insmod maruswap.ko
```

