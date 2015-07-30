# Makefile for CANopenNode, basic compile with no CAN device.


STACK_SRC =  stack
STACKDRV_SRC =  stack/drvTemplate


INCLUDE_DIRS = $(STACK_SRC) \
	-I$(STACKDRV_SRC) \
	-I.


SOURCES = $(STACKDRV_SRC)/CO_driver.c \
	$(STACKDRV_SRC)/eeprom.c \
	$(STACK_SRC)/crc16-ccitt.c \
	$(STACK_SRC)/CO_SDO.c \
	$(STACK_SRC)/CO_Emergency.c \
	$(STACK_SRC)/CO_NMT_Heartbeat.c \
	$(STACK_SRC)/CO_SYNC.c \
	$(STACK_SRC)/CO_PDO.c \
	$(STACK_SRC)/CO_HBconsumer.c \
	$(STACK_SRC)/CO_SDOmaster.c \
	CANopen.c \
	CO_OD.c \
	main.c


OBJS = ${SOURCES:%.c=%.o}
CC = gcc
CFLAGS = -Wall -I$(INCLUDE_DIRS)


.PHONY: all clean

all: canopennode

clean:
	rm -f $(OBJS) canopennode

%.o: %.c
	$(CC) $(CFLAGS) -c -o $*.o $<

canopennode: $(OBJS)
	$(CC) $(CFLAGS)  $(OBJS) -o canopennode

