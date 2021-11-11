KDIR ?= /lib/modules/`uname -r`/build



default:
	$(MAKE) -C $(KDIR) M=$$PWD


load:
	./test.sh
	sudo insmod mbswap.ko
unload:
	sudo rmmod mbswap.ko
	./untest.sh

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean


