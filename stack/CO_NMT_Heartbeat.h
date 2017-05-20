/**
 * CANopen Network management and Heartbeat producer protocol.
 *
 * @file        CO_NMT_Heartbeat.h
 * @ingroup     CO_NMT_Heartbeat
 * @author      Janez Paternoster
 * @copyright   2004 - 2013 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
 */


#ifndef CO_NMT_HEARTBEAT_H
#define CO_NMT_HEARTBEAT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_NMT_Heartbeat NMT and Heartbeat
 * @ingroup CO_CANopen
 * @{
 *
 * CANopen Network management and Heartbeat producer protocol.
 * 
 * CANopen device can be in one of the #CO_NMT_internalState_t
 *  - Initializing. It is active before CANopen is initialized.
 *  - Pre-operational. All CANopen objects are active, except PDOs.
 *  - Operational. Process data objects (PDOs) are active too.
 *  - Stopped. Only Heartbeat producer and NMT consumer are active.
 *
 * NMT master can change the internal state of the devices by sending
 * #CO_NMT_command_t.
 * 
 * ###NMT message contents:
 *   
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *     0  | #CO_NMT_command_t
 *     1  | Node ID. If zero, command addresses all nodes.
 *
 * ###Heartbeat message contents:
 *
 *   Byte | Description
 *   -----|-----------------------------------------------------------
 *     0  | #CO_NMT_internalState_t
 *
 * @see #CO_Default_CAN_ID_t
 *
 * ###Status LED diodes
 * Macros for @ref CO_NMT_statusLEDdiodes are also implemented in this object.
 */


/**
 * @defgroup CO_NMT_statusLEDdiodes Status LED diodes
 * @{
 *
 * Macros for status LED diodes.
 *
 * Helper macros for implementing status LED diodes are used by stack and can
 * also be used by the application. If macro returns 1 LED should be ON,
 * otherwise OFF. Function CO_NMT_blinkingProcess50ms() must be called cyclically
 * to update the variables.
 */
    #define LED_FLICKERING(NMT)     (((NMT)->LEDflickering>=0)     ? 1 : 0) /**< 10HZ (100MS INTERVAL) */
    #define LED_BLINKING(NMT)       (((NMT)->LEDblinking>=0)       ? 1 : 0) /**< 2.5HZ (400MS INTERVAL) */
    #define LED_SINGLE_FLASH(NMT)   (((NMT)->LEDsingleFlash>=0)    ? 1 : 0) /**< 200MS ON, 1000MS OFF */
    #define LED_DOUBLE_FLASH(NMT)   (((NMT)->LEDdoubleFlash>=0)    ? 1 : 0) /**< 200MS ON, 200MS OFF, 200MS ON, 1000MS OFF */
    #define LED_TRIPLE_FLASH(NMT)   (((NMT)->LEDtripleFlash>=0)    ? 1 : 0) /**< 200MS ON, 200MS OFF, 200MS ON, 200MS OFF, 200MS ON, 1000MS OFF */
    #define LED_QUADRUPLE_FLASH(NMT)(((NMT)->LEDquadrupleFlash>=0) ? 1 : 0) /**< 200MS ON, 200MS OFF, 200MS ON, 200MS OFF, 200MS ON, 200MS OFF, 200MS ON, 1000MS OFF */
    #define LED_GREEN_RUN(NMT)      (((NMT)->LEDgreenRun>=0)       ? 1 : 0) /**< CANOPEN RUN LED ACCORDING TO CIA DR 303-3 */
    #define LED_RED_ERROR(NMT)      (((NMT)->LEDredError>=0)       ? 1 : 0) /**< CANopen error LED according to CiA DR 303-3 */
/** @} */


/**
 * Internal network state of the CANopen node
 */
typedef enum{
    CO_NMT_INITIALIZING             = 0,    /**< Device is initializing */
    CO_NMT_PRE_OPERATIONAL          = 127,  /**< Device is in pre-operational state */
    CO_NMT_OPERATIONAL              = 5,    /**< Device is in operational state */
    CO_NMT_STOPPED                  = 4     /**< Device is stopped */
}CO_NMT_internalState_t;


/**
 * Commands from NMT master.
 */
typedef enum{
    CO_NMT_ENTER_OPERATIONAL        = 1,    /**< Start device */
    CO_NMT_ENTER_STOPPED            = 2,    /**< Stop device */
    CO_NMT_ENTER_PRE_OPERATIONAL    = 128,  /**< Put device into pre-operational */
    CO_NMT_RESET_NODE               = 129,  /**< Reset device */
    CO_NMT_RESET_COMMUNICATION      = 130   /**< Reset CANopen communication on device */
}CO_NMT_command_t;


/**
 * Return code for CO_NMT_process() that tells application code what to
 * reset.
 */
typedef enum{
    CO_RESET_NOT  = 0,/**< Normal return, no action */
    CO_RESET_COMM = 1,/**< Application must provide communication reset. */
    CO_RESET_APP  = 2,/**< Application must provide complete device reset */
    CO_RESET_QUIT = 3 /**< Application must quit, no reset of microcontroller (command is not requested by the stack.) */
}CO_NMT_reset_cmd_t;


/**
 * NMT consumer and Heartbeat producer object. It includes also variables for
 * @ref CO_NMT_statusLEDdiodes. Object is initialized by CO_NMT_init().
 */
typedef struct{
    uint8_t             operatingState; /**< See @ref CO_NMT_internalState_t */
    int8_t              LEDflickering;  /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDblinking;    /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDsingleFlash; /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDdoubleFlash; /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDtripleFlash; /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDquadrupleFlash; /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDgreenRun;    /**< See @ref CO_NMT_statusLEDdiodes */
    int8_t              LEDredError;    /**< See @ref CO_NMT_statusLEDdiodes */

    uint8_t             resetCommand;   /**< If different than zero, device will reset */
    uint8_t             nodeId;         /**< CANopen Node ID of this device */
    uint16_t            HBproducerTimer;/**< Internal timer for HB producer */
    uint16_t            firstHBTime;    /**< From CO_NMT_init() */
    CO_EMpr_t          *emPr;           /**< From CO_NMT_init() */
    CO_CANmodule_t     *HB_CANdev;      /**< From CO_NMT_init() */
    void              (*pFunctNMT)(CO_NMT_internalState_t state); /**< From CO_NMT_initCallback() or NULL */
    CO_CANtx_t         *HB_TXbuff;      /**< CAN transmit buffer */
}CO_NMT_t;


/**
 * Initialize NMT and Heartbeat producer object.
 *
 * Function must be called in the communication reset section.
 *
 * @param NMT This object will be initialized.
 * @param emPr Emergency main object.
 * @param nodeId CANopen Node ID of this device.
 * @param firstHBTime Time between bootup and first heartbeat message in milliseconds.
 * If firstHBTime is greater than _Producer Heartbeat time_
 * (object dictionary, index 0x1017), latter is used instead.
 * @param NMT_CANdev CAN device for NMT reception.
 * @param NMT_rxIdx Index of receive buffer in above CAN device.
 * @param CANidRxNMT CAN identifier for NMT message.
 * @param HB_CANdev CAN device for HB transmission.
 * @param HB_txIdx Index of transmit buffer in the above CAN device.
 * @param CANidTxHB CAN identifier for HB message.
 *
 * @return #CO_ReturnError_t CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_NMT_init(
        CO_NMT_t               *NMT,
        CO_EMpr_t              *emPr,
        uint8_t                 nodeId,
        uint16_t                firstHBTime,
        CO_CANmodule_t         *NMT_CANdev,
        uint16_t                NMT_rxIdx,
        uint16_t                CANidRxNMT,
        CO_CANmodule_t         *HB_CANdev,
        uint16_t                HB_txIdx,
        uint16_t                CANidTxHB);

/**
 * Initialize NMT callback function.
 *
 * Function initializes optional callback function, which is called after
 * NMT State change has occured. Function may wake up external task which
 * handles NMT events.
 * The first call is made immediately to give the consumer the current NMT state.
 *
 * @remark Be aware that the callback function is run inside the CAN receive
 * function context. Depending on the driver, this might be inside an interrupt!
 *
 * @param NMT This object.
 * @param pFunctNMT Pointer to the callback function. Not called if NULL.
 */
void CO_NMT_initCallback(
        CO_NMT_t               *NMT,
        void                  (*pFunctNMT)(CO_NMT_internalState_t state));


/**
 * Calculate blinking bytes.
 *
 * Function must be called cyclically every 50 milliseconds. See @ref CO_NMT_statusLEDdiodes.
 *
 * @param NMT NMT object.
 */
void CO_NMT_blinkingProcess50ms(CO_NMT_t *NMT);


/**
 * Process received NMT and produce Heartbeat messages.
 *
 * Function must be called cyclically.
 *
 * @param NMT This object.
 * @param timeDifference_ms Time difference from previous function call in [milliseconds].
 * @param HBtime _Producer Heartbeat time_ (object dictionary, index 0x1017).
 * @param NMTstartup _NMT startup behavior_ (object dictionary, index 0x1F80).
 * @param errorRegister _Error register_ (object dictionary, index 0x1001).
 * @param errorBehavior pointer to _Error behavior_ array (object dictionary, index 0x1029).
 *        Object controls, if device should leave NMT operational state.
 *        Length of array must be 6. If pointer is NULL, no calculation is made.
 * @param timerNext_ms Return value - info to OS - see CO_process().
 *
 * @return #CO_NMT_reset_cmd_t
 */
CO_NMT_reset_cmd_t CO_NMT_process(
        CO_NMT_t               *NMT,
        uint16_t                timeDifference_ms,
        uint16_t                HBtime,
        uint32_t                NMTstartup,
        uint8_t                 errorRegister,
        const uint8_t           errorBehavior[],
        uint16_t               *timerNext_ms);


/**
 * Query current NMT state
 *
 * @param NMT This object.
 *
 * @return #CO_NMT_internalState_t
 */
CO_NMT_internalState_t CO_NMT_getInternalState(
        CO_NMT_t               *NMT);


#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
