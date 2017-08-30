/*
 * CANopen.h
 *
 *  Created on: 29 авг. 2016 г.
 *      Author: a.smirnov
 */

#ifndef CANOPEN_H_
#define CANOPEN_H_

/** Preliminary class declarations *******************************************/
class cActiveClass_CO_CAN_NMTdepended;
class cCO_Main;

/** Includes *****************************************************************/
#include <stdint.h>         // for 'int8_t' to 'uint64_t'
#include "active_class.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#include "can.h"

/** Defines ******************************************************************/
#define   N_ELEMENTS(x)         sizeof(x)/sizeof(x[0])        // macros retuns number of elements in static array X
#define   PDO_VALID_MASK        0xBFFFF800

// SDO Client command specifier, see DS301
#define CCS_DOWNLOAD_INITIATE           1
#define CCS_DOWNLOAD_SEGMENT            0
#define CCS_UPLOAD_INITIATE             2
#define CCS_UPLOAD_SEGMENT              3
#define CCS_ABORT                       0x80

// SDO server command specifier, see DS301
#define SCS_UPLOAD_INITIATE             2
#define SCS_UPLOAD_SEGMENT              0
#define SCS_DOWNLOAD_INITIATE           3
#define SCS_DOWNLOAD_SEGMENT            1
#define SCS_ABORT                       0x80

/** Typedef, enumeration, class **********************************************/

/**
 * Internal state of the CANopen node
 */
typedef enum  {
  CO_NMT_INITIALIZING             = 0,    /**< Device is initializing */
  CO_NMT_PRE_OPERATIONAL          = 127,  /**< Device is in pre-operational state */
  CO_NMT_OPERATIONAL              = 5,    /**< Device is in operational state */
  CO_NMT_STOPPED                  = 4     /**< Device is stopped */
} CO_NMT_internalState_t;


/**
 * Commands from NMT master.
 */
typedef enum  {
  CO_NMT_ENTER_OPERATIONAL        = 1,    /**< Start device */
  CO_NMT_ENTER_STOPPED            = 2,    /**< Stop device */
  CO_NMT_ENTER_PRE_OPERATIONAL    = 128,  /**< Put device into pre-operational */
  CO_NMT_RESET_NODE               = 129,  /**< Reset device */
  CO_NMT_RESET_COMMUNICATION      = 130   /**< Reset CANopen communication on device */
} CO_NMT_command_t;


/**
 * CAN TX overflow
 * Manufacturer specific error codes
 */
enum  {
  CAN_TX_OVERFLOW_EMCYSEND        = 1,    // CAN TX overflow from EMCYSend task
  CAN_TX_OVERFLOW_HBPRODUCER      = 11,   // CAN TX overflow from HBProducer task
  CAN_TX_OVERFLOW_TPDO            = 21,   // CAN TX overflow from TPDO task
  CAN_TX_OVERFLOW_SDO             = 31,   // CAN TX overflow from SDO task
  CAN_TX_OVERFLOW_SDO_MASTER      = 41,   // CAN TX overflow from SDO master task
  CAN_TX_OVERFLOW_USER            = 51,   // CAN TX overflow from user task
};


/**
 * Internal soft error, info
 * Manufacturer specific error codes
 */
enum  {
  LED_TASK_QUEUE_OVERFLOW         = 1,    // LED task queue overflow
};


/**
 * Internal soft error, critical
 * Manufacturer specific error codes
 */
enum  {
  QUEUE_FULL_HB_CONSUMER_1          = 1,    // HB CONSUMER task queue 1 overflow
  QUEUE_FULL_HB_PRODUCER_1          = 2,    // HB PRODUCER task queue 1 overflow
  QUEUE_FULL_EMCYSEND_1             = 3,    // EMCYSend task queue 1 overflow
  QUEUE_FULL_SDO_1                  = 4,    // SDO task queue 1 overflow
  QUEUE_FULL_RPDO_1                 = 5,    // RPDO task queue 1 overflow
  QUEUE_FULL_TPDO_1                 = 6,    // TPDO task queue 1 overflow
  QUEUE_FULL_TPDO_2                 = 7,    // TPDO task queue 2 overflow
  QUEUE_FULL_USER_1                 = 11,   // user task queue 1 overflow

  QUEUE_FULL_NMTEMCY_1              = 21,   // NMT_EMCY task queue 1 overflow
  QUEUE_FULL_HB_CONSUMER_2          = 22,   // HB CONSUMER task queue 2 overflow
  QUEUE_FULL_SDO_2                  = 23,   // SDO task queue 2 overflow
  QUEUE_FULL_RPDO_2                 = 24,   // RPDO task queue 2 overflow
  QUEUE_FULL_SDO_CLIENT_1           = 25,   // SDO master task queue 1 overflow

  QUEUE_CAN_RX_OVERFLOW             = 31,   // CAN Driver RX queue overflow
};

/**
 * Default CANopen identifiers.
 *
 * Default CANopen identifiers for CANopen communication objects. Same as
 * 11-bit addresses of CAN messages. These are default identifiers and
 * can be changed in CANopen. Especially PDO identifiers are confgured
 * in PDO linking phase of the CANopen network configuration.
 */
typedef enum  {
     CO_CAN_ID_NMT_SERVICE       = 0x000,   /**< 0x000, Network management */
     CO_CAN_ID_SYNC              = 0x080,   /**< 0x080, Synchronous message */
     CO_CAN_ID_EMERGENCY         = 0x080,   /**< 0x080, Emergency messages (+nodeID) */
     CO_CAN_ID_TIME_STAMP        = 0x100,   /**< 0x100, Time stamp message */
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
     CO_CAN_ID_HEARTBEAT         = 0x700    /**< 0x700, Heartbeat message */
} CO_Default_CAN_ID_t;


/**
 * Return values of some CANopen functions. If function was executed
 * successfully it returns 0 otherwise it returns <0.
 */
typedef enum {
  CO_ERROR_NO                     = 0, /**< Operation completed successfully */
  CO_ERROR_ILLEGAL_ARGUMENT       = -1, /**< Error in function arguments */
  CO_ERROR_OUT_OF_MEMORY          = -2, /**< Memory allocation failed */
  CO_ERROR_TIMEOUT                = -3, /**< Function timeout */
  CO_ERROR_ILLEGAL_BAUDRATE       = -4, /**< Illegal baudrate passed to function CO_CANmodule_init() */
  CO_ERROR_RX_OVERFLOW            = -5, /**< Previous message was not processed yet */
  CO_ERROR_RX_PDO_OVERFLOW        = -6, /**< previous PDO was not processed yet */
  CO_ERROR_RX_MSG_LENGTH          = -7, /**< Wrong receive message length */
  CO_ERROR_RX_PDO_LENGTH          = -8, /**< Wrong receive PDO length */
  CO_ERROR_TX_OVERFLOW            = -9, /**< Previous message is still waiting, buffer full */
  CO_ERROR_TX_PDO_WINDOW          = -10, /**< Synchronous TPDO is outside window */
  CO_ERROR_TX_UNCONFIGURED        = -11, /**< Transmit buffer was not confugured properly */
  CO_ERROR_PARAMETERS             = -12, /**< Error in function function parameters */
  CO_ERROR_DATA_CORRUPT           = -13, /**< Stored data are corrupt */
  CO_ERROR_CRC                    = -14 /**< CRC does not match */
} CO_ReturnError_t;


/**
 * SDO abort codes.
 *
 * Send with Abort SDO transfer message.
 *
 * The abort codes not listed here are reserved.
 */
typedef enum {
    CO_SDO_AB_NONE                  = 0x00000000, /**< 0x00000000, No abort */
    CO_SDO_AB_TOGGLE_BIT            = 0x05030000, /**< 0x05030000, Toggle bit not altered */
    CO_SDO_AB_TIMEOUT               = 0x05040000, /**< 0x05040000, SDO protocol timed out */
    CO_SDO_AB_CMD                   = 0x05040001, /**< 0x05040001, Command specifier not valid or unknown */
    CO_SDO_AB_BLOCK_SIZE            = 0x05040002, /**< 0x05040002, Invalid block size in block mode */
    CO_SDO_AB_SEQ_NUM               = 0x05040003, /**< 0x05040003, Invalid sequence number in block mode */
    CO_SDO_AB_CRC                   = 0x05040004, /**< 0x05040004, CRC error (block mode only) */
    CO_SDO_AB_OUT_OF_MEM            = 0x05040005, /**< 0x05040005, Out of memory */
    CO_SDO_AB_UNSUPPORTED_ACCESS    = 0x06010000, /**< 0x06010000, Unsupported access to an object */
    CO_SDO_AB_WRITEONLY             = 0x06010001, /**< 0x06010001, Attempt to read a write only object */
    CO_SDO_AB_READONLY              = 0x06010002, /**< 0x06010002, Attempt to write a read only object */
    CO_SDO_AB_NOT_EXIST             = 0x06020000, /**< 0x06020000, Object does not exist */
    CO_SDO_AB_NO_MAP                = 0x06040041, /**< 0x06040041, Object cannot be mapped to the PDO */
    CO_SDO_AB_MAP_LEN               = 0x06040042, /**< 0x06040042, Number and length of object to be mapped exceeds PDO length */
    CO_SDO_AB_PRAM_INCOMPAT         = 0x06040043, /**< 0x06040043, General parameter incompatibility reasons */
    CO_SDO_AB_DEVICE_INCOMPAT       = 0x06040047, /**< 0x06040047, General internal incompatibility in device */
    CO_SDO_AB_HW                    = 0x06060000, /**< 0x06060000, Access failed due to hardware error */
    CO_SDO_AB_TYPE_MISMATCH         = 0x06070010, /**< 0x06070010, Data type does not match, length of service parameter does not match */
    CO_SDO_AB_DATA_LONG             = 0x06070012, /**< 0x06070012, Data type does not match, length of service parameter too high */
    CO_SDO_AB_DATA_SHORT            = 0x06070013, /**< 0x06070013, Data type does not match, length of service parameter too short */
    CO_SDO_AB_SUB_UNKNOWN           = 0x06090011, /**< 0x06090011, Sub index does not exist */
    CO_SDO_AB_INVALID_VALUE         = 0x06090030, /**< 0x06090030, Invalid value for parameter (download only). */
    CO_SDO_AB_VALUE_HIGH            = 0x06090031, /**< 0x06090031, Value range of parameter written too high */
    CO_SDO_AB_VALUE_LOW             = 0x06090032, /**< 0x06090032, Value range of parameter written too low */
    CO_SDO_AB_MAX_LESS_MIN          = 0x06090036, /**< 0x06090036, Maximum value is less than minimum value. */
    CO_SDO_AB_NO_RESOURCE           = 0x060A0023, /**< 0x060A0023, Resource not available: SDO connection */
    CO_SDO_AB_GENERAL               = 0x08000000, /**< 0x08000000, General error */
    CO_SDO_AB_DATA_TRANSF           = 0x08000020, /**< 0x08000020, Data cannot be transferred or stored to application */
    CO_SDO_AB_DATA_LOC_CTRL         = 0x08000021, /**< 0x08000021, Data cannot be transferred or stored to application because of local control */
    CO_SDO_AB_DATA_DEV_STATE        = 0x08000022, /**< 0x08000022, Data cannot be transferred or stored to application because of present device state */
    CO_SDO_AB_DATA_OD               = 0x08000023, /**< 0x08000023, Object dictionary not present or dynamic generation fails */
    CO_SDO_AB_NO_DATA               = 0x08000024  /**< 0x08000024, No data available */
} CO_SDO_abortCode_t;

/**
 * Object contains all information about the object being transferred by SDO server.
 *
 * Object is used as an argument to @ref CO_SDO_OD_function. It is also
 * part of the CO_SDO_t object.
 */
typedef struct {
    /** Informative parameter. It may point to object, which is connected
    with this OD entry. It can be used inside @ref CO_SDO_OD_function, ONLY
    if it was registered by CO_OD_configure() function before. */
    void*               object;
    /** SDO data buffer contains data, which are exchanged in SDO transfer.
    @ref CO_SDO_OD_function may verify or manipulate that data before (after)
    they are written to (read from) Object dictionary. Data have the same
    endianes as processor. Pointer must NOT be changed. (Data up to length
    can be changed.) */
    uint8_t*            data;
    /** Pointer to location in object dictionary, where data are stored.
    (informative reference to old data, read only). Data have the same
    endianes as processor. If data type is Domain, this variable is null. */
    const void*         ODdataStorage;
    /** Length of data in the above buffer. Read only, except for domain. If
    data type is domain see @ref CO_SDO_OD_function for special rules by upload. */
    uint16_t            dataLength;
    /** Attribute of object in Object dictionary (informative, must NOT be changed). */
    uint16_t            attribute;
    /** Pointer to the #CO_SDO_OD_flags_t byte. */
    //uint8_t            *pFlags;
    /** Index of object in Object dictionary (informative, must NOT be changed). */
    uint16_t            index;
    /** Subindex of object in Object dictionary (informative, must NOT be changed). */
    uint8_t             subIndex;
    /** True, if SDO upload is in progress, false if SDO download is in progress. */
    bool                reading;
    /** Used by domain data type. Indicates the first segment. Variable is informative. */
    bool                firstSegment;
    /** Used by domain data type. If false by download, then application will
    receive more segments during SDO communication cycle. If uploading,
    application may set variable to false, so SDO server will call
    @ref CO_SDO_OD_function again for filling the next data. */
    bool                lastSegment;
    /** Used by domain data type. By upload @ref CO_SDO_OD_function may write total
    data length, so this information will be send in SDO upload initiate phase. It
    is not necessary to specify this variable. By download this variable contains
    total data size, if size is indicated in SDO download initiate phase */
    uint32_t            dataLengthTotal;
    /** Used by domain data type. In case of multiple segments, this indicates the offset
    into the buffer this segment starts at. */
    uint32_t            offset;
} CO_ODF_arg_t;


/**
 * Helper union for manipulating data bytes.
 */
typedef union {
    uint8_t  u8[8];  /**< 8 bytes */
    uint16_t u16[4]; /**< 4 words */
    uint32_t u32[2]; /**< 2 double words */
} CO_bytes_t;


/**
 *    Class, depending of NMT state and receiving CAN msgs
 *    To use when constructing other CANOpen active classes
 */
class cActiveClass_CO_CAN_NMTdepended : public cActiveClass  {
public:

  /**
   *  @brief  Method signals that appropriate CAN message been received
   *    Tries to place a message to CAN receive queue without timeout
   *  @param    pCO_CanMsg                     Pointer to message been received
   *  @return   true if success, false if queue is full
   */
  bool bSignalCANReceived(const sCANMsg* pCO_CanMsg);

  /**
   *  @brief  Method signals to task that NMT state was changed
   *    Tries to place a message to CANOpen state change queue without timeout
   *  @param    nNewState                     new state
   *  @return   true if success, false if queue is full
   */
  bool bSignalCOStateChanged(const CO_NMT_internalState_t nNewState);

protected:

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInitPartial(void);

  SemaphoreHandle_t   m_xBinarySemaphore;             // binary semaphore handle
  QueueHandle_t       m_xQueueHandle_CANReceive;      // CAN receive queue handle
  QueueHandle_t       m_xQueueHandle_NMTStateChange;  // CANOpen state change queue handle

};//cActiveClass_NMTdepend ----------------------------------------------------


/**
 *    Main CANOpen class-coordinator
 */
class cCO_Main : public cActiveClass  {
public:

  /**
   *  @brief  Init function
   *  Creates and inits all internal OS objects and tasks
   *
   *  @param    none
   *  @return   none
   */
  void vInit(void);

  /**
   *  @brief  Suspend function
   *  Suspends all internal tasks and objects
   *
   *  @param    none
   *  @return   none
   */
  void vSuspend(void);

  /**
   *  @brief  Resume function
   *  Resumes all internal tasks and objects
   *
   *  @param    none
   *  @return   none
   */
  void vResume(void);

};//cCO_Main ------------------------------------------------------------------

/** Exported variables and objects *******************************************/

extern cCO_Main oCO_Main;

/** Exported function prototypes *********************************************/

/**
 * Helper function returns uint32 from byte array.
 *
 * @param data Location of source data.
 * @return Variable of type uint32_t.
 */
uint32_t CO_getUint32(const uint8_t data[]);

/**
 * Helper function writes uint32 to byte array.
 *
 * @param data Location of destination data.
 * @param value Variable of type uint32_t to be written into data.
 */
void CO_setUint32(uint8_t data[], const uint32_t value);

#endif /* CANOPEN_H_ */
