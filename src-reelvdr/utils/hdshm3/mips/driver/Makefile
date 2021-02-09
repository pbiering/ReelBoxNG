
CROSSPREFIX  = mipsel-linux-gnu-


CC?=${CROSSPREFIX}gcc
LD?=${CROSSPREFIX}ld

ifeq ($(KERNELRELEASE),)
 KERNELRELEASE= $(shell uname -r)
endif

ifneq ($(findstring 2.6,$(KERNELRELEASE)),)
 export-objs:=
 list-multi:=
 KMODSUF:= ko
else
 KMODSUF:= o
endif

ifeq ($(DESTDIR),)
 DESTDIR=
endif

MODULEPATH   = /lib/modules/${KERNELRELEASE}/extra

#bspshm-objs := bspshm.o

obj-m = hdshm.o

-include $(KERNELPATH)/Rules.make

all: KERNELPATH = $(MIPS_KERNELPATH)
all:
	echo $(KERNELPATH)
	$(MAKE) V=1 -C $(KERNELPATH) SUBDIRS=$(shell pwd) modules
	cp hdshm.ko hdshm_mips.ko


clean:
	$(RM) *~ *.ko hdshm.ko.cmd *.o.cmd *.o.cmd *.mod.c *.mod.o *.o .*cmd
	$(RM) -r .tmp_versions  $(DEPFILE)
