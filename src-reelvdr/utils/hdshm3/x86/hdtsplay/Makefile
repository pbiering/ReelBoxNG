#SDK      = /home/acher/exp/reelbox/cms/DECYPHER-SDK-1-4-2-RC3/
HDSHM    = ../../src/

CC        ?= gcc
CFLAGS    ?= -Wall -O3 -funroll-loops
INCLUDES  = -I. -I${HDSHM}/include  -I${HDSHM}
LDFLAGS   = -lpthread
#LDADD     =

MODULES = hdtsplay.o hdshmlib.o hdchannel.o

DEPFILE := .dependencies
MAKEDEP := $(CC) -MM -MG

PATH    += :$(MIPSEL)/bin

all: hdtsplay

$(DEPFILE):
	$(MAKEDEP) $(INCLUDES) $(MODULES:%.o=%.c) > $@

-include $(DEPFILE)


hdtsplay: $(MODULES)
	@rm -f $@
	$(CC) $(LDFLAGS) $(MODULES) $(LDADD) $(LIBS) -o $@

.c.o:
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $<

clean:
	$(RM) *.o *~ hdtsplay $(DEPFILE)
