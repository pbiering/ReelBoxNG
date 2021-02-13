LDFLAGS = -m32 
CXXFLAGS ?=  -O2 -m32 -Wall
DEFINES = -D__LINUX__ 
INCLUDES = -I../../include 

LD_EXTRA_FLAGS = 
osddump_OBJECTS= osddump.o bspshmlib.o bspchannel.o

all: osddump

.c.o:
	$(CXX) $(DEFINES) $(INCLUDES)  $(CXXFLAGS) -c $<

# dependencies
MAKEDEP = $(CXX) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)


osddump: $(osddump_OBJECTS)
	@rm -f osddump
	$(CXX) $(LDFLAGS) $(LD_EXTRA_FLAGS) $(osddump_OBJECTS) $(LDADD) $(LIBS) -o $@
	strip $@


clean:
	$(RM)  *.o *~ osddump $(DEPFILE)
