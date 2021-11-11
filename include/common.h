/*
 * JooWon work now in HD
 *
 * */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <stdio.h>
#include <stdint.h>
#define true 1
#define false 0

enum{
	
	RDMA = true,
	MULTICAST = false,

	SERVER = true,
	CLIENT = false,
	

	PAGE_SHIFT = 12,
	PAGE_SIZE = (1U << PAGE_SHIFT),

};


#define board(h, f, a...) { \
    fprintf(stdin, "[%s] " f, h, ##a); \
}

#define rdma_error(msg, args...) do {\
	fprintf(stderr, "%s : %d : ERROR : "msg, __FILE__, __LINE__, ## args);\
	fprintf(stderr,"\n");\
}while(0);

#define debug(msg, args...) do {\
    printf("DEBUG: "msg, ## args);\
}while(0);


#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof((*arr)))

#define container_of(ptr, type, member) ({              \
    void *__mptr = (void *)(ptr);                   \
    ((type *)(__mptr - offsetof(type, member))); })


#define min(x,y)    ({ (x) < (y) ? (x) : (y); })
#define max(x,y)    ({ (x) > (y) ? (x) : (y); })
#define diff(x,y)   ({ max(x,y) - min(x,y); })

typedef uint8_t bool;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned long pgoff_t;


#endif
