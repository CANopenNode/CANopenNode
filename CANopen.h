/**
 * Main CANopenNode file.
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

/**
 * @defgroup CO_CANopen CANopen
 * @{
 *
 * CANopenNode is free and open source implementation of CANopen communication
 * protocol.
 *
 * CANopen is the internationally standardized (EN 50325-4) (CiA DS-301)
 * CAN-based higher-layer protocol for embedded control system. For more
 * information on CANopen see http://www.can-cia.org/
 *
 * CANopenNode homepage is https://github.com/CANopenNode/CANopenNode
 *
 * CANopen.h file combines Object dictionary (CO_OD) and all other CANopen
 * source files. Configuration information are read from CO_OD.h file.
 * CO_OD.h/.c files defines CANopen Object Dictionary and are generated by
 * external tool.
 * CANopen.h file contains most common configuration of CANopenNode objects
 * and can also be a template for custom, more complex configurations.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * @}
 */

/**
 * @defgroup CO_CANopen_301 CANopen_301
 * @{
 *
 * CANopen application layer and communication profile (CiA 301 v4.2.0)
 *
 * Definitions of data types, encoding rules, object dictionary objects and
 * CANopen communication services and protocols.
 * @}
 */

/**
 * @defgroup CO_CANopen_303 CANopen_303
 * @{
 *
 * CANopen recommendation for indicator specification (CiA 303-3 v1.4.0)
 *
 * Description of communication related indicators - green and red LED diodes.
 * @}
 */

/**
 * @defgroup CO_CANopen_304 CANopen_304
 * @{
 *
 * CANopen Safety (EN 50325­-5:2010 (formerly CiA 304))
 *
 * Standard defines the usage of Safety Related Data Objects (SRDO) and the GFC.
 * This is an additional protocol (to SDO, PDO) to exchange data.
 * The meaning of "security" here refers not to security (crypto) but to data consistency.
 * @}
 */

/**
 * @defgroup CO_CANopen_305 CANopen_305
 * @{
 *
 * CANopen layer setting services (LSS) and protocols (CiA 305 DSP v3.0.0)
 *
 * Inquire or change three parameters on a CANopen device with LSS slave
 * capability by a CANopen device with LSS master capability via the CAN
 * network: the settings of Node-ID of the CANopen device, bit timing
 * parameters of the physical layer (bit rate) or LSS address compliant to the
 * identity object (1018h).
 * @}
 */

/**
 * @defgroup CO_CANopen_309 CANopen_309
 * @{
 *
 * CANopen access from other networks (CiA 309)
 *
 * Standard defines the services and protocols to interface CANopen networks to
 * other networks. Standard is organized as follows:
 * - Part 1: General principles and services
 * - Part 2: Modbus/TCP mapping
 * - Part 3: ASCII mapping
 * - Part 4: Amendment 7 to Fieldbus Integration into PROFINET IO
 * @}
 */

/**
 * @defgroup CO_CANopen_extra CANopen_extra
 * @{
 *
 * Additional non-standard objects related to CANopenNode.
 * @}
 */

/**
 * @addtogroup CO_CANopen
 * @{
 */

    #include <stdint.h>
    #include "301/CO_driver.h"
    #include "301/CO_SDOserver.h"
    #include "301/CO_Emergency.h"
    #include "301/CO_NMT_Heartbeat.h"
    #include "301/CO_TIME.h"
    #include "301/CO_PDO.h"
    #include "301/CO_HBconsumer.h"
    #include "CO_OD.h"


#ifdef CO_DOXYGEN
/**
 * @defgroup CO_NO_OBJ CANopen configuration
 *
 * Definitions specify, which and how many CANopenNode communication objects
 * will be used in current configuration. Usage of some objects is mandatory and
 * is fixed. Others are defined in CO_OD.h.
 * @{
 */
/* Definitions valid only for documentation. */
/** Number of NMT objects, fixed to 1 slave(CANrx) */
#define CO_NO_NMT (1)
/** Number of NMT master objects, 0 or 1 master(CANtx). It depends on
 * @ref CO_CONFIG_NMT setting. */
#define CO_NO_NMT_MST (0 - 1)
/** Number of SYNC objects, 0 or 1 (consumer(CANrx) + producer(CANtx)) */
#define CO_NO_SYNC (0 - 1)
/** Number of Emergency producer objects, fixed to 1 producer(CANtx) */
#define CO_NO_EMERGENCY (1)
/** Number of Emergency consumer objects, 0 or 1 consumer(CANrx). It depends on
 * @ref CO_CONFIG_EM setting. */
#define CO_NO_EM_CONS (0 - 1)
/** Number of Time-stamp objects, 0 or 1 (consumer(CANrx) + producer(CANtx)) */
#define CO_NO_TIME (0 - 1)
/** Number of GFC objects, 0 or 1 (consumer(CANrx) + producer(CANtx)) */
#define CO_NO_GFC (0 - 1)
/** Number of SRDO objects, 0 to 64 (consumer(CANrx) + producer(CANtx)) */
#define CO_NO_SRDO (0 - 64)
/** Number of RPDO objects, 1 to 512 consumers (CANrx) */
#define CO_NO_RPDO (1 - 512)
/** Number of TPDO objects, 1 to 512 producers (CANtx) */
#define CO_NO_TPDO (1 - 512)
/** Number of SDO server objects, from 1 to 128 (CANrx + CANtx) */
#define CO_NO_SDO_SERVER (1 - 128)
/** Number of SDO client objects, from 0 to 128 (CANrx + CANtx) */
#define CO_NO_SDO_CLIENT (0 - 128)
/** Number of HB producer objects, fixed to 1 producer(CANtx) */
#define CO_NO_HB_PROD (1)
/** Number of HB consumer objects, from 0 to 127 consumers(CANrx) */
#define CO_NO_HB_CONS (0 - 127)
/** Number of LSS slave objects, 0 or 1 (CANrx + CANtx). It depends on
 * @ref CO_CONFIG_LSS setting. */
#define CO_NO_LSS_SLAVE (0 - 1)
/** Number of LSS master objects, 0 or 1 (CANrx + CANtx). It depends on
 * @ref CO_CONFIG_LSS setting. */
#define CO_NO_LSS_MASTER (0 - 1)
/** Number of Trace objects, 0 to many */
#define CO_NO_TRACE (0 - )
/** @} */

#else  /* CO_DOXYGEN */

/* Valid Definitions for program. */
/* NMT slave count (fixed) */
#define CO_NO_NMT     1

/* NMT master count depends on stack configuration */
#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
#define CO_NO_NMT_MST 1
#else
#define CO_NO_NMT_MST 0
#endif

/* Emergency consumer depends on stack configuration */
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
#define CO_NO_EM_CONS 1
#else
#define CO_NO_EM_CONS 0
#endif

/* Heartbeat producer count (fixed) */
#define CO_NO_HB_PROD 1

/* Heartbeat consumer count depends on Object Dictionary configuration */
#ifdef ODL_consumerHeartbeatTime_arrayLength
    #define CO_NO_HB_CONS ODL_consumerHeartbeatTime_arrayLength
#else
    #define CO_NO_HB_CONS 0
#endif

/* LSS slave count depends on stack configuration */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
#define CO_NO_LSS_SLAVE 1
#else
#define CO_NO_LSS_SLAVE 0
#endif

/* LSS master count depends on stack configuration */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
#define CO_NO_LSS_MASTER 1
#else
#define CO_NO_LSS_MASTER 0
#endif
#endif /* CO_DOXYGEN */


#if CO_NO_SYNC == 1 || defined CO_DOXYGEN
    #include "301/CO_SYNC.h"
#endif
#if CO_NO_SDO_CLIENT != 0 || defined CO_DOXYGEN
    #include "301/CO_SDOclient.h"
#endif
#if CO_NO_GFC != 0 || defined CO_DOXYGEN
    #include "304/CO_GFC.h"
#endif
#if CO_NO_SRDO != 0 || defined CO_DOXYGEN
    #include "304/CO_SRDO.h"
#endif
#if CO_NO_LSS_SLAVE != 0 || defined CO_DOXYGEN
    #include "305/CO_LSSslave.h"
#endif
#if ((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) || defined CO_DOXYGEN
    #include "303/CO_LEDs.h"
#endif
#if CO_NO_LSS_MASTER != 0 || defined CO_DOXYGEN
    #include "305/CO_LSSmaster.h"
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) || defined CO_DOXYGEN
    #include "309/CO_gateway_ascii.h"
#endif
#if CO_NO_TRACE != 0 || defined CO_DOXYGEN
    #include "extra/CO_trace.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

/**
 * CANopen object with pointers to all CANopenNode objects.
 */
typedef struct {
    bool_t nodeIdUnconfigured;       /**< True in unconfigured LSS slave */
    CO_CANmodule_t *CANmodule;       /**< CAN module objects */
    CO_SDO_t *SDO;                   /**< SDO server objects */
    CO_EM_t *em;                     /**< Emergency report object */
    CO_EMpr_t *emPr;                 /**< Emergency process object */
    CO_NMT_t *NMT;                   /**< NMT object */
#if CO_NO_SYNC == 1 || defined CO_DOXYGEN
    CO_SYNC_t *SYNC;                 /**< SYNC object */
#endif
    CO_TIME_t *TIME;                 /**< TIME object */
    CO_RPDO_t *RPDO;                 /**< RPDO objects */
    CO_TPDO_t *TPDO[CO_NO_TPDO];     /**< TPDO objects */
    CO_HBconsumer_t *HBcons;         /**< Heartbeat consumer object*/
#if CO_NO_SDO_CLIENT != 0 || defined CO_DOXYGEN
    CO_SDOclient_t *SDOclient[CO_NO_SDO_CLIENT]; /**< SDO client object */
#endif
#if ((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE) || defined CO_DOXYGEN
    CO_LEDs_t *LEDs;                 /**< LEDs object */
#endif
#if CO_NO_GFC != 0 || defined CO_DOXYGEN
    CO_GFC_t *GFC;                   /**< GFC objects */
#endif
#if CO_NO_SRDO != 0 || defined CO_DOXYGEN
    CO_SRDOGuard_t *SRDOGuard;       /**< SRDO objects */
    CO_SRDO_t *SRDO[CO_NO_SRDO];     /**< SRDO objects */
#endif
#if CO_NO_LSS_SLAVE == 1 || defined CO_DOXYGEN
    CO_LSSslave_t *LSSslave;         /**< LSS slave object */
#endif
#if CO_NO_LSS_MASTER == 1 || defined CO_DOXYGEN
    CO_LSSmaster_t *LSSmaster;       /**< LSS master object */
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) || defined CO_DOXYGEN
    CO_GTWA_t *gtwa;                 /**< Gateway-ascii object (CiA309-3) */
#endif
#if CO_NO_TRACE > 0 || defined CO_DOXYGEN
    CO_trace_t *trace[CO_NO_TRACE];  /**< Trace object for recording variables */
#endif
    uint16_t rpdoCount;              /**< number of RPDO objects in \a RPDO field */
    uint8_t sdoCount;                /**< number of SDO server objects in \a SDO field */
} CO_t;


/** CANopen object */
extern CO_t *CO;


/**
 * Allocate and initialize memory for CANopen object
 *
 * Function must be called the first time, after program starts.
 *
 * @remark
 * With some microcontrollers it is necessary to specify Heap size within
 * linker configuration.
 *
 * @param [out] heapMemoryUsed Information about heap memory used. Ignored if
 * NULL.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_OUT_OF_MEMORY
 */
CO_ReturnError_t CO_new(uint32_t *heapMemoryUsed);


/**
 * Delete CANopen object and free memory. Must be called at program exit.
 *
 * @param CANptr Pointer to the user-defined CAN base structure, passed to
 *               CO_CANmodule_init().
 */
void CO_delete(void *CANptr);


/**
 * Initialize CAN driver
 *
 * Function must be called in the communication reset section.
 *
 * @param CANptr Pointer to the user-defined CAN base structure, passed to
 *               CO_CANmodule_init().
 * @param bitRate CAN bit rate.
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT,
 * CO_ERROR_ILLEGAL_BAUDRATE, CO_ERROR_OUT_OF_MEMORY
 */
CO_ReturnError_t CO_CANinit(void *CANptr,
                            uint16_t bitRate);


#if CO_NO_LSS_SLAVE == 1 || defined CO_DOXYGEN
/**
 * Initialize CANopen LSS slave
 *
 * Function must be called before CO_CANopenInit.
 *
 * See #CO_LSSslave_init() for description of parameters.
 *
 * @param [in,out] pendingNodeID Pending node ID or 0xFF(unconfigured)
 * @param [in,out] pendingBitRate Pending bit rate of the CAN interface
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT
 */
CO_ReturnError_t CO_LSSinit(uint8_t *pendingNodeID,
                            uint16_t *pendingBitRate);
#endif /* CO_NO_LSS_SLAVE == 1 */


/**
 * Initialize CANopenNode.
 *
 * Function must be called in the communication reset section.
 *
 * @param nodeId CANopen Node ID (1 ... 127) or 0xFF(unconfigured). In the
 * CANopen initialization it is the same as pendingBitRate from CO_LSSinit().
 * If it is unconfigured, then some CANopen objects will not be initialized nor
 * processed.
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_ILLEGAL_ARGUMENT
 */
CO_ReturnError_t CO_CANopenInit(uint8_t nodeId);


/**
 * Process CANopen objects.
 *
 * Function must be called cyclically. It processes all "asynchronous" CANopen
 * objects.
 *
 * @param co CANopen object.
 * @param timeDifference_us Time difference from previous function call in
 *                          microseconds.
 * @param [out] timerNext_us info to OS - maximum delay time after this function
 *        should be called next time in [microseconds]. Value can be used for OS
 *        sleep time. Initial value must be set to maximum interval time.
 *        Output will be equal or lower to initial value. Calculation is based
 *        on various timers which expire in known time. Parameter should be
 *        used in combination with callbacks configured with
 *        CO_***_initCallbackPre() functions. Those callbacks should also
 *        trigger calling of CO_process() function. Parameter is ignored if
 *        NULL. See also @ref CO_CONFIG_FLAG_CALLBACK_PRE configuration macro.
 *
 * @return #CO_NMT_reset_cmd_t from CO_NMT_process().
 */
CO_NMT_reset_cmd_t CO_process(CO_t *co,
                              uint32_t timeDifference_us,
                              uint32_t *timerNext_us);


#if CO_NO_SYNC == 1 || defined CO_DOXYGEN
/**
 * Process CANopen SYNC objects.
 *
 * Function must be called cyclically from real time thread with constant
 * interval (1ms typically). It processes SYNC CANopen objects.
 *
 * @param co CANopen object.
 * @param timeDifference_us Time difference from previous function call in
 * microseconds.
 * @param [out] timerNext_us info to OS - see CO_process().
 *
 * @return True, if CANopen SYNC message was just received or transmitted.
 */
bool_t CO_process_SYNC(CO_t *co,
                       uint32_t timeDifference_us,
                       uint32_t *timerNext_us);
#endif /* CO_NO_SYNC == 1 */


/**
 * Process CANopen RPDO objects.
 *
 * Function must be called cyclically from real time thread with constant.
 * interval (1ms typically). It processes receive PDO CANopen objects.
 *
 * @param co CANopen object.
 * @param syncWas True, if CANopen SYNC message was just received or
 * transmitted.
 */
void CO_process_RPDO(CO_t *co, bool_t syncWas);


/**
 * Process CANopen TPDO objects.
 *
 * Function must be called cyclically from real time thread with constant.
 * interval (1ms typically). It processes transmit PDO CANopen objects.
 *
 * @param co CANopen object.
 * @param syncWas True, if CANopen SYNC message was just received or
 * transmitted.
 * @param timeDifference_us Time difference from previous function call in
 * microseconds.
 * @param [out] timerNext_us info to OS - see CO_process().
 */
void CO_process_TPDO(CO_t *co,
                     bool_t syncWas,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us);

#if CO_NO_SRDO != 0 || defined CO_DOXYGEN
/**
 * Process CANopen SRDO objects.
 *
 * Function must be called cyclically from real time thread with constant.
 * interval (1ms typically). It processes receive SRDO CANopen objects.
 *
 * @param co CANopen object.
 * @param timeDifference_us Time difference from previous function call in
 * microseconds.
 * @param [out] timerNext_us info to OS - see CO_process().
 */
void CO_process_SRDO(CO_t *co,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us);
#endif /* CO_NO_SRDO != 0 */

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CANopen_H */
