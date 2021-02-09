CROSS=
CC=$(CROSS)gcc
LDFLAGS =
LIBS= -lpthread

CFLAGS=  -O3 -Wall
DEFINES = -D__LINUX__
INCLUDES = -I../../src/include

SBINDIR  ?= /usr/sbin

hdctrld_OBJECTS = hdctrld.o hdvideo.o hdshmlib.o

all: hdctrld

.c.o:
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $<

# dependencies
MAKEDEP = $(CC) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)

hdctrld: $(hdctrld_OBJECTS)
	@rm -f hdctrld
	$(CC) $(LDFLAGS) $(LD_EXTRA_FLAGS) $(hdctrld_OBJECTS) $(LDADD) $(LIBS) -o $@
	$(CROSS)strip $@

clean:
	$(RM)  *.o *~ hdctrld $(DEPFILE)

install: hdctrld
	install -sp -m 755 $< $(SBINDIR)
