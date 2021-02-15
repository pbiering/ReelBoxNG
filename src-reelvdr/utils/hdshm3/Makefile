#
# Makefile for ReelBox tools
#
#

#export CMS_SDK = cms-sdk-1.4.32
#export SDKNAME = DECYPHER-SDK-1-4-0-RC7
#export SDKROOT = ${HOME}/temp/${CMS_SDK}/${SDKNAME}
#export SDKROOT = /home/acher/exp/reelbox/cms/${CMS_SDK}/${SDKNAME}

#export CMS1080ROOT=${CMS_SDK_INSTALLDIR}/${CMS_SDK_VERSION}/cms   ### unused now?

export SDKROOT = /usr/src
export MIPS_KERNELPATH = $(SDKROOT)/linux-2.6.12.6-wis

export TOOLS=/opt/wischip/mipsel-linux-gnu/gcc-3.3.4-glibc-2.3.2/bin
export PATH := $(PATH):$(TOOLS)

X86_TARGETS  ?= x86/driver x86/hdboot x86/hdctrld x86/hdfbctl x86/shmnetd 
#x86/test

MIPS_TARGETS ?= mips/driver mips/hdctrld mips/shmnetd 
#mips/test


-include ../Make.config

ifdef RBLITE
  export CC  = gcc-3.3
  export CXX = g++-3.3
else
  export CC  = gcc
  export CXX = g++
endif


SBINDIR  := /usr/sbin
LRMDIR   := /lib/linux-restricted-modules/`uname -r`
export SBINDIR LRMDIR

default: x86

all: x86 mips


x86: $(X86_TARGETS)
	for i in $^ ; do \
		$(MAKE) -C $$i || exit 1;\
	done


mips: $(MIPS_TARGETS)
	for i in $^ ; do \
		$(MAKE) -C $$i || exit 1;\
	done

install: install-x86

install-x86: $(X86_TARGETS)
	for i in $^ ; do \
		$(MAKE) -C $$i install || exit 1;\
	done

clean:
	#echo $(PATH)
	@for i in $(X86_TARGETS) $(MIPS_TARGETS); do \
		$(MAKE) -C $$i clean ;\
	done

