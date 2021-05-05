# Makefile for CANopenNode, basic compile with blank CAN device


DRV_SRC = .
CANOPEN_SRC = ..
APPL_SRC = .


LINK_TARGET = canopennode_blank


INCLUDE_DIRS = \
	-I$(DRV_SRC) \
	-I$(CANOPEN_SRC) \
	-I$(APPL_SRC)


SOURCES = \
	$(DRV_SRC)/CO_driver_blank.c \
	$(DRV_SRC)/CO_storageBlank.c \
	$(CANOPEN_SRC)/301/CO_ODinterface.c \
	$(CANOPEN_SRC)/301/CO_NMT_Heartbeat.c \
	$(CANOPEN_SRC)/301/CO_HBconsumer.c \
	$(CANOPEN_SRC)/301/CO_Emergency.c \
	$(CANOPEN_SRC)/301/CO_SDOserver.c \
	$(CANOPEN_SRC)/301/CO_TIME.c \
	$(CANOPEN_SRC)/301/CO_SYNC.c \
	$(CANOPEN_SRC)/301/CO_PDO.c \
	$(CANOPEN_SRC)/303/CO_LEDs.c \
	$(CANOPEN_SRC)/305/CO_LSSslave.c \
	$(CANOPEN_SRC)/storage/CO_storage.c \
	$(CANOPEN_SRC)/CANopen.c \
	$(APPL_SRC)/OD.c \
	$(DRV_SRC)/main_blank.c


OBJS = $(SOURCES:%.c=%.o)
CC ?= gcc
OPT =
OPT += -g
#OPT += -DCO_USE_GLOBALS
#OPT += -DCO_MULTIPLE_OD
CFLAGS = -Wall $(OPT) $(INCLUDE_DIRS)
LDFLAGS =


.PHONY: all clean

all: clean $(LINK_TARGET)

clean:
	rm -f $(OBJS) $(LINK_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LINK_TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
