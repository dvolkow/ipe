KDIR := /lib/modules/$(shell uname -r)/build
ccflags-y += -DIPE_DEBUG=1 -Wall 
obj-m += ipe.o 
ipe-y = ipeDrv.o ipeDebug.o 

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	rm -rf *.o *.mod.* *.cmd .module* modules* Module* .*.cmd .tmp* ./a.out
disclean:
	rm -rf *.o *.mod.* *.cmd .module* modules* Module* .*.cmd .tmp* ./a.out *.ko

