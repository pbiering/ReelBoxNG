#LDFLAGS  = -m32 -lpthread -lm
#CXXFLAGS  ?= -O2 -m32 -Wall
LDFLAGS  = -lpthread -lm
CXXFLAGS  ?= -O2 -Wall
DEFINES  = -D__LINUX__ 
INCLUDES = -I../include

bspd_OBJECTS= bspd.o bspshmlib.o bspchannel.o videomodes.o Focus.o

all: bspd

# implicit rules
.c.o:
	$(CXX) $(DEFINES) $(INCLUDES) $(CXXFLAGS) -c $<

# dependencies
MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)


bspd: $(bspd_OBJECTS)
	@rm -f bspd
	$(CXX) $(LDFLAGS) $(bspd_OBJECTS) $(LDADD) $(LIBS) -o $@
	strip $@

clean:
	$(RM)  *.o *~ bspd $(DEPFILE)
