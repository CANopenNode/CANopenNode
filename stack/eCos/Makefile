# Mostly written by Jonathan Larmour, Red Hat, Inc.
# Reference to ecos.mak added by John Dallaway, eCosCentric Limited, 2003-01-20
# This file is in the public domain and may be used for any purpose

# Usage:   make INSTALL_DIR=/path/to/ecos/install

#INSTALL_DIR=$$(INSTALL_DIR) # override on make command line
#INSTALL_DIR=/tmp/lpc-e2294_default_CAN_04_debug_install
#INSTALL_DIR=/tmp/lpc-e2294_ustl_CAN_04_debug_install
INSTALL_DIR=/tmp/lpc-l2294_ustl_CAN_02_debug_install

# path to CANopen node source code
CANOPENNODE_SRC = /cygdrive/c/CodingXP/CANopenNode_SVN/CANopen_stack


# Include directories
INCLUDE_DIRS = $(CANOPENNODE_SRC) \
	-I"src"         
               
# source files
SOURCES = $(CANOPENNODE_SRC)/CANopen.c \
	$(CANOPENNODE_SRC)/CO_Emergency.c \
	$(CANOPENNODE_SRC)/CO_HBconsumer.c \
	$(CANOPENNODE_SRC)/CO_NMT_Heartbeat.c \
	$(CANOPENNODE_SRC)/CO_PDO.c \
	$(CANOPENNODE_SRC)/CO_SDO.c \
	$(CANOPENNODE_SRC)/CO_SDOmaster.c \
	$(CANOPENNODE_SRC)/CO_SYNC.c \
	$(CANOPENNODE_SRC)/crc16-ccitt.c \
	src/application.cpp \
	src/CO_driver_eCos.c \
	src/main.c \
	src/CO_OD.c \
	src/CO_PollingTimer.c \
	src/CO_Flash.c \
	src/ecos_helper.c
    

OBJSC=${SOURCES:%.c=%.o}
OBJS=${OBJSC:%.cpp=%.o}

include $(INSTALL_DIR)/include/pkgconf/ecos.mak

XCC           = $(ECOS_COMMAND_PREFIX)gcc
OBJCOPY       = $(ECOS_COMMAND_PREFIX)objcopy
XCXX          = $(XCC)
XLD           = $(XCC)

CFLAGS        = -I$(INSTALL_DIR)/include -I$(INCLUDE_DIRS)
CXXFLAGS      = $(CFLAGS)
LDFLAGS       = -nostartfiles -L$(INSTALL_DIR)/lib -Ttarget.ld

# RULES

.PHONY: all clean

all: ecos_canopennode  ecos_canopennode.bin

clean:
	-rm -f  src/*.o
	-rm -f  $(CANOPENNODE_SRC)/*.o

%.o: %.c
	$(XCC) -c -o $*.o $(CFLAGS) $(ECOS_GLOBAL_CFLAGS) $<

%.o: %.cxx
	$(XCXX) -c -o $*.o $(CFLAGS) $(ECOS_GLOBAL_CFLAGS) $<
	
%.o: %.cpp
	$(XCXX) -c -o $*.o $(CXXFLAGS) $(ECOS_GLOBAL_CFLAGS) $<

%.o: %.C
	$(XCXX) -c -o $*.o $(CXXFLAGS) $(ECOS_GLOBAL_CFLAGS) $<

%.o: %.cc
	$(XCXX) -c -o $*.o $(CXXFLAGS) $(ECOS_GLOBAL_CFLAGS) $<


ecos_canopennode: $(OBJS)
	$(XLD) $(LDFLAGS) $(ECOS_GLOBAL_LDFLAGS) $(OBJS) -o $@
	
	
ecos_canopennode.bin : ecos_canopennode
	$(OBJCOPY) --strip-debug $< $(@:.bin=.img) 
	$(OBJCOPY) -O srec $< $(@:.bin=.srec)
	$(OBJCOPY) -O binary $< $@

