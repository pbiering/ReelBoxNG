CROSS=mipsel-linux-gnu-
CC=$(CROSS)gcc
LDFLAGS =
LIBS= -lpthread 

CFLAGS=  -O3 -Wall
DEFINES = -D__LINUX__ -DCONFIG_MIPS
INCLUDES = -I../../src/include -I$(MIPS_KERNELPATH)/include

hdctrld_OBJECTS= hdctrld.o hdvideo.o hdshmlib.o decypher_tricks.o
LD_EXTRA_FLAGS += -lm

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
