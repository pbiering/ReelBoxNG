#
# Makefile for ReelBox tools
#
#


X86_TARGETS  ?= bspd driver tools/bspecho tools/osddump

-include ../../Make.config


default: x86

all: x86 mips


x86: $(X86_TARGETS)
	for i in $^ ; do \
		$(MAKE) -C $$i || exit 1;\
	done \


mips: $(MIPS_TARGETS)
	for i in $^ ; do \
		$(MAKE) -C $$i || exit 1;\
	done \


clean:
	@for i in $(X86_TARGETS) $(MIPS_TARGETS); do \
                $(MAKE) -C $$i clean ;\
        done

