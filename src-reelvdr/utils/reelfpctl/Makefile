include ../../Make.config

LD_EXTRA_FLAGS = \
	-L ../../libs/graphlcd-base/glcdgraphics \
	-L ../../libs/graphlcd-base/glcddrivers

LIBS     = -lusb -lpthread
GLCD_LIBS = -lglcdgraphics -lglcddrivers 


INCLUDES = -I ../../libs/graphlcd-base

DEFINES += -DFP_DEVICE=\"$(FP_DEVICE)\"
DEFINES += -DFONTPATH=\"$(FONTPATH)\"
DEFINES += -DFPCTL

ifdef RBMINI   
   reelfpctl_OBJECTS= reelfpctl.o frontpanel.o frontpanel_nc.o frontpanel_usb.o

all: reelfpctl

else
   reelfpctl_OBJECTS= reelfpctl.o frontpanel.o frontpanel_rs232.o frontpanel_ice.o frontpanel_usb.o
ifdef REELVDR64
LDFLAGS   =
CXXFLAGS ?= -O3
else
LDFLAGS   = -m32
CXXFLAGS ?= -m32 -O3
CXX_STATIC_FLAGS = -static
endif
ifdef GRAPHLCD_NEW
DEFINES += -DGRAPHLCD_NEW
endif
CXXFLAGS += -Wno-unused-result -Wno-unused-variable

all: reelfpctl reelfpctl1

endif

ifdef RBMINI

reelfpctl: reelfpctl.c frontpanel.c frontpanel_nc.c frontpanel_usb.c
	@rm -f $@ *.o
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c reelfpctl.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel_nc.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel_usb.c
	$(CXX) $(LDFLAGS) $(LIBS) $(reelfpctl_OBJECTS) -o $@
	$(STRIP) $@

else

reelfpctl: reelfpctl.c frontpanel.c frontpanel_rs232.c frontpanel_ice.c frontpanel_usb.c
	@rm -f $@ *.o
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c reelfpctl.c
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c frontpanel.c
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c frontpanel_rs232.c
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c frontpanel_ice.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel_usb.c
	$(CXX) $(LDFLAGS) $(LIBS) $(LD_EXTRA_FLAGS) $(reelfpctl_OBJECTS) $(GLCD_LIBS) -o $@
#	$(STRIP) $@

reelfpctl1: reelfpctl.c frontpanel.c frontpanel_rs232.c frontpanel_ice.c frontpanel_usb.c
	@rm -f $@ *.o
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c reelfpctl.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel_rs232.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel_ice.c
	$(CXX) $(CXXFLAGS) $(DEFINES) -DNOGLCD $(INCLUDES) -c frontpanel_usb.c	
	$(CXX) $(LDFLAGS) $(CXX_STATIC_FLAGS) $(reelfpctl_OBJECTS) $(LIBS) -o $@
#	$(STRIP) $@

endif

install: reelfpctl
	install -m755 -D $< /sbin

clean:
	$(RM) reelfpctl reelfpctl1 reelfpctl2 *.o *~
