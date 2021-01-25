/*
 * CANopen main program file for CANopenNode on Linux.
 *
 * @file        CO_main_basic.c
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <bits/getopt_core.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <sys/epoll.h>
#include <net/if.h>
#include <linux/reboot.h>
#include <sys/reboot.h>

#include "CANopen.h"
#include "OD.h"
#include "CO_error.h"
#include "CO_epoll_interface.h"
#include "CO_storageLinux.h"

/* Call external application functions. */
#ifdef CO_USE_APPLICATION
#include "CO_application.h"
#endif

/* Add trace functionality for recording variables over time */
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
#include "CO_time_trace.h"
#endif


/* Interval of mainline and real-time thread in microseconds */
#ifndef MAIN_THREAD_INTERVAL_US
#define MAIN_THREAD_INTERVAL_US 100000
#endif
#ifndef TMR_THREAD_INTERVAL_US
#define TMR_THREAD_INTERVAL_US 1000
#endif

/* default values for CO_CANopenInit() */
#ifndef NMT_CONTROL
#define NMT_CONTROL \
            CO_NMT_STARTUP_TO_OPERATIONAL \
         || CO_NMT_ERR_ON_ERR_REG \
         || CO_ERR_REG_GENERIC_ERR \
         || CO_ERR_REG_COMMUNICATION
#endif
#ifndef FIRST_HB_TIME
#define FIRST_HB_TIME 500
#endif
#ifndef SDO_SRV_TIMEOUT_TIME
#define SDO_SRV_TIMEOUT_TIME 1000
#endif
#ifndef SDO_CLI_TIMEOUT_TIME
#define SDO_CLI_TIMEOUT_TIME 500
#endif
#ifndef SDO_CLI_BLOCK
#define SDO_CLI_BLOCK false
#endif
#ifndef OD_STATUS_BITS
#define OD_STATUS_BITS NULL
#endif
/* CANopen gateway enable switch for CO_epoll_processMain() */
#ifndef GATEWAY_ENABLE
#define GATEWAY_ENABLE true
#endif

/* CANopen object */
CO_t *CO = NULL;

/* Configurable CAN bit-rate and CANopen node-id, store-able to non-volatile
 * memory. Can be set by argument and changed by LSS slave. */
typedef struct { uint16_t bitRate; uint8_t nodeId; } CO_pending_t;
CO_pending_t CO_pending = { .bitRate = 0, .nodeId = CO_LSS_NODE_ID_ASSIGNMENT };
/* Active node-id, copied from CO_pending.nodeId in the communication reset */
static uint8_t CO_activeNodeId = CO_LSS_NODE_ID_ASSIGNMENT;

/* Data storage for Object Dictionnary and LSS */
#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
/* Maximum file name length including path for storage */
#ifndef CO_STORAGE_PATH_MAX
#define CO_STORAGE_PATH_MAX 255
#endif

/* Definitions for application specific storage objects */
#ifndef CO_STORAGE_APPLICATION
#define CO_STORAGE_APPLICATION
#endif

/* Interval for automatic storage in microseconds */
#ifndef CO_STORAGE_AUTO_INTERVAL
#define CO_STORAGE_AUTO_INTERVAL 60000000
#endif

/* Configure objects for storage */
typedef struct {
    /* Address, length and filename of data to store */
    void *addr;
    OD_size_t len;
    char filename[CO_STORAGE_PATH_MAX];
    /* Subindex in OD objects 1010 and 1011 or 0 for auto storage.
     * 1 is reserved for storing all parameters */
    uint8_t subIndexOD;
    /* Object for storage or auto storage, usage depends on subIndexOD */
    CO_storage_entry_t entry;
    CO_storageLinux_auto_t entryAuto;
} CO_storageLinux_entry_t;
CO_storageLinux_entry_t storage[] = {
    {
        .addr = &CO_pending,
        .len = sizeof(CO_pending),
        .filename = {'l', 's', 's',
                     '.', 'p', 'e', 'r', 's', 'i', 's', 't', '\0'},
        .subIndexOD = 0
    },
    {
        .addr = &OD_PERSIST_COMM,
        .len = sizeof(OD_PERSIST_COMM),
        .filename = {'o', 'd', '_', 'c', 'o', 'm', 'm',
                     '.', 'p', 'e', 'r', 's', 'i', 's', 't', '\0'},
        .subIndexOD = 2
    },
    CO_STORAGE_APPLICATION
};
#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */

#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
static CO_time_t            CO_time;            /* Object for current time */
#endif


/* Helper functions ***********************************************************/
#ifndef CO_SINGLE_THREAD
/* Realtime thread */
CO_epoll_t epRT;
static void* rt_thread(void* arg);
#endif

/* Signal handler */
volatile sig_atomic_t CO_endProgram = 0;
static void sigHandler(int sig) {
    (void)sig;
    CO_endProgram = 1;
}

/* Message logging function */
void log_printf(int priority, const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG
    if (CO != NULL) {
        char buf[200];
        time_t timer;
        struct tm* tm_info;
        size_t len;

        timer = time(NULL);
        tm_info = localtime(&timer);
        len = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S: ", tm_info);

        va_start(ap, format);
        vsnprintf(buf + len, sizeof(buf) - len - 2, format, ap);
        va_end(ap);
        strcat(buf, "\r\n");
        CO_GTWA_log_print(CO->gtwa, buf);
    }
#endif
}

#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
/* callback for emergency messages */
static void EmergencyRxCallback(const uint16_t ident,
                                const uint16_t errorCode,
                                const uint8_t errorRegister,
                                const uint8_t errorBit,
                                const uint32_t infoCode)
{
    int16_t nodeIdRx = ident ? (ident&0x7F) : CO_activeNodeId;

    log_printf(LOG_NOTICE, DBG_EMERGENCY_RX, nodeIdRx, errorCode,
               errorRegister, errorBit, infoCode);
}
#endif

#if ((CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE) \
 || ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE)
/* return string description of NMT state. */
static char *NmtState2Str(CO_NMT_internalState_t state)
{
    switch(state) {
        case CO_NMT_INITIALIZING:    return "initializing";
        case CO_NMT_PRE_OPERATIONAL: return "pre-operational";
        case CO_NMT_OPERATIONAL:     return "operational";
        case CO_NMT_STOPPED:         return "stopped";
        default:                     return "unknown";
    }
}
#endif

#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE
/* callback for NMT change messages */
static void NmtChangedCallback(CO_NMT_internalState_t state)
{
    log_printf(LOG_NOTICE, DBG_NMT_CHANGE, NmtState2Str(state), state);
}
#endif

#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE
/* callback for monitoring Heartbeat remote NMT state change */
static void HeartbeatNmtChangedCallback(uint8_t nodeId, uint8_t idx,
                                        CO_NMT_internalState_t state,
                                        void *object)
{
    (void)object;
    log_printf(LOG_NOTICE, DBG_HB_CONS_NMT_CHANGE,
               nodeId, idx, NmtState2Str(state), state);
}
#endif

/* callback for storing node id and bitrate */
static bool_t LSScfgStoreCallback(void *object, uint8_t id, uint16_t bitRate) {
    (void)object;
    CO_pending.nodeId = id;
    CO_pending.bitRate = bitRate;
    return true;
}

/* Print usage */
static void printUsage(char *progName) {
printf(
"Usage: %s <CAN device name> [options]\n", progName);
printf(
"\n"
"Options:\n"
"  -i <Node ID>        CANopen Node-id (1..127) or 0xFF (LSS unconfigured).\n");
#ifndef CO_SINGLE_THREAD
printf(
"  -p <RT priority>    Real-time priority of RT thread (1 .. 99). If not set or\n"
"                      set to -1, then normal scheduler is used for RT thread.\n");
#endif
printf(
"  -r                  Enable reboot on CANopen NMT reset_node command. \n");
#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
printf(
"  -s <storage path>   Path and filename prefix for data storage files.\n"
"                      By default files are stored in current dictionary.\n");
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
printf(
"  -c <interface>      Enable command interface for master functionality.\n"
"                      One of three types of interfaces can be specified as:\n"
"                   1. \"stdio\" - Standard IO of a program (terminal).\n"
"                   2. \"local-<file path>\" - Local socket interface on file\n"
"                      path, for example \"local-/tmp/CO_command_socket\".\n"
"                   3. \"tcp-<port>\" - Tcp socket interface on specified \n"
"                      port, for example \"tcp-60000\".\n"
"                      Note that this option may affect security of the CAN.\n"
"  -T <timeout_time>   If -c is specified as local or tcp socket, then this\n"
"                      parameter specifies socket timeout time in milliseconds.\n"
"                      Default is 0 - no timeout on established connection.\n");
#endif
printf(
"\n"
"See also: https://github.com/CANopenNode/CANopenNode\n"
"\n");
}


/*******************************************************************************
 * Mainline thread
 ******************************************************************************/
int main (int argc, char *argv[]) {
    int programExit = EXIT_SUCCESS;
    CO_epoll_t epMain;
#ifndef CO_SINGLE_THREAD
    pthread_t rt_thread_id;
    int rtPriority = -1;
#endif
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_ReturnError_t err;
#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
    uint32_t storageInitError = 0;
    uint32_t storageIntervalTimer = 0;
#endif
    CO_CANptrSocketCan_t CANptr = {0};
    int opt;
    bool_t firstRun = true;

    char* CANdevice = NULL;         /* CAN device, configurable by arguments. */
    bool_t nodeIdFromArgs = false;  /* True, if program arguments are used for CANopen Node Id */
    bool_t rebootEnable = false;    /* Configurable by arguments */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    CO_epoll_gtw_t epGtw;
    /* values from CO_commandInterface_t */
    int32_t commandInterface = CO_COMMAND_IF_DISABLED;
    /* local socket path if commandInterface == CO_COMMAND_IF_LOCAL_SOCKET */
    char *localSocketPath = NULL;
    uint32_t socketTimeout_ms = 0;
#else
    #define commandInterface 0
    #define localSocketPath NULL
#endif

    /* configure system log */
    setlogmask(LOG_UPTO (LOG_DEBUG)); /* LOG_DEBUG - log all messages */
    openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER); /* print also to standard error */

    /* Get program options */
    if(argc < 2 || strcmp(argv[1], "--help") == 0){
        printUsage(argv[0]);
        exit(EXIT_SUCCESS);
    }
    while((opt = getopt(argc, argv, "i:p:rc:T:s:a:")) != -1) {
        switch (opt) {
            case 'i':
                nodeIdFromArgs = true;
                CO_pending.nodeId = (uint8_t)strtol(optarg, NULL, 0);
                break;
#ifndef CO_SINGLE_THREAD
            case 'p': rtPriority = strtol(optarg, NULL, 0);
                break;
#endif
            case 'r': rebootEnable = true;
                break;
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
            case 'c': {
                const char *comm_stdio = "stdio";
                const char *comm_local = "local-";
                const char *comm_tcp = "tcp-";
                if (strcmp(optarg, comm_stdio) == 0) {
                    commandInterface = CO_COMMAND_IF_STDIO;
                }
                else if (strncmp(optarg, comm_local, strlen(comm_local)) == 0) {
                    commandInterface = CO_COMMAND_IF_LOCAL_SOCKET;
                    localSocketPath = &optarg[6];
                }
                else if (strncmp(optarg, comm_tcp, strlen(comm_tcp)) == 0) {
                    const char *portStr = &optarg[4];
                    uint16_t port;
                    int nMatch = sscanf(portStr, "%hu", &port);
                    if(nMatch != 1) {
                        log_printf(LOG_CRIT, DBG_NOT_TCP_PORT, portStr);
                        exit(EXIT_FAILURE);
                    }
                    commandInterface = port;
                }
                else {
                    log_printf(LOG_CRIT, DBG_ARGUMENT_UNKNOWN, "-c", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            }
            case 'T':
                socketTimeout_ms = strtoul(optarg, NULL, 0);
                break;
#endif
#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
            case 's': {
                /* add prefix to each storage[i].filename */
                for (int i = 0; i < sizeof(storage) / sizeof(storage[0]); i++) {
                    char* filePrefix = optarg;
                    size_t filePrefixLen = strlen(filePrefix);
                    char* file = storage[i].filename;
                    size_t fileLen = strlen(file);
                    if (fileLen + filePrefixLen < CO_STORAGE_PATH_MAX) {
                        memmove(&file[filePrefixLen], &file[0], fileLen + 1);
                        memcpy(&file[0], &filePrefix[0], filePrefixLen);
                    }
                }
                break;
            }
#endif
            default:
                printUsage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(optind < argc) {
        CANdevice = argv[optind];
        CANptr.can_ifindex = if_nametoindex(CANdevice);
    }

    if((CO_pending.nodeId < 1 || CO_pending.nodeId > 127)
       && CO_isLSSslaveEnabled(CO)
       && CO_pending.nodeId != CO_LSS_NODE_ID_ASSIGNMENT
    ) {
        log_printf(LOG_CRIT, DBG_WRONG_NODE_ID, CO_pending.nodeId);
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

#ifndef CO_SINGLE_THREAD
    if(rtPriority != -1 && (rtPriority < sched_get_priority_min(SCHED_FIFO)
                         || rtPriority > sched_get_priority_max(SCHED_FIFO))) {
        log_printf(LOG_CRIT, DBG_WRONG_PRIORITY, rtPriority);
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }
#endif

    if(CANptr.can_ifindex == 0) {
        log_printf(LOG_CRIT, DBG_NO_CAN_DEVICE, CANdevice);
        exit(EXIT_FAILURE);
    }


    log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_pending.nodeId, "starting");


    /* Allocate memory for CANopen objects */
    uint32_t heapMemoryUsed = 0;
    CO = CO_new(NULL, &heapMemoryUsed);
    if (CO == NULL) {
        log_printf(LOG_CRIT, DBG_GENERAL,
                   "CO_new(), heapMemoryUsed=", heapMemoryUsed);
        exit(EXIT_FAILURE);
    }


#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
    uint8_t pendingNodeIdOriginal = CO_pending.nodeId;
    for (int i = 0; i < sizeof(storage) / sizeof(storage[0]); i++) {
        CO_storageLinux_entry_t *st = &storage[i];
        if (st->subIndexOD == 0) {
            err = CO_storageLinux_auto_init(&st->entryAuto,
                                            st->addr,
                                            st->len,
                                            st->filename);
        } else {
            err = CO_storageLinux_entry_init(CO->storage,
                                             &st->entry,
                                             st->addr,
                                             st->len,
                                             st->filename,
                                             st->subIndexOD);
        }
        if (err == CO_ERROR_DATA_CORRUPT) {
            uint32_t bit = 1;
            if (i < 32) bit <<= i;
            storageInitError |= bit;
        }
        else if (err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_OBJECT_DICTIONARY, st->filename);
            exit(EXIT_FAILURE);
        }
    }
    /* Overwrite stored node-id, if specified by program arguments */
    if (nodeIdFromArgs) {
        CO_pending.nodeId = pendingNodeIdOriginal;
    }
#endif

    /* Catch signals SIGINT and SIGTERM */
    if(signal(SIGINT, sigHandler) == SIG_ERR) {
        log_printf(LOG_CRIT, DBG_ERRNO, "signal(SIGINT, sigHandler)");
        exit(EXIT_FAILURE);
    }
    if(signal(SIGTERM, sigHandler) == SIG_ERR) {
        log_printf(LOG_CRIT, DBG_ERRNO, "signal(SIGTERM, sigHandler)");
        exit(EXIT_FAILURE);
    }


    /* Create epoll functions */
    err = CO_epoll_create(&epMain, MAIN_THREAD_INTERVAL_US);
    if(err != CO_ERROR_NO) {
        log_printf(LOG_CRIT, DBG_GENERAL,
                   "CO_epoll_create(main), err=", err);
        exit(EXIT_FAILURE);
    }
#ifndef CO_SINGLE_THREAD
    err = CO_epoll_create(&epRT, TMR_THREAD_INTERVAL_US);
    if(err != CO_ERROR_NO) {
        log_printf(LOG_CRIT, DBG_GENERAL,
                   "CO_epoll_create(RT), err=", err);
        exit(EXIT_FAILURE);
    }
    CANptr.epoll_fd = epRT.epoll_fd;
#else
    CANptr.epoll_fd = epMain.epoll_fd;
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    err = CO_epoll_createGtw(&epGtw, epMain.epoll_fd, commandInterface,
                              socketTimeout_ms, localSocketPath);
    if(err != CO_ERROR_NO) {
        log_printf(LOG_CRIT, DBG_GENERAL, "CO_epoll_createGtw(), err=", err);
        exit(EXIT_FAILURE);
    }
#endif


    while(reset != CO_RESET_APP && reset != CO_RESET_QUIT && CO_endProgram == 0) {
        uint32_t errInfo;
/* CANopen communication reset - initialize CANopen objects *******************/

        /* Wait rt_thread. */
        if(!firstRun) {
            CO_LOCK_OD();
            CO->CANmodule->CANnormal = false;
            CO_UNLOCK_OD();
        }

        /* Enter CAN configuration. */
        CO_CANsetConfigurationMode((void *)&CANptr);
        CO_CANmodule_disable(CO->CANmodule);


        /* initialize CANopen */
        err = CO_CANinit(CO, (void *)&CANptr, 0 /* bit rate not used */);
        if(err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_CANinit()", err);
            programExit = EXIT_FAILURE;
            CO_endProgram = 1;
            continue;
        }

        CO_LSS_address_t lssAddress = {.identity = {
            .vendorID = OD_PERSIST_COMM.x1018_identity.vendor_ID,
            .productCode = OD_PERSIST_COMM.x1018_identity.productCode,
            .revisionNumber = OD_PERSIST_COMM.x1018_identity.revisionNumber,
            .serialNumber = OD_PERSIST_COMM.x1018_identity.serialNumber
        }};
        err = CO_LSSinit(CO, &lssAddress,
                         &CO_pending.nodeId, &CO_pending.bitRate);
        if(err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_LSSinit()", err);
            programExit = EXIT_FAILURE;
            CO_endProgram = 1;
            continue;
        }

        CO_activeNodeId = CO_pending.nodeId;
        errInfo = 0;

        err = CO_CANopenInit(CO,                /* CANopen object */
                             NULL,              /* alternate NMT */
                             NULL,              /* alternate em */
                             OD,                /* Object dictionary */
                             OD_STATUS_BITS,    /* Optional OD_statusBits */
                             NMT_CONTROL,       /* CO_NMT_control_t */
                             FIRST_HB_TIME,     /* firstHBTime_ms */
                             SDO_SRV_TIMEOUT_TIME, /* SDOserverTimeoutTime_ms */
                             SDO_CLI_TIMEOUT_TIME, /* SDOclientTimeoutTime_ms */
                             SDO_CLI_BLOCK,     /* SDOclientBlockTransfer */
                             CO_activeNodeId,
                             &errInfo);
        if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
            if (err == CO_ERROR_OD_PARAMETERS) {
                log_printf(LOG_CRIT, DBG_OD_ENTRY, errInfo);
            }
            else {
                log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_CANopenInit()", err);
            }
            programExit = EXIT_FAILURE;
            CO_endProgram = 1;
            continue;
        }

        /* initialize part of threadMain and callbacks */
        CO_epoll_initCANopenMain(&epMain, CO);
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
        CO_epoll_initCANopenGtw(&epGtw, CO);
#endif
        CO_LSSslave_initCfgStoreCallback(CO->LSSslave, NULL,
                                         LSScfgStoreCallback);
        if(!CO->nodeIdUnconfigured) {
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
            CO_EM_initCallbackRx(CO->em, EmergencyRxCallback);
#endif
#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_CALLBACK_CHANGE
            CO_NMT_initCallbackChanged(CO->NMT, NmtChangedCallback);
#endif
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE
            CO_HBconsumer_initCallbackNmtChanged(CO->HBcons, NULL,
                                                 HeartbeatNmtChangedCallback);
#endif
#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
            if(storageInitError != 0) {
                CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY,
                               CO_EMC_HARDWARE, storageInitError);
            }
#endif

#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
            /* Initialize time */
            CO_time_init(&CO_time, CO->SDO[0], &OD_time.epochTimeBaseMs, &OD_time.epochTimeOffsetMs, 0x2130);
#endif
            log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_activeNodeId, "communication reset");
        }
        else {
            log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_activeNodeId, "node-id not initialized");
        }

        /* First time only initialization. */
        if(firstRun) {
            firstRun = false;
#ifndef CO_SINGLE_THREAD
            /* Create rt_thread and set priority */
            if(pthread_create(&rt_thread_id, NULL, rt_thread, NULL) != 0) {
                log_printf(LOG_CRIT, DBG_ERRNO, "pthread_create(rt_thread)");
                programExit = EXIT_FAILURE;
                CO_endProgram = 1;
                continue;
            }
            if(rtPriority > 0) {
                struct sched_param param;

                param.sched_priority = rtPriority;
                if (pthread_setschedparam(rt_thread_id, SCHED_FIFO, &param) != 0) {
                    log_printf(LOG_CRIT, DBG_ERRNO, "pthread_setschedparam()");
                    programExit = EXIT_FAILURE;
                    CO_endProgram = 1;
                    continue;
                }
            }
#endif
#ifdef CO_USE_APPLICATION
            /* Execute optional additional application code */
            errInfo = 0;
            err = app_programStart(!CO->nodeIdUnconfigured, &errInfo);
            if(err != CO_ERROR_NO) {
                if (err == CO_ERROR_OD_PARAMETERS) {
                    log_printf(LOG_CRIT, DBG_OD_ENTRY, errInfo);
                }
                else {
                    log_printf(LOG_CRIT, DBG_CAN_OPEN, "app_programStart()", err);
                }
                programExit = EXIT_FAILURE;
                CO_endProgram = 1;
                continue;
            }
#endif
        } /* if(firstRun) */


#ifdef CO_USE_APPLICATION
        /* Execute optional additional application code */
        app_communicationReset(!CO->nodeIdUnconfigured);
#endif


        /* start CAN */
        CO_CANsetNormalMode(CO->CANmodule);

        reset = CO_RESET_NOT;

        log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_activeNodeId, "running ...");


        while(reset == CO_RESET_NOT && CO_endProgram == 0) {
/* loop for normal program execution ******************************************/
            CO_epoll_wait(&epMain);
#ifdef CO_SINGLE_THREAD
            CO_epoll_processRT(&epMain, CO, false);
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
            CO_epoll_processGtw(&epGtw, CO, &epMain);
#endif
            CO_epoll_processMain(&epMain, CO, GATEWAY_ENABLE, &reset);
            CO_epoll_processLast(&epMain);

#ifdef CO_USE_APPLICATION
            app_programAsync(!CO->nodeIdUnconfigured, epMain.timeDifference_us);
#endif

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
            /* don't save more often than interval */
            if (storageIntervalTimer < CO_STORAGE_AUTO_INTERVAL) {
                storageIntervalTimer += epMain.timeDifference_us;
            }
            else {
                storageIntervalTimer = 0;
                for (int i = 0; i < sizeof(storage) / sizeof(storage[0]); i++) {
                    CO_storageLinux_entry_t *st = &storage[i];
                    if (st->subIndexOD == 0) {
                        bool_t storageOK = CO_storageLinux_auto_process(
                                                        &st->entryAuto, false);
                        if(!storageOK) {
                            CO_errorReport(CO->em, CO_EM_NON_VOLATILE_AUTO_SAVE,
                                           CO_EMC_HARDWARE, i);
                        }
                    }
                }
            }
#endif
        }
    } /* while(reset != CO_RESET_APP */


/* program exit ***************************************************************/
    /* join threads */
    CO_endProgram = 1;
#ifndef CO_SINGLE_THREAD
    if (pthread_join(rt_thread_id, NULL) != 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "pthread_join()");
        exit(EXIT_FAILURE);
    }
#endif
#ifdef CO_USE_APPLICATION
    /* Execute optional additional application code */
    app_programEnd();
#endif

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE
    for (int i = 0; i < sizeof(storage) / sizeof(storage[0]); i++) {
        CO_storageLinux_entry_t *st = &storage[i];
        if (st->subIndexOD == 0) {
            CO_storageLinux_auto_process(&st->entryAuto, true);
        }
    }
#endif

    /* delete objects from memory */
#ifndef CO_SINGLE_THREAD
    CO_epoll_close(&epRT);
#endif
    CO_epoll_close(&epMain);
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    CO_epoll_closeGtw(&epGtw);
#endif
    CO_CANsetConfigurationMode((void *)&CANptr);
    CO_delete(CO);

    log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_activeNodeId, "finished");

    /* Flush all buffers (and reboot) */
    if(rebootEnable && reset == CO_RESET_APP) {
        sync();
        if(reboot(LINUX_REBOOT_CMD_RESTART) != 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "reboot()");
            exit(EXIT_FAILURE);
        }
    }

    exit(programExit);
}

#ifndef CO_SINGLE_THREAD
/*******************************************************************************
 * Realtime thread for CAN receive and threadTmr
 ******************************************************************************/
static void* rt_thread(void* arg) {
    (void)arg;
    /* Endless loop */
    while(CO_endProgram == 0) {

        CO_epoll_wait(&epRT);
        CO_epoll_processRT(&epRT, CO, true);
        CO_epoll_processLast(&epRT);

#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
        /* Monitor variables with trace objects */
        CO_time_process(&CO_time);
        for(i=0; i<OD_traceEnable && i<co->CNT_TRACE; i++) {
            CO_trace_process(CO->trace[i], *CO_time.epochTimeOffsetMs);
        }
#endif

#ifdef CO_USE_APPLICATION
        /* Execute optional additional application code */
        app_program1ms(!CO->nodeIdUnconfigured, epRT.timeDifference_us);
#endif

    }

    return NULL;
}
#endif
