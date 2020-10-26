/*
 * CANopen main program file for Linux SocketCAN.
 *
 * @file        main.c
 * @author      Janez Paternoster
 * @copyright   2015 - 2020 Janez Paternoster
 *
 * This file is part of CANopenSocket, a Linux implementation of CANopen
 * stack with master functionality. Project home page is
 * <https://github.com/CANopenNode/CANopenSocket>. CANopenSocket is based
 * on CANopenNode: <https://github.com/CANopenNode/CANopenNode>.
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

#ifndef CO_OD_STORAGE
#define CO_OD_STORAGE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include "CO_error.h"
#include "CO_epoll_interface.h"
#if CO_OD_STORAGE == 1
#include "CO_OD_storage.h"
#endif

/* Call external application functions. */
#if __has_include("CO_application.h")
#include "CO_application.h"
#define CO_USE_APPLICATION
#endif

/* Add trace functionality for recording variables over time */
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
#include "CO_time_trace.h"
#endif

/* Use DS309-3 standard - ASCII command interface to CANopen: NMT master,
 * LSS master and SDO client */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
#include "309/CO_gateway_ascii.h"
#endif

/* Interval of mainline and real-time thread in microseconds */
#ifndef MAIN_THREAD_INTERVAL_US
#define MAIN_THREAD_INTERVAL_US 100000
#endif
#ifndef TMR_THREAD_INTERVAL_US
#define TMR_THREAD_INTERVAL_US 1000
#endif


/* Other variables and objects */
#ifndef CO_SINGLE_THREAD
CO_epoll_t                  epRT; /* Epoll-timer object for realtime thread */
static int                  rtPriority = -1;    /* Real time priority, configurable by arguments. (-1=RT disabled) */
#endif
static uint8_t              CO_pendingNodeId = 0xFF;/* Use value from Object Dictionary or by arguments (set to 1..127
                                                     * or unconfigured=0xFF). Can be changed by LSS slave. */
static uint8_t              CO_activeNodeId = 0xFF;/* Copied from CO_pendingNodeId in the communication reset section */
static uint16_t             CO_pendingBitRate = 0;  /* CAN bitrate, not used here */
#if CO_OD_STORAGE == 1
static CO_OD_storage_t      odStor;             /* Object Dictionary storage object for CO_OD_ROM */
static CO_OD_storage_t      odStorAuto;         /* Object Dictionary storage object for CO_OD_EEPROM */
static char                *odStorFile_rom    = "od_storage";       /* Name of the file */
static char                *odStorFile_eeprom = "od_storage_auto";  /* Name of the file */
#endif
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
static CO_time_t            CO_time;            /* Object for current time */
#endif

/* Helper functions ***********************************************************/
#ifndef CO_SINGLE_THREAD
/* Realtime thread */
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

/* callback for NMT change messages */
static void NmtChangedCallback(CO_NMT_internalState_t state)
{
    log_printf(LOG_NOTICE, DBG_NMT_CHANGE, NmtState2Str(state), state);
}

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

#if CO_OD_STORAGE == 1
/* callback for storing node id and bitrate */
static bool_t LSScfgStoreCallback(void *object, uint8_t id, uint16_t bitRate) {
    (void)object;
    OD_CANNodeID = id;
    OD_CANBitRate = bitRate;
    return true;
}
#endif

/* Print usage */
static void printUsage(char *progName) {
printf(
"Usage: %s <CAN device name> [options]\n", progName);
printf(
"\n"
"Options:\n"
"  -i <Node ID>        CANopen Node-id (1..127) or 0xFF(unconfigured). If not\n"
"                      specified, value from Object dictionary (0x2101) is used.\n");
#ifndef CO_SINGLE_THREAD
printf(
"  -p <RT priority>    Real-time priority of RT thread (1 .. 99). If not set or\n"
"                      set to -1, then normal scheduler is used for RT thread.\n");
#endif
printf(
"  -r                  Enable reboot on CANopen NMT reset_node command. \n");
#if CO_OD_STORAGE == 1
printf(
"  -s <ODstorage file> Set Filename for OD storage ('od_storage' is default).\n"
"  -a <ODstorageAuto>  Set Filename for automatic storage variables from\n"
"                      Object dictionary. ('od_storage_auto' is default).\n");
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
    CO_epoll_t epMain;
#ifndef CO_SINGLE_THREAD
    pthread_t rt_thread_id;
#endif
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_ReturnError_t err;
#if CO_OD_STORAGE == 1
    CO_ReturnError_t odStorStatus_rom, odStorStatus_eeprom;
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
        const char comm_stdio[] = "stdio";
        const char comm_local[] = "local-";
        const char comm_tcp[] = "tcp-";
        switch (opt) {
            case 'i':
                CO_pendingNodeId = (uint8_t)strtol(optarg, NULL, 0);
                nodeIdFromArgs = true;
                break;
#ifndef CO_SINGLE_THREAD
            case 'p': rtPriority = strtol(optarg, NULL, 0);
                break;
#endif
            case 'r': rebootEnable = true;
                break;
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
            case 'c':
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
            case 'T':
                socketTimeout_ms = strtoul(optarg, NULL, 0);
                break;
#endif
#if CO_OD_STORAGE == 1
            case 's': odStorFile_rom = optarg;
                break;
            case 'a': odStorFile_eeprom = optarg;
                break;
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

    if(!nodeIdFromArgs) {
        /* use value from Object dictionary, if not set by program arguments */
        CO_pendingNodeId = OD_CANNodeID;
    }

    if((CO_pendingNodeId < 1 || CO_pendingNodeId > 127)
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
      && CO_NO_LSS_SLAVE == 1 && CO_pendingNodeId != CO_LSS_NODE_ID_ASSIGNMENT
#endif
    ) {
        log_printf(LOG_CRIT, DBG_WRONG_NODE_ID, CO_pendingNodeId);
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


    log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_pendingNodeId, "starting");


    /* Allocate memory for CANopen objects */
    err = CO_new(NULL);
    if (err != CO_ERROR_NO) {
        log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_new()", err);
        exit(EXIT_FAILURE);
    }


#if CO_OD_STORAGE == 1
    /* Verify, if OD structures have proper alignment of initial values */
    if(CO_OD_RAM.FirstWord != CO_OD_RAM.LastWord) {
        log_printf(LOG_CRIT, DBG_OBJECT_DICTIONARY, "CO_OD_RAM");
        exit(EXIT_FAILURE);
    }
    if(CO_OD_EEPROM.FirstWord != CO_OD_EEPROM.LastWord) {
        log_printf(LOG_CRIT, DBG_OBJECT_DICTIONARY, "CO_OD_EEPROM");
        exit(EXIT_FAILURE);
    }
    if(CO_OD_ROM.FirstWord != CO_OD_ROM.LastWord) {
        log_printf(LOG_CRIT, DBG_OBJECT_DICTIONARY, "CO_OD_ROM");
        exit(EXIT_FAILURE);
    }


    /* initialize Object Dictionary storage */
    odStorStatus_rom = CO_OD_storage_init(&odStor, (uint8_t*) &CO_OD_ROM, sizeof(CO_OD_ROM), odStorFile_rom);
    odStorStatus_eeprom = CO_OD_storage_init(&odStorAuto, (uint8_t*) &CO_OD_EEPROM, sizeof(CO_OD_EEPROM), odStorFile_eeprom);
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
/* CANopen communication reset - initialize CANopen objects *******************/

        /* Wait rt_thread. */
        if(!firstRun) {
            CO_LOCK_OD();
            CO->CANmodule[0]->CANnormal = false;
            CO_UNLOCK_OD();
        }

        /* Enter CAN configuration. */
        CO_CANsetConfigurationMode((void *)&CANptr);
        CO_CANmodule_disable(CO->CANmodule[0]);


        /* initialize CANopen */
        err = CO_CANinit((void *)&CANptr, 0 /* bit rate not used */);
        if(err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_CANinit()", err);
            exit(EXIT_FAILURE);
        }

        err = CO_LSSinit(&CO_pendingNodeId, &CO_pendingBitRate);
        if(err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_LSSinit()", err);
            exit(EXIT_FAILURE);
        }

        CO_activeNodeId = CO_pendingNodeId;

        err = CO_CANopenInit(CO_activeNodeId);
        if(err != CO_ERROR_NO && err != CO_ERROR_NODE_ID_UNCONFIGURED_LSS) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_CANopenInit()", err);
            exit(EXIT_FAILURE);
        }

        /* initialize part of threadMain and callbacks */
        CO_epoll_initCANopenMain(&epMain, CO);
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
        CO_epoll_initCANopenGtw(&epGtw, CO);
#endif
#if CO_OD_STORAGE == 1
        CO_LSSslave_initCfgStoreCallback(CO->LSSslave, NULL,
                                         LSScfgStoreCallback);
#endif
        if(!CO->nodeIdUnconfigured) {
            CO_EM_initCallbackRx(CO->em, EmergencyRxCallback);
            CO_NMT_initCallbackChanged(CO->NMT, NmtChangedCallback);
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_CALLBACK_CHANGE
            CO_HBconsumer_initCallbackNmtChanged(CO->HBcons, NULL,
                                                 HeartbeatNmtChangedCallback);
#endif
#if CO_OD_STORAGE == 1
            /* initialize OD objects 1010 and 1011 and verify errors. */
            CO_OD_configure(CO->SDO[0], OD_H1010_STORE_PARAM_FUNC, CO_ODF_1010, (void*)&odStor, 0, 0U);
            CO_OD_configure(CO->SDO[0], OD_H1011_REST_PARAM_FUNC, CO_ODF_1011, (void*)&odStor, 0, 0U);
            if(odStorStatus_rom != CO_ERROR_NO) {
                CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)odStorStatus_rom);
            }
            if(odStorStatus_eeprom != CO_ERROR_NO) {
                CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)odStorStatus_eeprom + 1000);
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
                exit(EXIT_FAILURE);
            }
            if(rtPriority > 0) {
                struct sched_param param;

                param.sched_priority = rtPriority;
                if (pthread_setschedparam(rt_thread_id, SCHED_FIFO, &param) != 0) {
                    log_printf(LOG_CRIT, DBG_ERRNO, "pthread_setschedparam()");
                    exit(EXIT_FAILURE);
                }
            }
#endif
#ifdef CO_USE_APPLICATION
            /* Execute optional additional application code */
            app_programStart(!CO->nodeIdUnconfigured);
#endif
        } /* if(firstRun) */


#ifdef CO_USE_APPLICATION
        /* Execute optional additional application code */
        app_communicationReset(!CO->nodeIdUnconfigured);
#endif


        /* start CAN */
        CO_CANsetNormalMode(CO->CANmodule[0]);

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
            CO_epoll_processMain(&epMain, CO, &reset);
            CO_epoll_processLast(&epMain);

#ifdef CO_USE_APPLICATION
            app_programAsync(!CO->nodeIdUnconfigured, epMain.timeDifference_us);
#endif

#if CO_OD_STORAGE == 1
            CO_OD_storage_autoSave(&odStorAuto,
                                   epMain.timeDifference_us, 60000000);
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

#if CO_OD_STORAGE == 1
    /* Store CO_OD_EEPROM */
    CO_OD_storage_autoSave(&odStorAuto, 0, 0);
    CO_OD_storage_autoSaveClose(&odStorAuto);
#endif

    /* delete objects from memory */
#ifndef CO_SINGLE_THREAD
    CO_epoll_close(&epRT);
#endif
    CO_epoll_close(&epMain);
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    CO_epoll_closeGtw(&epGtw);
#endif
    CO_delete((void *)&CANptr);

    log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_activeNodeId, "finished");

    /* Flush all buffers (and reboot) */
    if(rebootEnable && reset == CO_RESET_APP) {
        sync();
        if(reboot(LINUX_REBOOT_CMD_RESTART) != 0) {
            log_printf(LOG_CRIT, DBG_ERRNO, "reboot()");
            exit(EXIT_FAILURE);
        }
    }

    exit(EXIT_SUCCESS);
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
        for(i=0; i<OD_traceEnable && i<CO_NO_TRACE; i++) {
            CO_trace_process(CO->trace[i], *CO_time.epochTimeOffsetMs);
        }
#endif

#ifdef CO_USE_APPLICATION
        /* Execute optional additional application code */
        app_program1ms(!CO->nodeIdUnconfigured);
#endif

    }

    return NULL;
}
#endif
