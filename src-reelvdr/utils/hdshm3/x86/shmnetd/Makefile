CROSS   =
CC      ?= $(CROSS)gcc
LDFLAGS ?= -g

CFLAGS  ?= -g  -O2 -Wall



DEFINES += -D__LINUX__
INCLUDES  = -I../../src/include
INCLUDES += -I../../src/driver

shmnetd_OBJECTS= shmnetd.o hdshmlib.o hdchannel.o

SBINDIR  ?= /usr/sbin

#
# targets
#

all: shmnetd

.c.o:
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $<

# dependencies
MAKEDEP = $(CC) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)

shmnetd: $(shmnetd_OBJECTS)
	@rm -f shmnetd
	$(CC) $(LDFLAGS) $(LD_EXTRA_FLAGS) $(shmnetd_OBJECTS) $(LDADD) $(LIBS) -o $@
#	$(CROSS)strip $@

clean:
	$(RM)  *.o *~ shmnetd $(DEPFILE)

install: shmnetd
	install -sp -m 755 $< $(SBINDIR)

