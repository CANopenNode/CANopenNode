/**
 * Main CANopen stack file.
 *
 * It combines Object dictionary (CO_OD) and all other CANopen source files.
 * Configuration information are read from CO_OD.h file. This file uses one
 * CAN module. If multiple CAN modules are to be used, then this file may be
 * customized for different CANopen configuration. (One or multiple CANopen
 * device on one or multiple CAN modules.)
 *
 * @file        CANopen.h
 * @ingroup     CO_CANopen
 * @author      Janez Paternoster
 * @author      Uwe Kindler
 * @copyright   2010 - 2020 Janez Paternoster
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


#ifndef CANopen_H
#define CANopen_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_CANopen CANopen stack
 * @{
 *
 * CANopenNode is free and open source CANopen Stack.
 *
 * CANopen is the internationally standardized (EN 50325-4) (CiA DS-301)
 * CAN-based higher-layer protocol for embedded control system. For more
 * information on CANopen see http://www.can-cia.org/
 *
 * CANopenNode homepage is https://github.com/CANopenNode/CANopenNode
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


    #include "CO_driver.h"
    #include "CO_OD.h"
    #include "CO_SDO.h"
    #include "CO_Emergency.h"
    #include "CO_NMT_Heartbeat.h"
    #include "CO_SYNC.h"
    #include "CO_TIME.h"
    #include "CO_PDO.h"
    #include "CO_HBconsumer.h"
#if CO_NO_SDO_CLIENT != 0
    #include "CO_SDOmaster.h"
#endif
#if CO_NO_TRACE > 0
    #include "CO_trace.h"
#endif
#if CO_NO_LSS_SERVER == 1
    #include "CO_LSSslave.h"
#endif
#if CO_NO_LSS_CLIENT == 1
    #include "CO_LSSmaster.h"
#endif

/**
 * Default CANopen identifiers.
 *
 * Default CANopen identifiers for CANopen communication objects. Same as
 * 11-bit addresses of CAN messages. These are default identifiers and
 * can be changed in CANopen. Especially PDO identifiers are confgured
 * in PDO linking phase of the CANopen network configuration.
 */
typedef enum{
     CO_CAN_ID_NMT_SERVICE       = 0x000,   /**< 0x000, Network management */
     CO_CAN_ID_SYNC              = 0x080,   /**< 0x080, Synchronous message */
     CO_CAN_ID_EMERGENCY         = 0x080,   /**< 0x080, Emergency messages (+nodeID) */
	 CO_CAN_ID_TIME        		 = 0x100,   /**< 0x100, Time message */
     CO_CAN_ID_TPDO_1            = 0x180,   /**< 0x180, Default TPDO1 (+nodeID) */
     CO_CAN_ID_RPDO_1            = 0x200,   /**< 0x200, Default RPDO1 (+nodeID) */
     CO_CAN_ID_TPDO_2            = 0x280,   /**< 0x280, Default TPDO2 (+nodeID) */
     CO_CAN_ID_RPDO_2            = 0x300,   /**< 0x300, Default RPDO2 (+nodeID) */
     CO_CAN_ID_TPDO_3            = 0x380,   /**< 0x380, Default TPDO3 (+nodeID) */
     CO_CAN_ID_RPDO_3            = 0x400,   /**< 0x400, Default RPDO3 (+nodeID) */
     CO_CAN_ID_TPDO_4            = 0x480,   /**< 0x480, Default TPDO4 (+nodeID) */
     CO_CAN_ID_RPDO_4            = 0x500,   /**< 0x500, Default RPDO5 (+nodeID) */
     CO_CAN_ID_TSDO              = 0x580,   /**< 0x580, SDO response from server (+nodeID) */
     CO_CAN_ID_RSDO              = 0x600,   /**< 0x600, SDO request from client (+nodeID) */
     CO_CAN_ID_HEARTBEAT         = 0x700,   /**< 0x700, Heartbeat message */
     CO_CAN_ID_LSS_CLI           = 0x7E4,   /**< 0x7E4, LSS response from server to client */
     CO_CAN_ID_LSS_SRV           = 0x7E5    /**< 0x7E5, LSS request from client to server */
}CO_Default_CAN_ID_t;


/**
 * CANopen stack object combines pointers to all CANopen objects.
 */
typedef struct{
    CO_CANmodule_t     *CANmodule[1];   /**< CAN module objects */
    CO_SDO_t           *SDO[CO_NO_SDO_SERVER]; /**< SDO object */
    CO_EM_t            *em;             /**< Emergency report object */
    CO_EMpr_t          *emPr;           /**< Emergency process object */
    CO_NMT_t           *NMT;            /**< NMT object */
    CO_SYNC_t          *SYNC;           /**< SYNC object */
    CO_TIME_t          *TIME;           /**< TIME object */
    CO_RPDO_t          *RPDO[CO_NO_RPDO];/**< RPDO objects */
    CO_TPDO_t          *TPDO[CO_NO_TPDO];/**< TPDO objects */
    CO_HBconsumer_t    *HBcons;         /**<  Heartbeat consumer object*/
#if CO_NO_LSS_SERVER == 1
    CO_LSSslave_t      *LSSslave;       /**< LSS server/slave object */
#endif
#if CO_NO_LSS_CLIENT == 1
    CO_LSSmaster_t     *LSSmaster;      /**< LSS master/client object */
#endif
#if CO_NO_SDO_CLIENT != 0
    CO_SDOclient_t     *SDOclient[CO_NO_SDO_CLIENT]; /**< SDO client object */
#endif
#if CO_NO_TRACE > 0
    CO_trace_t         *trace[CO_NO_TRACE]; /**< Trace object for monitoring variables */
#endif
}CO_t;


/** CANopen object */
    extern CO_t *CO;


/**
 * Function CO_sendNMTcommand() is simple function, which sends CANopen message.
 * This part of code is an example of custom definition of simple CANopen
 * object. Follow the code in CANopen.c file. If macro CO_NO_NMT_MASTER is 1,
 * function CO_sendNMTcommand can be used to send NMT master message.
 *
 * @param co CANopen object.
 * @param command NMT command.
 * @param nodeID Node ID.
 *
 * @return 0: Operation completed successfully.
 * @return other: same as CO_CANsend().
 */
#if CO_NO_NMT_MASTER == 1
    CO_ReturnError_t CO_sendNMTcommand(CO_t *co, uint8_t command, uint8_t nodeID);
#endif


#if CO_NO_LSS_SERVER == 1
/**
 * Allocate and initialize memory for CANopen object
 *
 * Function must be called in the communication reset section.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_OUT_OF_MEMORY
 */
CO_ReturnError_t CO_new(void);


/**
 * Initialize CAN driver
 *
 * Function must be called in the communication reset section.
 *
 * @param CANdriverState Pointer to the CAN module, passed to CO_CANmodule_init().
 * @param bitRate CAN bit rate.
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_ILLEGAL_BAUDRATE, CO_ERROR_OUT_OF_MEMORY
 */
CO_ReturnError_t CO_CANinit(
        void                   *CANdriverState,
        uint16_t                bitRate);


/**
 * Initialize CANopen LSS slave
 *
 * Function must be called in the communication reset section.
 *
 * @param nodeId Node ID of the CANopen device (1 ... 127) or CO_LSS_NODE_ID_ASSIGNMENT
 * @param bitRate CAN bit rate.
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT
 */
CO_ReturnError_t CO_LSSinit(
        uint8_t                 nodeId,
        uint16_t                bitRate);


/**
 * Initialize CANopen stack.
 *
 * Function must be called in the communication reset section.
 *
 * @param nodeId Node ID of the CANopen device (1 ... 127).
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT
 */
CO_ReturnError_t CO_CANopenInit(
        uint8_t                 nodeId);


#else /* CO_NO_LSS_SERVER == 1 */
/**
 * Initialize CANopen stack.
 *
 * Function must be called in the communication reset section.
 *
 * @param CANdriverState Pointer to the user-defined CAN base structure, passed to CO_CANmodule_init().
 * @param nodeId Node ID of the CANopen device (1 ... 127).
 * @param bitRate CAN bit rate.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_OUT_OF_MEMORY, CO_ERROR_ILLEGAL_BAUDRATE
 */
CO_ReturnError_t CO_init(
        void                   *CANdriverState,
        uint8_t                 nodeId,
        uint16_t                bitRate);

#endif /* CO_NO_LSS_SERVER == 1 */


/**
 * Delete CANopen object and free memory. Must be called at program exit.
 *
 * @param CANdriverState Pointer to the user-defined CAN base structure, passed to CO_CANmodule_init().
 */
void CO_delete(void *CANdriverState);


/**
 * Process CANopen objects.
 *
 * Function must be called cyclically. It processes all "asynchronous" CANopen
 * objects.
 *
 * @param co CANopen object.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 * @param timerNext_ms Return value - info to OS - maximum delay after function
 *        should be called next time in [milliseconds]. Value can be used for OS
 *        sleep time. Initial value must be set to something, 50ms typically.
 *        Output will be equal or lower to initial value. If there is new object
 *        to process, delay should be suspended and this function should be
 *        called immediately. Parameter is ignored if NULL.
 *
 * @return #CO_NMT_reset_cmd_t from CO_NMT_process().
 */
CO_NMT_reset_cmd_t CO_process(
        CO_t                   *co,
        uint16_t                timeDifference_ms,
        uint16_t               *timerNext_ms);


#if CO_NO_SYNC == 1
/**
 * Process CANopen SYNC objects.
 *
 * Function must be called cyclically from real time thread with constant
 * interval (1ms typically). It processes SYNC CANopen objects.
 *
 * @param co CANopen object.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 *
 * @return True, if CANopen SYNC message was just received or transmitted.
 */
bool_t CO_process_SYNC(
        CO_t                   *co,
        uint32_t                timeDifference_us);
#endif

/**
 * Process CANopen RPDO objects.
 *
 * Function must be called cyclically from real time thread with constant.
 * interval (1ms typically). It processes receive PDO CANopen objects.
 *
 * @param co CANopen object.
 * @param syncWas True, if CANopen SYNC message was just received or transmitted.
 */
void CO_process_RPDO(
        CO_t                   *co,
        bool_t                  syncWas);

/**
 * Process CANopen TPDO objects.
 *
 * Function must be called cyclically from real time thread with constant.
 * interval (1ms typically). It processes transmit PDO CANopen objects.
 *
 * @param co CANopen object.
 * @param syncWas True, if CANopen SYNC message was just received or transmitted.
 * @param timeDifference_us Time difference from previous function call in [microseconds].
 */
void CO_process_TPDO(
        CO_t                   *co,
        bool_t                  syncWas,
        uint32_t                timeDifference_us);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
