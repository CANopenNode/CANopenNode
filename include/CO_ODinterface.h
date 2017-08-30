/*
 * CO_ODinterface.h
 *
 *  Created on: 2 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen Object Dictionary interface
 */

#ifndef CO_ODINTERFACE_H_
#define CO_ODINTERFACE_H_

/** Preliminary class declarations *******************************************/
class cCO_OD_Interface;

/** Includes *****************************************************************/
/** Defines ******************************************************************/
/** Typedef, enumeration, class **********************************************/

/**
 * Object Dictionary attributes. Bit masks for attribute in CO_OD_entry_t.
 */
typedef enum{
    CO_ODA_MEM_ROM          = 0x0001,  /**< Variable is located in ROM memory */
    CO_ODA_MEM_RAM          = 0x0002,  /**< Variable is located in RAM memory */
    CO_ODA_MEM_EEPROM       = 0x0003,  /**< Variable is located in EEPROM memory */
    CO_ODA_READABLE         = 0x0004,  /**< SDO server may read from the variable */
    CO_ODA_WRITEABLE        = 0x0008,  /**< SDO server may write to the variable */
    CO_ODA_RPDO_MAPABLE     = 0x0010,  /**< Variable is mappable for RPDO */
    CO_ODA_TPDO_MAPABLE     = 0x0020,  /**< Variable is mappable for TPDO */
    CO_ODA_TPDO_DETECT_COS  = 0x0040,  /**< If variable is mapped to any PDO, then
                                             PDO is automatically send, if variable
                                             changes its value */
    CO_ODA_MB_VALUE         = 0x0080   /**< True when variable is a multibyte value */
} CO_SDO_OD_attributes_t;


/**
 * Common DS301 object dictionary entries.
 */
typedef enum  {
    OD_H1000_DEV_TYPE             = 0x1000,/**< Device type */
    OD_H1001_ERR_REG              = 0x1001,/**< Error register */
    OD_H1002_MANUF_STATUS_REG     = 0x1002,/**< Manufacturer status register */
    OD_H1003_PREDEF_ERR_FIELD     = 0x1003,/**< Predefined error field */
    OD_H1004_RSV                  = 0x1004,/**< Reserved */
    OD_H1005_COBID_SYNC           = 0x1005,/**< Sync message cob-id */
    OD_H1006_COMM_CYCL_PERIOD     = 0x1006,/**< Communication cycle period */
    OD_H1007_SYNC_WINDOW_LEN      = 0x1007,/**< Sync windows length */
    OD_H1008_MANUF_DEV_NAME       = 0x1008,/**< Manufacturer device name */
    OD_H1009_MANUF_HW_VERSION     = 0x1009,/**< Manufacturer hardware version */
    OD_H100A_MANUF_SW_VERSION     = 0x100A,/**< Manufacturer software version */
    OD_H100B_RSV                  = 0x100B,/**< Reserved */
    OD_H100C_GUARD_TIME           = 0x100C,/**< Guard time */
    OD_H100D_LIFETIME_FACTOR      = 0x100D,/**< Life time factor */
    OD_H100E_RSV                  = 0x100E,/**< Reserved */
    OD_H100F_RSV                  = 0x100F,/**< Reserved */
    OD_H1010_STORE_PARAM_FUNC     = 0x1010,/**< Store parameter in persistent memory function */
    OD_H1011_REST_PARAM_FUNC      = 0x1011,/**< Restore default parameter function */
    OD_H1012_COBID_TIME           = 0x1012,/**< Timestamp message cob-id */
    OD_H1013_HIGH_RES_TIMESTAMP   = 0x1013,/**< High resolution timestamp */
    OD_H1014_COBID_EMERGENCY      = 0x1014,/**< Emergency message cob-id */
    OD_H1015_INHIBIT_TIME_MSG     = 0x1015,/**< Inhibit time message */
    OD_H1016_CONSUMER_HB_TIME     = 0x1016,/**< Consumer heartbeat time */
    OD_H1017_PRODUCER_HB_TIME     = 0x1017,/**< Producer heartbeat time */
    OD_H1018_IDENTITY_OBJECT      = 0x1018,/**< Identity object */
    OD_H1019_SYNC_CNT_OVERFLOW    = 0x1019,/**< Sync counter overflow value */
    OD_H1020_VERIFY_CONFIG        = 0x1020,/**< Verify configuration */
    OD_H1021_STORE_EDS            = 0x1021,/**< Store EDS */
    OD_H1022_STORE_FORMAT         = 0x1022,/**< Store format */
    OD_H1023_OS_CMD               = 0x1023,/**< OS command */
    OD_H1024_OS_CMD_MODE          = 0x1024,/**< OS command mode */
    OD_H1025_OS_DBG_INTERFACE     = 0x1025,/**< OS debug interface */
    OD_H1026_OS_PROMPT            = 0x1026,/**< OS prompt */
    OD_H1027_MODULE_LIST          = 0x1027,/**< Module list */
    OD_H1028_EMCY_CONSUMER        = 0x1028,/**< Emergency consumer object */
    OD_H1029_ERR_BEHAVIOR         = 0x1029,/**< Error behaviour */
    OD_H1200_SDO_SERVER_PARAM     = 0x1200,/**< SDO server parameters */
    OD_H1280_SDO_CLIENT_PARAM     = 0x1280,/**< SDO client parameters */
    OD_H1400_RXPDO_1_PARAM        = 0x1400,/**< RXPDO communication parameter */
    OD_H1401_RXPDO_2_PARAM        = 0x1401,/**< RXPDO communication parameter */
    OD_H1402_RXPDO_3_PARAM        = 0x1402,/**< RXPDO communication parameter */
    OD_H1403_RXPDO_4_PARAM        = 0x1403,/**< RXPDO communication parameter */
    OD_H1600_RXPDO_1_MAPPING      = 0x1600,/**< RXPDO mapping parameters */
    OD_H1601_RXPDO_2_MAPPING      = 0x1601,/**< RXPDO mapping parameters */
    OD_H1602_RXPDO_3_MAPPING      = 0x1602,/**< RXPDO mapping parameters */
    OD_H1603_RXPDO_4_MAPPING      = 0x1603,/**< RXPDO mapping parameters */
    OD_H1800_TXPDO_1_PARAM        = 0x1800,/**< TXPDO communication parameter */
    OD_H1801_TXPDO_2_PARAM        = 0x1801,/**< TXPDO communication parameter */
    OD_H1802_TXPDO_3_PARAM        = 0x1802,/**< TXPDO communication parameter */
    OD_H1803_TXPDO_4_PARAM        = 0x1803,/**< TXPDO communication parameter */
    OD_H1A00_TXPDO_1_MAPPING      = 0x1A00,/**< TXPDO mapping parameters */
    OD_H1A01_TXPDO_2_MAPPING      = 0x1A01,/**< TXPDO mapping parameters */
    OD_H1A02_TXPDO_3_MAPPING      = 0x1A02,/**< TXPDO mapping parameters */
    OD_H1A03_TXPDO_4_MAPPING      = 0x1A03 /**< TXPDO mapping parameters */
} CO_ObjDicId_t;


/**
 * Object for one entry with specific index in @ref CO_SDO_objectDictionary.
 */
typedef struct {
    /** The index of Object from 0x1000 to 0xFFFF */
    uint16_t            index;
    /** Number of (sub-objects - 1). If Object Type is variable, then
    maxSubIndex is 0, otherwise maxSubIndex is equal or greater than 1. */
    uint8_t             maxSubIndex;
    /** If Object Type is record, attribute is set to zero. Attribute for
    each member is then set in special array with members of type
    CO_OD_entryRecord_t. If Object Type is Array, attribute is common for
    all array members. See #CO_SDO_OD_attributes_t. */
    uint16_t            attribute;
    /** If Object Type is Variable, length is the length of variable in bytes.
    If Object Type is Array, length is the length of one array member.
    If Object Type is Record, length is zero. Length for each member is
    set in special array with members of type CO_OD_entryRecord_t.
    If Object Type is Domain, length is zero. Length is specified
    by application in @ref CO_SDO_OD_function. */
    uint16_t            length;
    /** If Object Type is Variable, pData is pointer to data.
    If Object Type is Array, pData is pointer to data. Data doesn't
    include Sub-Object 0.
    If object type is Record, pData is pointer to special array
    with members of type CO_OD_entryRecord_t.
    If object type is Domain, pData is null. */
    void*               pData;
} CO_OD_entry_t;


/**
 * Object for record type entry in @ref CO_SDO_objectDictionary.
 *
 * See CO_OD_entry_t.
 */
typedef struct  {
    /** Pointer to data. If object type is Domain, pData is null */
    void*               pData;
    /** See #CO_SDO_OD_attributes_t */
    uint16_t            attribute;
    /** Length of variable in bytes. If object type is Domain, length is zero */
    uint16_t            length;
} CO_OD_entryRecord_t;


/**
 *  Object dictionary interface class
 *  Includes helper functions
 *
 */
class cCO_OD_Interface  {
public:

  /**
   *  @brief  Init function
   *  @param  pCO_OD                              Pointer to Object Dictionary
   */
  void vCO_OD_Init(CO_OD_entry_t const * const pCO_OD);

  /**
   * Find object with specific index in Object dictionary
   * Fast search in ordered Object Dictionary. If indexes are mixed, this won't work
   * If Object Dictionary has up to 2^N entries, then N is max number of loop passes
   *
   * @param index Index of the object in Object dictionary.
   * @return Sequence number of the CO_OD entry, 0xFFFF if not found.
   */
  uint16_t nCO_OD_Find(uint16_t index);

  /**
   * Get length of the given object with specific subIndex.
   *
   * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
   * @param subIndex Sub-index of the object in Object dictionary.
   *
   * @return Data length of the variable.
   */
  uint16_t nCO_OD_GetLength(uint16_t entryNo, uint8_t subIndex);

  /**
   * Get attribute of the given object with specific subIndex. See #CO_SDO_OD_attributes_t.
   *
   * If Object Type is array and subIndex is zero, function always returns
   * 'read-only' attribute.
   *
   * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
   * @param subIndex Sub-index of the object in Object dictionary.
   *
   * @return Attribute of the variable.
   */
  uint16_t nCO_OD_GetAttribute(uint16_t entryNo, uint8_t subIndex);

  /**
   * Get pointer to data of the given object with specific subIndex.
   *
   * If Object Type is array and subIndex is zero, function returns pointer to
   * object->maxSubIndex variable.
   *
   * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
   * @param subIndex Sub-index of the object in Object dictionary.
   *
   * @return Pointer to the variable in @ref CO_SDO_objectDictionary.
   */
  void* pCO_OD_GetDataPointer(uint16_t entryNo, uint8_t subIndex);

  /**
   * Returns max subindex of specific index in Object dictionary
   * @param entryNo Sequence number of OD entry as returned from CO_OD_find().
   *
   * @return maxSubIndex
   */
  uint8_t nCO_OD_GetMaxSubindex(uint16_t entryNo);

private:
  CO_OD_entry_t*  m_pCO_OD;             // pointer to Object Dictionary
};

/** Exported variables and objects *******************************************/

extern cCO_OD_Interface oCO_OD_Interface;

/** Exported function prototypes *********************************************/

#endif /* CO_ODINTERFACE_H_ */
