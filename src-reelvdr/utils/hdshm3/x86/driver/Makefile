ifndef CROSSPREFIX
 CROSSPREFIX=
endif

CC?=${CROSSPREFIX}gcc
LD?=${CROSSPREFIX}ld

ifeq ($(KERNELRELEASE),)
 KERNELRELEASE= $(shell uname -r)
endif

ifneq ($(findstring 2.6,$(KERNELRELEASE)),)
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

all:
	echo $(KERNELPATH)
	$(MAKE) V=1 -C $(KERNELPATH) $(KMAKEOPTION)=$(shell pwd) modules
	cp hdshm.ko hdshm_host.ko

install: hdshm.ko
	sudo install -p -D $^ /lib/modules/$(KERNELRELEASE)/kernel/drivers/video/
	install -p -D $^ ../../../../../packages/hdshm-modules-2.6.32/ROOT/lib/modules/$(KERNELRELEASE)/kernel/drivers/video/$^
	sudo depmod -a

clean:
	$(RM) *~ *.ko *.ko.cmd *.o.cmd *.o.cmd *.mod.c *.mod.o *.o .*cmd
	$(RM) -r .tmp_versions 
