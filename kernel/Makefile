KDIR := /lib/modules/$(shell uname -r)/build
ccflags-y += -DEMZ_DEBUG=1 -Wall 
obj-m += nlKernRec.o

all:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules
clean:
	rm -rf *.o *.mod.* *.cmd .module* modules* Module* .*.cmd .tmp* ./a.out
