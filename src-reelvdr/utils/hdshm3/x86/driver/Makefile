ifndef CROSSPREFIX
 CROSSPREFIX=
endif

CC?=${CROSSPREFIX}gcc
LD?=${CROSSPREFIX}ld

ifeq ($(KERNELRELEASE),)
 KERNELRELEASE= $(shell uname -r)
endif

KERNEL_VERSION=$(shell uname -r | awk -F. '{ print $$1 * 65536 + $$2 * 256 + $$3 }')
KERNEL_VERSION_TEST=$(shell echo "2.6.6" | awk -F. '{ print $$1 * 65536 + $$2 * 256 + $$3 }')

KERNEL_FLAG_TEST := $(shell if [ $(KERNEL_VERSION) -gt $(KERNEL_VERSION_TEST) ]; then echo 1; fi)

ifdef KERNEL_FLAG_TEST
 export-objs:=
 list-multi:=
 KMODSUF:= ko
 KMAKEOPTION = M
else
 KMODSUF:= o
 KMAKEOPTION = SUBDIRS
endif

ifeq ($(KERNELPATH),)
 KERNELPATH=    /lib/modules/${KERNELRELEASE}/build
endif

ifeq ($(DESTDIR),)
 DESTDIR=
endif

MODULEPATH=     /lib/modules/${KERNELRELEASE}/extra

#bspshm-objs := bspshm.o

obj-m = hdshm.o

-include $(KERNELPATH)/Rules.make

all:	var-test
	@echo KERNEL_VERSION=$(KERNEL_VERSION)
	@echo KERNELPATH=$(KERNELPATH)
	$(MAKE) V=1 -C $(KERNELPATH) $(KMAKEOPTION)=$(shell pwd) modules
	cp hdshm.ko hdshm_host.ko

var-test:
	@echo "KERNEL_VERSION     =$(KERNEL_VERSION)"
	@echo "KERNEL_VERSION_TEST=$(KERNEL_VERSION_TEST)"
	@echo "KERNEL_FLAG_TEST   =$(KERNEL_FLAG_TEST)"
	@echo "KMAKEOPTION        =$(KMAKEOPTION)"
	@echo "KMODSUF            =$(KMODSUF)"
	@echo "KERNELPATH         =$(KERNELPATH)"
	@echo "MODULEPATH         =$(MODULEPATH)"


install: hdshm.ko
	sudo install -p -D $^ /lib/modules/$(KERNELRELEASE)/kernel/drivers/video/
	install -p -D $^ ../../../../../packages/hdshm-modules-2.6.32/ROOT/lib/modules/$(KERNELRELEASE)/kernel/drivers/video/$^
	sudo depmod -a

clean:
	$(RM) *~ *.ko *.ko.cmd *.o.cmd *.o.cmd *.mod.c *.mod.o *.o .*cmd
	$(RM) -r .tmp_versions 
