CC       ?= gcc
CFLAGS   ?= -O2 -Wall
LDFLAGS  ?= 



DEFINES  += -D__LINUX__
INCLUDES  = -I../../src/include
INCLUDES += -I../../src/driver

hdboot_OBJECTS= hdboot.o

all: hdboot

.c.o:
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $<

# dependencies
MAKEDEP = $(CC) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)

hdboot: $(hdboot_OBJECTS)
	$(CC) $(LDFLAGS) $(LD_EXTRA_FLAGS) $< $(LDADD) $(LIBS) -o $@
	strip $@

clean:
	$(RM)  *.o *~ hdboot $(DEPFILE)

install: hdboot
	install -sp -m 755 $< $(SBINDIR)

