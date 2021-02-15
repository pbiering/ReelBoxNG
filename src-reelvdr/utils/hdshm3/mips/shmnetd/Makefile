CROSS=mipsel-linux-gnu-
CC=$(CROSS)gcc
LDFLAGS =

CFLAGS=  -Os -Wall
DEFINES = -D__LINUX__ -DCONFIG_MIPS
INCLUDES = -I../../src/include

shmnetd_OBJECTS= shmnetd.o hdshmlib.o hdchannel.o

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
	$(CROSS)strip $@

clean:
	$(RM)  *.o *~ shmnetd $(DEPFILE)
