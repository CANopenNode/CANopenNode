/**
 * \file CANopen.h
 *
 * \brief CANopen main module implementation.
 */

#ifndef CANopen_H
#define CANopen_H

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "CO_driver.h"
#include "CO_ODinterface.h"
#include "CO_NMT_Heartbeat.h"
#include "CO_HBconsumer.h"
#include "CO_Emergency.h"
#include "CO_SDOserver.h"
#include "CO_SDOclient.h"
#include "CO_SYNC.h"
#include "CO_PDO.h"
#include "CO_TIME.h"
#include "CO_LEDs.h"
#include "CO_GFC.h"
#include "CO_SRDO.h"
#include "CO_LSSslave.h"
#include "CO_LSSmaster.h"
#include "CO_gateway_ascii.h"
#include "CO_trace.h"

#ifdef __cplusplus
extern "C" {
#endif


/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
/**
 * Redefine void type to CO_config_t.
 */
typedef void CO_config_t;

/**
 * CANopen object, collection of all CANopenNode objects.
 */
typedef struct
{
    bool_t nodeIdUnconfigured; //True in un-configured LSS slave
#if defined CO_MULTIPLE_OD
    CO_config_t *config; //Remember the configuration parameters
#endif
    CO_CANmodule_t *CANmodule; //CAN module object
    CO_CANrx_t *CANrx; //CAN receive message objects
    CO_CANtx_t *CANtx; //CAN transmit message objects
#if defined CO_MULTIPLE_OD
    uint16_t CNT_ALL_RX_MSGS; //Number of all CAN receive message objects
    uint16_t CNT_ALL_TX_MSGS; //Number of all CAN transmit message objects
#endif
    CO_NMT_t *NMT; //NMT and heartbeat object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_NMT_SLV; //Start index in CANrx
    uint16_t TX_IDX_NMT_MST; //Start index in CANtx
    uint16_t TX_IDX_HB_PROD; //Start index in CANtx
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE)
    CO_HBconsumer_t *HBcons; //Heartbeat consumer object
    CO_HBconsNode_t *HBconsMonitoredNodes; //Object for monitored nodes
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_HB_CONS; //Start index in CANrx
#endif
#endif
    CO_EM_t *em; //Emergency object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_EM_CONS; //Start index in CANrx
    uint16_t TX_IDX_EM_PROD; //Start index in CANtx
#endif
#if ((CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY))
    /** FIFO for emergency object, initialized by CO_EM_init() */
    CO_EM_fifo_t *em_fifo;
#endif
    CO_SDOserver_t *SDOserver; //SDO server objects
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_SDO_SRV; //Start index in CANrx
    uint16_t TX_IDX_SDO_SRV; //Start index in CANtx
#endif
#if ((CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE)

    CO_SDOclient_t *SDOclient; //SDO client objects
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_SDO_CLI; //Start index in CANrx
    uint16_t TX_IDX_SDO_CLI; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE)
    CO_TIME_t *TIME; //TIME object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_TIME; //Start index in CANrx
    uint16_t TX_IDX_TIME; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE)
    CO_SYNC_t *SYNC; //SYNC object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_SYNC; //Start index in CANrx
    uint16_t TX_IDX_SYNC; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE)
    CO_RPDO_t *RPDO; //RPDO objects
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_RPDO; //Start index in CANrx
#endif
#endif
#if ((CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE)
    CO_TPDO_t *TPDO; //TPDO objects
#if defined CO_MULTIPLE_OD
    uint16_t TX_IDX_TPDO; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE)
    CO_LEDs_t *LEDs; //LEDs object
#endif
#if ((CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE)
    CO_GFC_t *GFC; //GFC object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_GFC; //Start index in CANrx
    uint16_t TX_IDX_GFC; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE)
    CO_SRDOGuard_t *SRDOGuard; //SRDO guard object
    CO_SRDO_t *SRDO; //SRDO object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_SRDO; //Start index in CANrx
    uint16_t TX_IDX_SRDO; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE)
    CO_LSSslave_t *LSSslave; //LSS slave object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_LSS_SLV; //Start index in CANrx
    uint16_t TX_IDX_LSS_SLV; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER)
    CO_LSSmaster_t *LSSmaster; //LSS master object
#if defined CO_MULTIPLE_OD
    uint16_t RX_IDX_LSS_MST; //Start index in CANrx
    uint16_t TX_IDX_LSS_MST; //Start index in CANtx
#endif
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII)
    CO_GTWA_t *gtwa; //Gateway-ascii object
#if defined CO_MULTIPLE_OD
#endif
#endif
#if ((CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE)
    CO_trace_t *trace; //Trace object
#endif
} CO_t;


/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/**
 * \brief Create new CANopen object. If CO_USE_GLOBALS is defined, then function uses global static variables for
 * 		  all the CANopenNode objects. Otherwise it allocates all objects from heap.
 *
 *
 * \param config Configuration structure, used if @ref CO_MULTIPLE_OD is defined.
 * 				 It must stay in memory permanently. If CO_MULTIPLE_OD is not
 * 				 defined, config should be NULL and parameters are retrieved from
 * 				 default "OD.h" file.
 * \param [out] heapMemoryUsed Information about heap memory used. Ignored if NULL.
 *
 * \return Successfully allocated and configured CO_t object or NULL.
 */
CO_t *CO_new(CO_config_t *config, uint32_t *heapMemoryUsed);

/**
 * \brief Delete CANopen object and free memory. Must be called at program exit.
 *
 * \param co CANopen object.
 */
void CO_delete(CO_t *co);

/**
 * \brief Test if LSS slave is enabled
 *
 * \param co CANopen object.
 *
 * \return True if enabled
 */
bool_t CO_isLSSslaveEnabled(CO_t *co);

/**
 * \brief Initialize CAN driver.Function must be called in the communication reset section.
 *
 * \param co CANopen object.
 * \param hw_cfg Pointer to the user-defined CAN base structure, passed to
 *               CO_CANmodule_init().
 *
 * \return CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_CANinit(CO_t *co, CO_hw_cfg *hw_cfg);

#if ((CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE)
/**
 * \brief Initialize CANopen LSS slave. Function must be called before CO_CANopenInit.
 *
 * \param co CANopen object.
 * \param lssAddress LSS slave address, from OD object 0x1018
 * \param [in,out] pendingNodeID Pending node ID or 0xFF (unconfigured)
 * \param [in,out] pendingBitRate Pending bit rate of the CAN interface
 *
 * \return CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_LSSinit(CO_t *co,
                            CO_LSS_address_t *lssAddress,
                            uint8_t *pendingNodeID,
                            uint16_t *pendingBitRate);
#endif

/**
 * \brief Initialize CANopenNode except PDO objects. Function must be called in
 * 		  the communication reset section.
 *
 * \param co CANopen object.
 * \param em Emergency object, which is used inside different CANopen objects,
 * 			 usually for error reporting. If NULL, then 'co->em' will be used.
 * 			 if NULL and 'co->CNT_EM' is 0, then function returns with error.
 * \param NMT If 'co->CNT_NMT' is 0, this object must be specified, If
 * 			 'co->CNT_NMT' is 1,then it is ignored and can be NULL. NMT object
 * 			 is used for retrieving NMT internal state inside CO_process().
 * \param od CANopen Object dictionary
 * \param OD_statusBits Argument passed to @ref CO_EM_init(). May be NULL.
 * \param NMTcontrol Argument passed to @ref CO_NMT_init().
 * \param firstHBTime_ms Argument passed to @ref CO_NMT_init().
 * \param SDOserverTimeoutTime_ms Argument passed to @ref CO_SDOserver_init().
 * \param SDOclientTimeoutTime_ms Default timeout in milliseconds for SDO
 * 		  client, 500 typically. SDO client is configured from CO_GTWA_init().
 * \param SDOclientBlockTransfer If true, block transfer will be set in SDO
 * 		  						 client by default. SDO client is configured
 * 		  						 from by CO_GTWA_init().
 * \param nodeId CANopen Node ID (1 ... 127) or 0xFF(unconfigured). In the CANopen
 * 				 initialization it is the same as pendingBitRate from CO_LSSinit().
 * 				 If it is unconfigured, then some CANopen objects will not be
 * 				 initialized nor processed.
 * \param [out] errInfo Additional information in case of error, may be NULL.
 * 						errInfo can also be set in noncritical errors, where
 * 						function returns CO_ERROR_NO.
 *
 * \return CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_CANopenInit(CO_t *co,
                                CO_NMT_t *NMT,
                                CO_EM_t *em,
                                OD_t *od,
                                OD_entry_t *OD_statusBits,
                                CO_NMT_control_t NMTcontrol,
                                uint16_t firstHBTime_ms,
                                uint16_t SDOserverTimeoutTime_ms,
                                uint16_t SDOclientTimeoutTime_ms,
                                bool_t SDOclientBlockTransfer,
                                uint8_t nodeId,
                                uint32_t *errInfo);

/**
 * \brief Initialize CANopenNode PDO objects. Function must be called in the
 * 		  end of communication reset section after all CANopen and application
 * 		  initialization, otherwise some OD variables wont be mapped into PDO
 * 		  correctly.
 *
 * \param co CANopen object.
 * \param em Emergency object, which is used inside PDO objects for error
 * 			 reporting.
 * \param od CANopen Object dictionary
 * \param nodeId CANopen Node ID (1 ... 127) or 0xFF(unconfigured).If unconfigured,
 * 				 then PDO will not be initialized nor processed.
 * \param [out] errInfo Additional information in case of error, may be NULL.
 *
 * \return CO_ERROR_NO in case of success.
 */
CO_ReturnError_t CO_CANopenInitPDO(CO_t *co,
                                   CO_EM_t *em,
                                   OD_t *od,
                                   uint8_t nodeId,
                                   uint32_t *errInfo);

/**
 * \brief Process CANopen objects. Function must be called cyclically. It processes
 * 		  all "asynchronous" CANopen objects.
 *
 * \param co CANopen object.
 * \param enableGateway If true, gateway to external world will be enabled.
 * \param timeDifference_us Time difference from previous function call in
 *                          microseconds.
 * \param [out] timerNext_us info to OS - maximum delay time after this function
 *        should be called next time in [microseconds]. Value can be used for OS
 *        sleep time. Initial value must be set to maximum interval time.
 *        Output will be equal or lower to initial value. Calculation is based
 *        on various timers which expire in known time. Parameter should be
 *        used in combination with callbacks configured with
 *        CO_***_initCallbackPre() functions. Those callbacks should also
 *        trigger calling of CO_process() function. Parameter is ignored if
 *        NULL. See also @ref CO_CONFIG_FLAG_CALLBACK_PRE configuration macro.
 *
 * \return Node or communication reset request, from @ref CO_NMT_process().
 */
CO_NMT_reset_cmd_t CO_process(CO_t *co,
                              bool_t enableGateway,
                              uint32_t timeDifference_us,
                              uint32_t *timerNext_us);


#if ((CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE)
/**
 * \brief Process CANopen SYNC objects. Function must be called cyclically.
 * 		  For time critical applications it may be called from real time thread
 * 		  with constant interval (1ms typically). It processes SYNC CANopen objects.
 *
 * \param co CANopen object.
 * \param timeDifference_us Time difference from previous function call in microseconds.
 * \param [out] timerNext_us info to OS - see CO_process().
 *
 * \return True, if CANopen SYNC message was just received or transmitted.
 */
bool_t CO_process_SYNC(CO_t *co,
                       uint32_t timeDifference_us,
                       uint32_t *timerNext_us);
#endif

#if ((CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE)
/**
 * \brief Process CANopen RPDO objects. Function must be called cyclically.
 * 		  For time critical applications it may be called from real time
 * 		  thread with constant interval (1ms typically). It processes receive
 * 		  PDO CANopen objects.
 *
 * \param co CANopen object.
 * \param syncWas True, if CANopen SYNC message was just received or transmitted.
 * \param timeDifference_us Time difference from previous function call in microseconds.
 * \param [out] timerNext_us info to OS - see CO_process().
 */
void CO_process_RPDO(CO_t *co,
                     bool_t syncWas,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us);
#endif

#if ((CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE)
/**
 * \brief Process CANopen TPDO objects. Function must be called cyclically. For time
 * 		  critical applications it may be called from real time thread with constant
 * 		  interval (1ms typically). It processes transmit PDO CANopen objects.
 *
 * \param co CANopen object.
 * \param syncWas True, if CANopen SYNC message was just received or transmitted.
 * \param timeDifference_us Time difference from previous function call in microseconds.
 * \param [out] timerNext_us info to OS - see CO_process().
 */
void CO_process_TPDO(CO_t *co,
                     bool_t syncWas,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us);
#endif

#if ((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE)
/**
 * \brief Process CANopen SRDO objects. Function must be called cyclically. For time
 * 		  critical applications it may be called from real time thread with constant
 * 		  interval (1ms typically). It processes SRDO CANopen objects.
 *
 * \param co CANopen object.
 * \param timeDifference_us Time difference from previous function call in microseconds.
 * \param [out] timerNext_us info to OS - see CO_process().
 */
void CO_process_SRDO(CO_t *co,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us);
#endif

/** @} */ /* CO_CANopen */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CANopen_H */
