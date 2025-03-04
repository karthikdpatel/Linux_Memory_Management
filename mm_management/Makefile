CONFIG_MODULE_SIG=n
obj-m += mm_simulatorko.o

mm_simulatorko-objs := mm_simulator.o mm/mm_management.o mm/mm_page_frame.o mm/mm_swap_space.o
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

	
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) clean
