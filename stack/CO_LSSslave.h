/**
 * CANopen LSS Master/Slave protocol.
 *
 * @file        CO_LSSslave.h
 * @ingroup     CO_LSS
 * @author      Martin Wagner
 * @copyright   2017 - 2020 Neuberger Gebaeudeautomation GmbH
 *
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


#ifndef CO_LSSslave_H
#define CO_LSSslave_H

#ifdef __cplusplus
extern "C" {
#endif

#if CO_NO_LSS_SERVER == 1

#include "CO_LSS.h"

/**
 * @addtogroup CO_LSS
 * @defgroup CO_LSSslave LSS Slave
 * @ingroup CO_LSS
 * @{
 *
 * CANopen Layer Setting Service - server protocol
 *
 * The server/slave provides the following services
 * - node selection via LSS address
 * - node selection via LSS fastscan
 * - Inquire LSS address of currently selected node
 * - Inquire node ID
 * - Configure bit timing
 * - Configure node ID
 * - Activate bit timing parameters
 * - Store configuration (bit rate and node ID)
 *
 * After CAN module start, the LSS server and NMT server are started and then
 * coexist alongside each other. To achieve this behaviour, the CANopen node
 * startup process has to be conrolled more detailled. Therefore, the function
 * CO_init() is split up into the functions #CO_new(), #CO_CANinit(), #CO_LSSinit()
 * and #CO_CANopenInit().
 * Moreover, the LSS server needs to pause the NMT server initialization in case
 * no valid node ID is available at start up.
 *
 * ###Example
 *
 * It is strongly recommended that the user already has a fully working application
 * running with the standard (non LSS) version of CANopenNode. This is required
 * to understand what this example does and where you need to change it for your
 * requirements.
 *
 * The following code is only a suggestion on how to use the LSS server. It is
 * not a working example! To simplify the code, no error handling is
 * included. For stable code, proper error handling has to be added to the user
 * code.
 *
 * This example is not intended for bare metal targets. If you intend to do CAN
 * message receiving inside interrupt, be aware that the callback functions
 * will be called inside the interrupt handler context!
 *
 * \code{.c}

 const uint16_t FIRST_BIT = 125;
 queue changeBitRate;
 uint8_t activeNid;
 uint16_t activeBit;

 bool_t checkBitRateCallback(void *object, uint16_t bitRate)
 {
     if (validBit(bitRate)) {
         return true;
     }
     return false;
 }

 void activateBitRateCallback(void *object, uint16_t delay)
 {
     int time = getCurrentTime();
     queueSend(&changeBitRate, time, delay);
 }

 bool_t cfgStoreCallback(void *object, uint8_t id, uint16_t bitRate)
 {
     savePersistent(id, bitRate);
     return true;
 }

 void start_canopen(uint8_t nid)
 {
    uint8_t persistentNid;
    uint8_t pendingNid;
    uint16_t persistentBit;
    uint16_t pendingBit;

    loadPersistent(&persistentNid, &persistentBit);

    if ( ! validBit(persistentBit)) {
        printf("no bit rate found, defaulting to %d", FIRST_BIT);
        pendingBit = FIRST_BIT;
    }
    else {
        printf("loaded bit rate from nvm: %d", persistentBit);
        pendingBit = persistentBit;
    }

    if (nid == 0) {
        if ( ! validNid(persistentNid)) {
            pendingNid = CO_LSS_NODE_ID_ASSIGNMENT;
            printf("no node id found, needs to be set by LSS. NMT will"
                   "not be started until valid node id is set");
        }
        else {
            printf("loaded node id from nvm: %d", persistentNid);
            pendingNid = persistentNid;
        }
    }
    else {
        printf("node id provided by application: %d", nid);
        pendingNid = nid;
    }

    CO_new();
    CO_CANinit(0, pendingBit);
    CO_LSSinit(pendingNid, pendingBit);
    CO_CANsetNormalMode(CO->CANmodule[0]);
    activeBit = pendingBit;

    CO_LSSslave_initCheckBitRateCallback(CO->LSSslave, NULL, checkBitRateCallback);
    CO_LSSslave_initActivateBitRateCallback(CO->LSSslave, NULL, activateBitRateCallback);
    CO_LSSslave_initCfgStoreCallback(CO->LSSslave, NULL, cfgStoreCallback);

    while (1) {
        CO_LSSslave_process(CO->LSSslave, activeBit, activeNid,
                            &pendingBit, &pendingNid);
        if (pendingNid!=CO_LSS_NODE_ID_ASSIGNMENT &&
            CO_LSSslave_getState(CO->LSSslave)==CO_LSS_STATE_WAITING) {
             printf("node ID has been found: %d", pendingNid);
             break;
        }

        if ( ! queueEmpty(&changeBitRate)) {
            printf("bit rate change requested: %d", pendingBit);
            int time;
            uint16_t delay;
            queueReceive(&changeBitRate, time, delay);
            delayUntil(time + delay);
            CO_CANsetBitrate(CO->CANmodule[0], pendingBit);
            delay(delay);
        }

        printf("waiting for node id");
        CO_CANrxWait(CO->CANmodule[0]);
    }

    CO_CANopenInit(pendingNid);
    activeNid = pendingNid;

    printf("from this on, initialization doesn't differ to non-LSS version"
           "You can now intialize your CO_CANrxWait() thread or interrupt");
 }

 void main(void)
 {
     uint8_t pendingNid;
     uint16_t pendingBit;

     printf("like example in dir \"example\"");

     CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
     uint16_t timer1msPrevious;

     start_canopen(0);

     reset = CO_RESET_NOT;
     timer1msPrevious = CO_timer1ms;
     while(reset == CO_RESET_NOT){
         printf("loop for normal program execution");
         uint16_t timer1msCopy, timer1msDiff;

         timer1msCopy = CO_timer1ms;
         timer1msDiff = timer1msCopy - timer1msPrevious;
         timer1msPrevious = timer1msCopy;

         reset = CO_process(CO, timer1msDiff, NULL);

         CO_LSSslave_process(CO->LSSslave, activeBit, activeNid,
                                           &pendingBit, &pendingNid);
         if (reset == CO_RESET_COMM) {
             printf("restarting CANopen using pending node ID %d", pendingNid);
             CO_delete(0);
             start_canopen(pendingNid);
             reset = CO_RESET_NOT;
         }
         if ( ! queueEmpty(&changeBitRate)) {
             printf("bit rate change requested: %d", pendingBit);
             int time;
             uint16_t delay;
             queueReceive(&changeBitRate, time, delay);
             printf("Disabling CANopen for givent time");
             pauseReceiveThread();
             delayUntil(time + delay);
             CO_CANsetBitrate(CO->CANmodule[0], pendingBit);
             delay(delay);
             resumeReceiveThread();
             printf("Re-enabling CANopen after bit rate switch");
         }
     }
 }

 * \endcode
 */

/**
 * LSS slave object.
 */
typedef struct{
    CO_LSS_address_t        lssAddress;       /**< From #CO_LSSslave_init */
    CO_LSS_state_t          lssState;         /**< #CO_LSS_state_t */
    CO_LSS_address_t        lssSelect;        /**< Received LSS Address by select */

    CO_LSS_address_t        lssFastscan;      /**< Received LSS Address by fastscan */
    uint8_t                 fastscanPos;      /**< Current state of fastscan */

    uint16_t                pendingBitRate;   /**< Bit rate value that is temporarily configured in volatile memory */
    uint8_t                 pendingNodeID;    /**< Node ID that is temporarily configured in volatile memory */
    uint8_t                 activeNodeID;     /**< Node ID used at the CAN interface */

    bool_t                (*pFunctLSScheckBitRate)(void *object, uint16_t bitRate); /**< From CO_LSSslave_initCheckBitRateCallback() or NULL */
    void                   *functLSScheckBitRateObject; /** Pointer to object */
    void                  (*pFunctLSSactivateBitRate)(void *object, uint16_t delay); /**< From CO_LSSslave_initActivateBitRateCallback() or NULL. Delay is in ms */
    void                   *functLSSactivateBitRateObject; /** Pointer to object */
    bool_t                (*pFunctLSScfgStore)(void *object, uint8_t id, uint16_t bitRate); /**< From CO_LSSslave_initCfgStoreCallback() or NULL */
    void                   *functLSScfgStore; /** Pointer to object */

    CO_CANmodule_t         *CANdevTx;         /**< From #CO_LSSslave_init() */
    CO_CANtx_t             *TXbuff;           /**< CAN transmit buffer */
}CO_LSSslave_t;

/**
 * Initialize LSS object.
 *
 * Function must be called in the communication reset section.
 *
 * Depending on the startup type, pending bit rate and node ID have to be
 * supplied differently. After #CO_NMT_RESET_NODE or at power up they should
 * be restored from persitent bit rate and node id. After #CO_NMT_RESET_COMMUNICATION
 * they have to be supplied from the application and are generally the values
 * that have been last returned by #CO_LSSslave_process() before resetting.
 *
 * @remark The LSS address needs to be unique on the network. For this, the 128
 * bit wide identity object (1018h) is used. Therefore, this object has to be fully
 * initalized before passing it to this function.
 *
 * @param LSSslave This object will be initialized.
 * @param lssAddress LSS address
 * @param pendingBitRate Bit rate of the CAN interface.
 * @param pendingNodeID Node ID or 0xFF - invalid.
 * @param CANdevRx CAN device for LSS slave reception.
 * @param CANdevRxIdx Index of receive buffer in the above CAN device.
 * @param CANidLssMaster COB ID for reception.
 * @param CANdevTx CAN device for LSS slave transmission.
 * @param CANdevTxIdx Index of transmit buffer in the above CAN device.
 * @param CANidLssSlave COB ID for transmission.
 * @return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_LSSslave_init(
        CO_LSSslave_t          *LSSslave,
        CO_LSS_address_t        lssAddress,
        uint16_t                pendingBitRate,
        uint8_t                 pendingNodeID,
        CO_CANmodule_t         *CANdevRx,
        uint16_t                CANdevRxIdx,
        uint32_t                CANidLssMaster,
        CO_CANmodule_t         *CANdevTx,
        uint16_t                CANdevTxIdx,
        uint32_t                CANidLssSlave);

/**
 * Process LSS communication
 *
 * - sets currently active node ID and bit rate so master can read it
 * - hands over pending node ID and bit rate to user application
 *
 * @param LSSslave This object.
 * @param activeBitRate Currently active bit rate
 * @param activeNodeId Currently active node ID
 * @param pendingBitRate [out] Requested bit rate
 * @param pendingNodeId [out] Requested node id
 */
void CO_LSSslave_process(
        CO_LSSslave_t          *LSSslave,
        uint16_t                activeBitRate,
        uint8_t                 activeNodeId,
        uint16_t               *pendingBitRate,
        uint8_t                *pendingNodeId);

/**
 * Get current LSS state
 *
 * @param LSSslave This object.
 * @return #CO_LSS_state_t
 */
CO_LSS_state_t CO_LSSslave_getState(
        CO_LSSslave_t          *LSSslave);

/**
 * Process LSS LED
 *
 * Returns the status of the LSS LED (if LSS is involved)
 * with the following meaning:
 *
 * UNCONFIGURED (activeNodeId is unconfigured) --> single flash
 * SELECTED                                    --> double flash
 *
 * If none of above conditions apply, returns false.
 *
 * @param LSSslave This object.
 * @param timeDifference_ms The amount of time elapsed since the last call
 * @param LEDon [out] LED state
 *
 * @return true if LSS is involved (unconfigured node or selected node)
 */
bool_t CO_LSSslave_LEDprocess(
        CO_LSSslave_t          *LSSslave,
        uint16_t                timeDifference_ms,
        bool_t *LEDon);

/**
 * Initialize verify bit rate callback
 *
 * Function initializes callback function, which is called when "config bit
 * timing parameters" is used. The callback function needs to check if the new bit
 * rate is supported by the CANopen device. Callback returns "true" if supported.
 * When no callback is set the LSS server will no-ack the request, indicating to
 * the master that bit rate change is not supported.
 *
 * @remark Depending on the CAN driver implementation, this function is called
 * inside an ISR
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctLSScheckBitRate(). Can be NULL
 * @param pFunctLSScheckBitRate Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initCheckBitRateCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        bool_t                (*pFunctLSScheckBitRate)(void *object, uint16_t bitRate));

/**
 * Initialize activate bit rate callback
 *
 * Function initializes callback function, which is called when "activate bit
 * timing parameters" is used. The callback function gives the user an event to
 * allow setting a timer or do calculations based on the exact time the request
 * arrived.
 * According to DSP 305 6.4.4, the delay has to be applied once before and once after
 * switching bit rates. During this time, a device musn't send any messages.
 *
 * @remark Depending on the CAN driver implementation, this function is called
 * inside an ISR
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctLSSactivateBitRate(). Can be NULL
 * @param pFunctLSSactivateBitRate Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initActivateBitRateCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        void                  (*pFunctLSSactivateBitRate)(void *object, uint16_t delay));

/**
 * Store configuration callback
 *
 * Function initializes callback function, which is called when "store configuration" is used.
 * The callback function gives the user an event to store the corresponding node id and bit rate
 * to NVM. Those values have to be supplied to the init function as "persistent values"
 * after reset. If callback returns "true", success is send to the LSS master. When no
 * callback is set the LSS server will no-ack the request, indicating to the master
 * that storing is not supported.
 *
 * @remark Depending on the CAN driver implementation, this function is called
 * inside an ISR
 *
 * @param LSSslave This object.
 * @param object Pointer to object, which will be passed to pFunctLSScfgStore(). Can be NULL
 * @param pFunctLSScfgStore Pointer to the callback function. Not called if NULL.
 */
void CO_LSSslave_initCfgStoreCallback(
        CO_LSSslave_t          *LSSslave,
        void                   *object,
        bool_t                (*pFunctLSScfgStore)(void *object, uint8_t id, uint16_t bitRate));

#else /* CO_NO_LSS_SERVER == 1 */

/**
 * @addtogroup CO_LSS
 * @{
 * If you need documetation for LSS slave usage, add "CO_NO_LSS_SERVER=1" to doxygen
 * "PREDEFINED" variable.
 *
 */

#endif /* CO_NO_LSS_SERVER == 1 */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
