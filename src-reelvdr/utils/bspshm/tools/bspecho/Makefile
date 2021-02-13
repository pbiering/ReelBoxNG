LDFLAGS = -m32 -lpthread -lm -lglcdgraphics
CXXFLAGS ?=  -O2 -m32 -Wall
DEFINES = -D__LINUX__ 
INCLUDES = -I../../include -I../../../../libs/graphlcd-base/

LD_EXTRA_FLAGS ?= \
        -L../../../../libs/graphlcd-base/glcdgraphics \
        -L../../../../libs/graphlcd-base/glcddrivers

bspecho_OBJECTS= bspecho.o bspshmlib.o bspchannel.o

all: bspecho

.c.o:
	$(CXX) $(DEFINES) $(INCLUDES) $(CXXFLAGS) -c $<

# dependencies
MAKEDEP = $(CXX) -MM -MG 
DEPFILE = .dependencies
$(DEPFILE): Makefile
	$(MAKEDEP) $(DEFINES) $(INCLUDES) *.c > $@

-include $(DEPFILE)

bspecho: $(bspecho_OBJECTS)
	@rm -f bspecho
	$(CXX) $(LDFLAGS) $(LD_EXTRA_FLAGS) $(bspecho_OBJECTS) $(LDADD) $(LIBS) -o $@
	strip $@

clean:
	$(RM)  *.o *~ bspecho $(DEPFILE)

