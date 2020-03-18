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


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <sys/epoll.h>
#include <net/if.h>
#include <linux/reboot.h>
#include <sys/reboot.h>

#include "CANopen.h"
#include "CO_OD_storage.h"
#include "CO_error.h"
#include "CO_Linux_threads.h"

/* Call external application functions. */
#if __has_include("CO_application.h")
#include "CO_application.h"
#define CO_USE_APPLICATION
#endif

/* Add trace functionality for recording variables over time */
#if CO_NO_TRACE > 0
#include "CO_time_trace.h"
#endif

/* Use DS309-3 standard - ASCII command interface to CANopen: NMT master,
 * LSS master and SDO client */
#if CO_CONFIG_309 > 0
#include "CO_command.h"
#endif

/* Interval of mainline and real-time thread in microseconds */
#ifndef MAIN_THREAD_INTERVAL_US
#define MAIN_THREAD_INTERVAL_US 100000
#endif
#ifndef TMR_THREAD_INTERVAL_US
#define TMR_THREAD_INTERVAL_US 1000
#endif


#if CO_CONFIG_309 > 0
/* Mutex is locked, when CAN is not valid (configuration state). May be used
 * from command interface. RT threads may use CO->CANmodule[0]->CANnormal instead. */
pthread_mutex_t             CO_CAN_VALID_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Other variables and objects */
static int                  rtPriority = -1;    /* Real time priority, configurable by arguments. (-1=RT disabled) */
static int                  CO_ownNodeId = -1;  /* Use value from Object Dictionary or set to 1..127 by arguments */
static CO_OD_storage_t      odStor;             /* Object Dictionary storage object for CO_OD_ROM */
static CO_OD_storage_t      odStorAuto;         /* Object Dictionary storage object for CO_OD_EEPROM */
static char                *odStorFile_rom    = "od_storage";       /* Name of the file */
static char                *odStorFile_eeprom = "od_storage_auto";  /* Name of the file */
#if CO_CONFIG_309 > 0
static in_port_t            CO_command_socket_tcp_port = 60000; /* default port when used in tcp gateway mode */
#endif
#if CO_NO_TRACE > 0
static CO_time_t            CO_time;            /* Object for current time */
#endif

/* Helper functions ***********************************************************/
/* Realtime thread */
static void* rt_thread(void* arg);

/* Signal handler */
volatile sig_atomic_t CO_endProgram = 0;
static void sigHandler(int sig) {
    CO_endProgram = 1;
}

/* callback for emergency messages */
static void EmergencyRxCallback(const uint16_t ident,
                                const uint16_t errorCode,
                                const uint8_t errorRegister,
                                const uint8_t errorBit,
                                const uint32_t infoCode)
{
    int16_t nodeIdRx = ident ? (ident&0x7F) : CO_ownNodeId;

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
static void NmtChangeCallback(CO_NMT_internalState_t state)
{
    log_printf(LOG_NOTICE, DBG_NMT_CHANGE, NmtState2Str(state), state);
}

/* callback for monitoring Heartbeat remote NMT state change */
static void HeartbeatNmtChangedCallback(uint8_t nodeId,
                                        CO_NMT_internalState_t state,
                                        void *object)
{
    log_printf(LOG_NOTICE, DBG_HB_CONS_NMT_CHANGE,
               nodeId, NmtState2Str(state), state);
}

/* Print usage */
static void printUsage(char *progName) {
printf(
"Usage: %s <CAN device name> [options]\n", progName);
printf(
"\n"
"Options:\n"
"  -i <Node ID>        CANopen Node-id (1..127). If not specified, value from\n"
"                      Object dictionary (0x2101) is used.\n"
"  -p <RT priority>    Real-time priority of RT thread (1 .. 99). If not set or\n"
"                      set to -1, then normal scheduler is used for RT thread.\n"
"  -r                  Enable reboot on CANopen NMT reset_node command. \n"
"  -s <ODstorage file> Set Filename for OD storage ('od_storage' is default).\n"
"  -a <ODstorageAuto>  Set Filename for automatic storage variables from\n"
"                      Object dictionary. ('od_storage_auto' is default).\n");
#if CO_CONFIG_309 > 0
printf(
"  -c <Socket path>    Enable command interface for master functionality. \n"
"                      If socket path is specified as empty string \"\",\n"
"                      default '%s' will be used.\n"
"                      Note that location of socket path may affect security.\n"
"                      See 'canopencomm/canopencomm --help' for more info.\n"
, CO_command_socketPath);
printf(
"  -t <port>           Enable command interface for master functionality over\n"
"                      tcp, listen to <port>.\n"
"                      Note that using this mode may affect security.\n"
);
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
    pthread_t rt_thread_id;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_ReturnError_t err;
    CO_ReturnError_t odStorStatus_rom, odStorStatus_eeprom;
    intptr_t CANdevice0Index = 0;
    int opt;
    bool_t firstRun = true;

    char* CANdevice = NULL;         /* CAN device, configurable by arguments. */
    bool_t nodeIdFromArgs = false;  /* True, if program arguments are used for CANopen Node Id */
    bool_t rebootEnable = false;    /* Configurable by arguments */
#if CO_CONFIG_309 > 0
    typedef enum CMD_MODE {CMD_NONE, CMD_LOCAL, CMD_REMOTE} cmdMode_t;
    cmdMode_t commandEnable = CMD_NONE;   /* Configurable by arguments */
#endif

    /* configure system log */
    setlogmask(LOG_UPTO (LOG_DEBUG)); /* LOG_DEBUG - log all messages */
    openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER); /* print also to standard error */

    /* Get program options */
    if(argc < 2 || strcmp(argv[1], "--help") == 0){
        printUsage(argv[0]);
        exit(EXIT_SUCCESS);
    }
    while((opt = getopt(argc, argv, "i:p:rc:t:s:a:")) != -1) {
        switch (opt) {
            case 'i':
                CO_ownNodeId = strtol(optarg, NULL, 0);
                nodeIdFromArgs = true;
                break;
            case 'p': rtPriority = strtol(optarg, NULL, 0); break;
            case 'r': rebootEnable = true;                  break;
#if CO_CONFIG_309 > 0
            case 'c':
                /* In case of empty string keep default name, just enable interface. */
                if(strlen(optarg) != 0) {
                    CO_command_socketPath = optarg;
                }
                commandEnable = CMD_LOCAL;
                break;
            case 't':
                /* In case of empty string keep default port, just enable interface. */
                if(strlen(optarg) != 0) {
                  //CO_command_socket_tcp_port = optarg;
                  int scanResult = sscanf(optarg, "%hu", &CO_command_socket_tcp_port);
                  if(scanResult != 1){ //expect one argument to be extracted
                      log_printf(LOG_CRIT, DBG_NOT_TCP_PORT, optarg);
                      exit(EXIT_FAILURE);
                  }
                }
                commandEnable = CMD_REMOTE;
                break;
#endif
            case 's': odStorFile_rom = optarg;              break;
            case 'a': odStorFile_eeprom = optarg;           break;
            default:
                printUsage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(optind < argc) {
        CANdevice = argv[optind];
        CANdevice0Index = if_nametoindex(CANdevice);
    }

    if(nodeIdFromArgs && (CO_ownNodeId < 1 || CO_ownNodeId > 127)) {
        log_printf(LOG_CRIT, DBG_WRONG_NODE_ID, CO_ownNodeId);
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(rtPriority != -1 && (rtPriority < sched_get_priority_min(SCHED_FIFO)
                         || rtPriority > sched_get_priority_max(SCHED_FIFO))) {
        log_printf(LOG_CRIT, DBG_WRONG_PRIORITY, rtPriority);
        printUsage(argv[0]);
        exit(EXIT_FAILURE);
    }

    if(CANdevice0Index == 0) {
        log_printf(LOG_CRIT, DBG_NO_CAN_DEVICE, CANdevice);
        exit(EXIT_FAILURE);
    }


    log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_ownNodeId, "starting");


    /* Allocate memory for CANopen objects */
    err = CO_new(NULL);
    if (err != CO_ERROR_NO) {
        log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_new()", err);
        exit(EXIT_FAILURE);
    }


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


    /* Catch signals SIGINT and SIGTERM */
    if(signal(SIGINT, sigHandler) == SIG_ERR) {
        log_printf(LOG_CRIT, DBG_ERRNO, "signal(SIGINT, sigHandler)");
        exit(EXIT_FAILURE);
    }
    if(signal(SIGTERM, sigHandler) == SIG_ERR) {
        log_printf(LOG_CRIT, DBG_ERRNO, "signal(SIGTERM, sigHandler)");
        exit(EXIT_FAILURE);
    }


    while(reset != CO_RESET_APP && reset != CO_RESET_QUIT && CO_endProgram == 0) {
/* CANopen communication reset - initialize CANopen objects *******************/

        log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_ownNodeId, "communication reset");


#if CO_CONFIG_309 > 0
        /* Wait other threads (command interface). */
        pthread_mutex_lock(&CO_CAN_VALID_mtx);
#endif

        /* Wait rt_thread. */
        if(!firstRun) {
            CO_LOCK_OD();
            CO->CANmodule[0]->CANnormal = false;
            CO_UNLOCK_OD();
        }


        /* Enter CAN configuration. */
        CO_CANsetConfigurationMode((void *)CANdevice0Index);


        /* initialize CANopen */
        if(!nodeIdFromArgs) {
            /* use value from Object dictionary, if not set by program arguments */
            CO_ownNodeId = OD_CANNodeID;
        }

        err = CO_CANinit((void *)CANdevice0Index, 0 /* bit rate not used */);
        if(err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_CANinit()", err);
            exit(EXIT_FAILURE);
        }

        err = CO_CANopenInit(CO_ownNodeId);
        if(err != CO_ERROR_NO) {
            log_printf(LOG_CRIT, DBG_CAN_OPEN, "CO_CANopenInit()", err);
            exit(EXIT_FAILURE);
        }

        /* initialize callbacks */
        CO_EM_initCallbackRx(CO->em, EmergencyRxCallback);
        CO_NMT_initCallbackChange(CO->NMT, NmtChangeCallback);
        CO_HBconsumer_initCallbackNmtChanged(CO->HBcons, NULL,
                                             HeartbeatNmtChangedCallback);


        /* initialize OD objects 1010 and 1011 and verify errors. */
        CO_OD_configure(CO->SDO[0], OD_H1010_STORE_PARAM_FUNC, CO_ODF_1010, (void*)&odStor, 0, 0U);
        CO_OD_configure(CO->SDO[0], OD_H1011_REST_PARAM_FUNC, CO_ODF_1011, (void*)&odStor, 0, 0U);
        if(odStorStatus_rom != CO_ERROR_NO) {
            CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)odStorStatus_rom);
        }
        if(odStorStatus_eeprom != CO_ERROR_NO) {
            CO_errorReport(CO->em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)odStorStatus_eeprom + 1000);
        }


#if CO_NO_TRACE > 0
        /* Initialize time */
        CO_time_init(&CO_time, CO->SDO[0], &OD_time.epochTimeBaseMs, &OD_time.epochTimeOffsetMs, 0x2130);
#endif

        /* First time only initialization. */
        if(firstRun) {
            firstRun = false;


            /* Init threadMainWait structure and file descriptors */
            threadMainWait_init(MAIN_THREAD_INTERVAL_US);


            /* Init threadRT structure and file descriptors */
            CANrx_threadTmr_init(TMR_THREAD_INTERVAL_US);

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

#if CO_CONFIG_309 > 0
            /* Initialize socket command interface */
            switch(commandEnable) {
              case CMD_LOCAL:
                if(CO_command_init() != 0) {
                    CO_errExit("Socket command interface initialization failed");
                }
                log_printf(LOG_INFO, DBG_COMMAND_LOCAL_INFO, CO_command_socketPath);
                break;
              case CMD_REMOTE:
                if(CO_command_init_tcp(CO_command_socket_tcp_port) != 0) {
                    CO_errExit("Socket command interface initialization failed");
                }
                log_printf(LOG_INFO, DBG_COMMAND_TCP_INFO, CO_command_socket_tcp_port);
                break;
              default:
                break;
            }
#endif

#ifdef CO_USE_APPLICATION
            /* Execute optional additional application code */
            app_programStart();
#endif
        } /* if(firstRun) */


#ifdef CO_USE_APPLICATION
        /* Execute optional additional application code */
        app_communicationReset();
#endif


        /* start CAN */
        CO_CANsetNormalMode(CO->CANmodule[0]);

#if CO_CONFIG_309 > 0
        pthread_mutex_unlock(&CO_CAN_VALID_mtx);
#endif


        reset = CO_RESET_NOT;

        log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_ownNodeId, "running ...");


        while(reset == CO_RESET_NOT && CO_endProgram == 0) {
/* loop for normal program execution ******************************************/
            uint32_t timer1usDiff = threadMainWait_process(&reset);

#ifdef CO_USE_APPLICATION
            app_programAsync(timer1usDiff);
#endif

            CO_OD_storage_autoSave(&odStorAuto, timer1usDiff, 60000000);
        }
    }


/* program exit ***************************************************************/
    /* join threads */
#if CO_CONFIG_309 > 0
    switch (commandEnable)
    {
      case CMD_LOCAL:
        if (CO_command_clear() != 0) {
          CO_errExit("Socket command interface removal failed");
        }
        break;
      case CMD_REMOTE:
        //nothing to do yet
        break;
      default:
        break;
    }
#endif

    CO_endProgram = 1;
    if (pthread_join(rt_thread_id, NULL) != 0) {
        log_printf(LOG_CRIT, DBG_ERRNO, "pthread_join()");
        exit(EXIT_FAILURE);
    }

#ifdef CO_USE_APPLICATION
    /* Execute optional additional application code */
    app_programEnd();
#endif

    /* Store CO_OD_EEPROM */
    CO_OD_storage_autoSave(&odStorAuto, 0, 0);
    CO_OD_storage_autoSaveClose(&odStorAuto);

    /* delete objects from memory */
    CANrx_threadTmr_close();
    threadMainWait_close();
    CO_delete((void *)CANdevice0Index);

    log_printf(LOG_INFO, DBG_CAN_OPEN_INFO, CO_ownNodeId, "finished");

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


/*******************************************************************************
 * Realtime thread for CAN receive and threadTmr
 ******************************************************************************/
static void* rt_thread(void* arg) {

    /* Endless loop */
    while(CO_endProgram == 0) {

        CANrx_threadTmr_process();

#if CO_NO_TRACE > 0
        /* Monitor variables with trace objects */
        CO_time_process(&CO_time);
        for(i=0; i<OD_traceEnable && i<CO_NO_TRACE; i++) {
            CO_trace_process(CO->trace[i], *CO_time.epochTimeOffsetMs);
        }
#endif

#ifdef CO_USE_APPLICATION
        /* Execute optional additional application code */
        app_program1ms();
#endif

    }

    return NULL;
}
