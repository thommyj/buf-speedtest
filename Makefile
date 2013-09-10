obj-m := memalloc.o
PWD := $(shell pwd)
PREFIX := arm-linux-gnueabihf-
CC := $(PREFIX)gcc

default: userspace module

module:
	$(MAKE) -C $(KERNEL_SRC) SUBDIRS=$(PWD) modules
userspace: speedtest.c
	$(CC) speedtest.c -o speedtest

clean:
	rm *.o *.ko
