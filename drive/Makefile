KDIR = /lib/modules/`uname -r`/build

default:
	$(MAKE) -C $(KDIR) M=$$PWD 

load:
	./test.sh
	sudo insmod maruswap.ko

unload:
	sudo rmmod maruswap.ko
	./untest.sh

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

