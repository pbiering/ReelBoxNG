include ../../Make.config

LIRC_DEVICE       = /dev/lircd

ifndef RBMINI
  UBUNTU10 := $(shell [ $$(lsb_release -sr | cut -f1 -d. ) -ge 10 ] && echo true || echo false )
endif

ifeq ($(UBUNTU10),true)
    USER_INPUT_DEVICE := /dev/uinput
else
    USER_INPUT_DEVICE := /dev/input/uinput
endif
BINPATH = /usr/sbin

INCLUDES =
DEFINES += -g -DUSER_INPUT_DEVICE=\"$(USER_INPUT_DEVICE)\"
DEFINES += -DFP_DEVICE=\"$(FP_DEVICE)\"
LDFLAGS += -lpthread -lusb



ifdef RBMINI
    rb_control_OBJECTS = rb_control.o frontpanel.o frontpanel_nc.o frontpanel_rs232.o frontpanel_usb.o daemon.o  external_ctrl.o
    CFLAGS +=  -Wno-unused-variable
else
    rb_control_OBJECTS = rb_control.o frontpanel.o frontpanel_ice.o frontpanel_rs232.o  frontpanel_usb.o daemon.o fan_control.o external_ctrl.o
    CFLAGS += -Wno-unused-result -Wno-unused-variable
endif

######## rules

all: reelbox-ctrld

reelbox-ctrld: $(rb_control_OBJECTS) 
	$(CXX) $(rb_control_OBJECTS) $(LDADD) $(LIBS) $(LDFLAGS) -o $@

.c.o:
	$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) -c $<

clean:
	$(RM)  *.o *~ reelbox-ctrld

install: reelbox-ctrld
	install -p -m 755 $< $(BINPATH)
