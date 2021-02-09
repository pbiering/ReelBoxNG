CROSS=
CC=$(CROSS)gcc
LDFLAGS =
LIBS= -lpthread

CFLAGS=  -O3 -Wall
DEFINES = -D__LINUX__
INCLUDES = -I../../src/include

hdfbctl_OBJECTS= hdfbctl.o hdshmlib.o

all: hdfbctl

.c.o:
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $<

# dependencies
MAKEDEP = $(CC) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)

hdfbctl: $(hdfbctl_OBJECTS)
	@rm -f hdfbctl
	$(CC) $(LDFLAGS) $(LD_EXTRA_FLAGS) $(hdfbctl_OBJECTS) $(LDADD) $(LIBS) -o $@
	$(CROSS)strip $@

clean:
	$(RM)  *.o *~ hdfbctl $(DEPFILE)

install: hdfbctl
	install -sp -m 755 $< $(SBINDIR)
