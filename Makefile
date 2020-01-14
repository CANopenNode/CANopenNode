# Makefile for CANopenNode, basic compile with no CAN device.


DRV_SRC = example
STACK_SRC = stack
CANOPEN_SRC =.
APPL_SRC = example


LINK_TARGET = canopennode


INCLUDE_DIRS = \
	-I$(DRV_SRC) \
	-I$(STACK_SRC) \
	-I$(CANOPEN_SRC) \
	-I$(APPL_SRC)


SOURCES = \
	$(DRV_SRC)/CO_driver.c \
	$(DRV_SRC)/eeprom.c \
	$(STACK_SRC)/crc16-ccitt.c \
	$(STACK_SRC)/CO_SDO.c \
	$(STACK_SRC)/CO_Emergency.c \
	$(STACK_SRC)/CO_NMT_Heartbeat.c \
	$(STACK_SRC)/CO_SYNC.c \
	$(STACK_SRC)/CO_TIME.c \
	$(STACK_SRC)/CO_PDO.c \
	$(STACK_SRC)/CO_HBconsumer.c \
	$(STACK_SRC)/CO_SDOmaster.c \
	$(STACK_SRC)/CO_LSSmaster.c \
	$(STACK_SRC)/CO_LSSslave.c \
	$(STACK_SRC)/CO_trace.c \
	$(CANOPEN_SRC)/CANopen.c \
	$(APPL_SRC)/CO_OD.c \
	$(APPL_SRC)/main.c


OBJS = $(SOURCES:%.c=%.o)
CC = gcc
CFLAGS = -Wall $(INCLUDE_DIRS)
LDFLAGS =


.PHONY: all clean doc

all: clean $(LINK_TARGET)

clean:
	rm -f $(OBJS) $(LINK_TARGET)

doc:
	doxygen

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LINK_TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
