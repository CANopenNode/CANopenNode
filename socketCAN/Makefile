# Makefile for CANopenNode with socketCAN


DRV_SRC = .
CANOPEN_SRC = ..
OD_SRC = ../example
APPL_SRC = .


LINK_TARGET = canopensocket


INCLUDE_DIRS = \
	-I$(DRV_SRC) \
	-I$(CANOPEN_SRC) \
	-I$(OD_SRC) \
	-I$(APPL_SRC)


SOURCES = \
	$(DRV_SRC)/CO_driver.c \
	$(DRV_SRC)/CO_error.c \
	$(DRV_SRC)/CO_notify_pipe.c \
	$(DRV_SRC)/CO_Linux_threads.c \
	$(DRV_SRC)/CO_OD_storage.c \
	$(CANOPEN_SRC)/301/CO_SDOserver.c \
	$(CANOPEN_SRC)/301/CO_Emergency.c \
	$(CANOPEN_SRC)/301/CO_NMT_Heartbeat.c \
	$(CANOPEN_SRC)/301/CO_HBconsumer.c \
	$(CANOPEN_SRC)/301/CO_SYNC.c \
	$(CANOPEN_SRC)/301/CO_PDO.c \
	$(CANOPEN_SRC)/301/CO_TIME.c \
	$(CANOPEN_SRC)/301/CO_SDOclient.c \
	$(CANOPEN_SRC)/301/crc16-ccitt.c \
	$(CANOPEN_SRC)/305/CO_LSSslave.c \
	$(CANOPEN_SRC)/305/CO_LSSmaster.c \
	$(CANOPEN_SRC)/extra/CO_trace.c \
	$(CANOPEN_SRC)/CANopen.c \
	$(OD_SRC)/CO_OD.c \
	$(APPL_SRC)/main.c


# This can be a single or multi threaded application. If multithreaded is
# used, then two nonblocking threads will be used: fast rt-thread for CAN
# reception + PDO + SYNC processing and slow mainline for other processing.
# For multithreaded operation:
# - Add flag -DCO_MULTI_THREAD to the CFLAGS.
# - Add flag -pthread to LDFLAGS.


OBJS = $(SOURCES:%.c=%.o)
CC ?= gcc
CFLAGS = -Wall -g $(INCLUDE_DIRS) -DCO_MULTI_THREAD
LDFLAGS = -pthread


.PHONY: all clean

all: clean $(LINK_TARGET)

clean:
	rm -f $(OBJS) $(LINK_TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(LINK_TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@
