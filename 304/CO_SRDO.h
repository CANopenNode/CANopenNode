/**
 * \file CO_SRDO.h
 *
 * \brief SRDO implementation.
 *
 */
#ifndef CO_SRDO_H
#define CO_SRDO_H


/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "CO_driver.h"
#include "CO_Emergency.h"
#include "CO_NMT_Heartbeat.h"

/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
/** default configuration */
#ifndef CO_CONFIG_SRDO
#define CO_CONFIG_SRDO (0)
#endif

/** minimum delay between normal and inverted SRDO messages */
#ifndef CO_CONFIG_SRDO_MINIMUM_DELAY
#define CO_CONFIG_SRDO_MINIMUM_DELAY 0
#endif

/**
 * Maximum number of entries, which can be mapped to SRDO, 16 for standard CAN,
 * may be less to preserve RAM usage. Note that all mapped objects need to be
 * specified twice, once for normal and once for inverted SRDO.
 */
#ifndef CO_SRDO_MAX_MAPPED_ENTRIES
#define CO_SRDO_MAX_MAPPED_ENTRIES (16U)
#endif

#if ((CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Macro used to replace external (object dictionary) CRC validation.
 */
#define CO_SRDO_CRC_VALID_CHECH    (0xA5)

/**
 * \brief SRDO communication parameter. The same as record from Object dictionary
 * 		  (index 0x1301-0x1380).
 */
typedef struct
{
    uint8_t   maxSubIndex; //fixed at seven.
    uint8_t   informationDirection; //0 = invalid; 1 = TX; 2 = RX;
    uint16_t  refreshTimeOrSCT; //TX = transmission type; RX = receive timeout between two SRDO;
    uint8_t   SRVT; //TX = unused; RX = receive timeout between first and second SRDO message;
    uint8_t   transmissionType; //transmission type.
    uint32_t  COB_ID_1; //normal message COB-ID.
    uint32_t  COB_ID_2; //inverted message COB-ID.
    uint8_t	  channel; //CAN channel.
} CO_SRDOCommPar_t;

/**
 * \brief SRDO mapping parameter. The same as record from Object dictionary
 * 		  (index 0x1381-0x13FF).
 */
typedef struct
{
    uint8_t   numberOfMappedObjects; //number of objects mapped.
    uint32_t  mappedObjects[16];	 //maps pointing to object dictionary entries.
} CO_SRDOMapPar_t;

/**
 * \brief Guard Object for SRDO monitors:
 * 			- access to CRC objects
 * 			- access configuration valid flag
 * 			- change in operation state
 */
typedef struct
{
    CO_NMT_internalState_t *operatingState;     //Pointer to current operation state.
    CO_NMT_internalState_t  operatingStatePrev; //Last operation state.
    uint8_t                 configurationValid; //Pointer to the configuration valid flag in OD.
    uint8_t                 checkCRC;           //Specifies whether a CRC check should be performed.
} CO_SRDOGuard_t;

/**
 * \brief Overall SRDO object containing all needed information.
 */
typedef struct
{
    CO_EM_t          *em;                  //Emergency module.
    OD_t 			 *OD; 				   //Object dictionary pointer.
    CO_SRDOGuard_t   *SRDOGuard;           //SRDO guard module, used to monitor SRDO validity.
    uint8_t          *mapPointer[2][8];	   //Array of pointers to normal[0] and inverted[1] objects in OD.
    uint8_t           dataLength;		   //Length of mapped data (same for normal and inverted).
    uint8_t           nodeId;              //NodeId.
    uint16_t          defaultCOB_ID[2];    //Default COB-ID for normal and inverted messages.
    uint8_t           valid;			   //Validity flag.
    CO_SRDOCommPar_t  SRDOCommPar;         //Communication record.
    CO_SRDOMapPar_t   SRDOMapPar;          //Mapping record.
    uint16_t          checksum;            //Checksum.
    CO_CANmodule_t   *CANdevRx;            //Receive device.
    CO_CANmodule_t   *CANdevTx;            //Transmit device.
    CO_CANtx_t       *CANtxBuff[2];        //Normal[0] and inverted[1] message buffers.
    uint16_t          CANdevRxIdx[2];      //Normal[0] and inverted[1] RX buffers indexes.
    uint16_t          CANdevTxIdx[2];      //Normal[0] and inverted[1] TX buffers indexes.
    uint8_t           toogle;              //Toggle used to define the current state.
    uint32_t          timer;               //Transmit timer and receive timeout.
    volatile void    *CANrxNew[2];		   //New SRDO message received from CAN bus.
    uint8_t           CANrxData[2][8];	   //2*8 data bytes of the received message */
    void            (*pFunctSignalSafe)(void *object); //SafeState function pointer.
    void             *functSignalObjectSafe; //Function pointer used to mark safe objects.
#if ((CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE)
    void            (*pFunctSignalPre)(void *object); //Function pointer for optional callback.
    void             *functSignalObjectPre; //Function pointer for optional callback.
#endif
} CO_SRDO_t;


/***********************************************************************************************************************
 * Functions
 **********************************************************************************************************************/
/**
 * \brief Initialize SRDOGuard object.Function must be called in the
 * 		  communication reset section.
 *
 * \param SRDOGuard This object will be initialized.
 * \param OD Object dictionary.
 * \param operatingState Pointer to variable indicating CANopen device NMT internal state.
 * \param configurationValid Pointer to variable with the SR valid flag
 * \param idx_SRDOvalid Index in Object Dictionary
 * \param idx_SRDOcrc Index in Object Dictionary
 *
 * \return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SRDOGuard_init(CO_SRDOGuard_t *SRDOGuard,
								   OD_t *OD,
								   CO_NMT_internalState_t *operatingState,
								   uint8_t configurationValid,
								   uint16_t idx_SRDOvalid,
								   uint16_t idx_SRDOcrc);

/**
 * \brief Process operation and valid state changes. NOTE: since SRDO cannot be
 * 		  modified at runtime, this function can be moved from the loop into
 * 		  the initialization, so that it is called only once.
 *
 * \param SRDOGuard This object.
 * \return uint8_t command for CO_SRDO_process().
 * - bit 0 entered operational
 * - bit 1 validate checksum
 */
uint8_t CO_SRDOGuard_process(CO_SRDOGuard_t *SRDOGuard);

/**
 * \brief Initialize SRDO object. Function must be called in the communication reset section.
 *
 * \param SRDO This object will be initialized.
 * \param SRDOGuard SRDOGuard object.
 * \param em Emergency object.
 * \param OD Object dictionary.
 * \param nodeId CANopen Node ID of this device. If default COB_ID is used, value will be added.
 * \param defaultCOB_ID Default COB ID for this SRDO (without NodeId).
 * \param SRDOCommPar Pointer to _SRDO communication parameter_ record from Object
 * 		  dictionary (index 0x1301+).
 * \param SRDOMapPar Pointer to _SRDO mapping parameter_ record from Object
 * 		   dictionary (index 0x1381+).
 * \param checksum
 * \param idx_SRDOCommPar Index in Object Dictionary
 * \param idx_SRDOMapPar Index in Object Dictionary
 * \param CANdevRx CAN device used for SRDO reception.
 * \param CANdevRxIdxNormal Index of receive buffer in the above CAN device.
 * \param CANdevRxIdxInverted Index of receive buffer in the above CAN device.
 * \param CANdevTx CAN device used for SRDO transmission.
 * \param CANdevTxIdxNormal Index of transmit buffer in the above CAN device.
 * \param CANdevTxIdxInverted Index of transmit buffer in the above CAN device.
 *
 * \return #CO_ReturnError_t: CO_ERROR_NO or CO_ERROR_ILLEGAL_ARGUMENT.
 */
CO_ReturnError_t CO_SRDO_init(CO_SRDO_t *SRDO,
							  CO_SRDOGuard_t *SRDOGuard,
							  CO_EM_t *em,
							  OD_t *OD,
							  uint8_t nodeId,
							  uint16_t defaultCOB_ID,
							  OD_entry_t *SRDOCommPar,
							  OD_entry_t *SRDOMapPar,
							  uint16_t checksum,
							  uint16_t idx_SRDOCommPar,
							  uint16_t  idx_SRDOMapPar,
							  CO_CANmodule_t *CANdevRx,
							  uint16_t CANdevRxIdxNormal,
							  uint16_t CANdevRxIdxInverted,
							  CO_CANmodule_t *CANdevTx,
							  uint16_t CANdevTxIdxNormal,
							  uint16_t CANdevTxIdxInverted);

#if ((CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE)
/**
 * \brief Initialize SRDO callback function. Function initializes optional callback function,
 * 		  which should immediately start processing of CO_SRDO_process() function.
 * 		  Callback is called after SRDO message is received from the CAN bus.
 *
 * \param SRDO This object.
 * \param object Pointer to object, which will be passed to pFunctSignalPre(). Can be NULL
 * \param pFunctSignalPre Pointer to the callback function. Not called if NULL.
 */
void CO_SRDO_initCallbackPre(CO_SRDO_t *SRDO,
							 void *object,
							 void (*pFunctSignalPre)(void *object));
#endif

/**
 * \brief Initialize SRDO callback function. Function initializes optional callback function,
 *	      that is called when SRDO enters a safe state.This happens when a timeout is reached
 *	      or the data is inconsistent. The safe state itself is not further defined.
 *	      One measure, for example, would be to go back to the pre-operational state Callback
 *	      is called from CO_SRDO_process().
 *
 * \param SRDO This object.
 * \param object Pointer to object, which will be passed to pFunctSignalSafe(). Can be NULL
 * \param pFunctSignalSafe Pointer to the callback function. Not called if NULL.
 */
void CO_SRDO_initCallbackEnterSafeState(CO_SRDO_t *SRDO,
										void *object,
										void (*pFunctSignalSafe)(void *object));

/**
 * \brief Process transmitting/receiving SRDO messages. This function verifies the checksum on demand.
 *  	  This function also configures the SRDO on operation state change to operational
 *
 * \param SRDO This object.
 * \param commands result from CO_SRDOGuard_process().
 * \param timeDifference_us Time difference from previous function call in [microseconds].
 * \param [out] timerNext_us info to OS.
 */
void CO_SRDO_process(CO_SRDO_t *SRDO,
					 uint8_t commands,
					 uint32_t timeDifference_us,
					 uint32_t *timerNext_us);


#ifdef __cplusplus
}
#endif

#endif

#endif /* CO_SRDO_H */
