# Makefile for CANopenNode, basic compile with blank CAN device


DRV_SRC = .
CANOPEN_SRC = ..
OD_SRC = .
APPL_SRC = .


LINK_TARGET = canopennode_blank


INCLUDE_DIRS = \
	-I$(DRV_SRC) \
	-I$(CANOPEN_SRC) \
	-I$(OD_SRC) \
	-I$(APPL_SRC)


SOURCES = \
	$(DRV_SRC)/CO_driver_blank.c \
	$(DRV_SRC)/eeprom.c \
	$(CANOPEN_SRC)/301/CO_SDOserver.c \
	$(CANOPEN_SRC)/301/CO_Emergency.c \
	$(CANOPEN_SRC)/301/CO_NMT_Heartbeat.c \
	$(CANOPEN_SRC)/301/CO_HBconsumer.c \
	$(CANOPEN_SRC)/301/CO_SYNC.c \
	$(CANOPEN_SRC)/301/CO_PDO.c \
	$(CANOPEN_SRC)/301/CO_TIME.c \
	$(CANOPEN_SRC)/301/CO_SDOclient.c \
	$(CANOPEN_SRC)/301/crc16-ccitt.c \
	$(CANOPEN_SRC)/301/CO_fifo.c \
	$(CANOPEN_SRC)/303/CO_LEDs.c \
	$(CANOPEN_SRC)/305/CO_LSSslave.c \
	$(CANOPEN_SRC)/305/CO_LSSmaster.c \
	$(CANOPEN_SRC)/309/CO_gateway_ascii.c \
	$(CANOPEN_SRC)/extra/CO_trace.c \
	$(CANOPEN_SRC)/CANopen.c \
	$(OD_SRC)/CO_OD.c \
	$(APPL_SRC)/main_blank.c


OBJS = $(SOURCES:%.c=%.o)
CC ?= gcc
OPT = -g
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
