CONFIG_MODULE_SIG=n
obj-m += slab_allocator.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

	
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
