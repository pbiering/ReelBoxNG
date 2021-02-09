CROSS=mipsel-linux-gnu-
CC=$(CROSS)gcc
LDFLAGS =
LIBS= 

CFLAGS=  -Os -Wall
DEFINES = -D__LINUX__ -DCONFIG_MIPS
INCLUDES = -I../../src/include -I$(MIPS_KERNELPATH)/include

cmdline_OBJECTS= cmdline.o

all: cmdline

.c.o:
	$(CC) $(DEFINES) $(INCLUDES) $(CFLAGS) -c $<

# dependencies
MAKEDEP = $(CC) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)

cmdline: $(cmdline_OBJECTS)
	@rm -f $@
	$(CC) $(LDFLAGS) $(LD_EXTRA_FLAGS) $(cmdline_OBJECTS) $(LDADD) $(LIBS) -o $@
	$(CROSS)strip $@

clean:
	$(RM)  *.o *~ cmdline $(DEPFILE)
