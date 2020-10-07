/**
 * CANopen Object Dictionary interface
 *
 * @file        CO_ODinterface.h
 * @ingroup     CO_ODinterface
 * @author      Janez Paternoster
 * @copyright   2020 Janez Paternoster
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

#ifndef CO_OD_INTERFACE_H
#define CO_OD_INTERFACE_H

#include "301/CO_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_ODinterface OD interface
 * @ingroup CO_CANopen_301
 * @{
 * See @ref doc/objectDictionary.md
 */

#ifndef OD_size_t
/** Variable of type OD_size_t contains data length in bytes of OD variable */
#define OD_size_t uint32_t
/** Type of flagsPDO variable from OD_subEntry_t */
#define OD_flagsPDO_t uint32_t
#endif

/** Size of Object Dictionary attribute */
#define OD_attr_t uint8_t


/**
 * Common DS301 object dictionary entries.
 */
typedef enum {
    OD_H1000_DEV_TYPE           = 0x1000U,/**< Device type */
    OD_H1001_ERR_REG            = 0x1001U,/**< Error register */
    OD_H1002_MANUF_STATUS_REG   = 0x1002U,/**< Manufacturer status register */
    OD_H1003_PREDEF_ERR_FIELD   = 0x1003U,/**< Predefined error field */
    OD_H1004_RSV                = 0x1004U,/**< Reserved */
    OD_H1005_COBID_SYNC         = 0x1005U,/**< Sync message cob-id */
    OD_H1006_COMM_CYCL_PERIOD   = 0x1006U,/**< Communication cycle period */
    OD_H1007_SYNC_WINDOW_LEN    = 0x1007U,/**< Sync windows length */
    OD_H1008_MANUF_DEV_NAME     = 0x1008U,/**< Manufacturer device name */
    OD_H1009_MANUF_HW_VERSION   = 0x1009U,/**< Manufacturer hardware version */
    OD_H100A_MANUF_SW_VERSION   = 0x100AU,/**< Manufacturer software version */
    OD_H100B_RSV                = 0x100BU,/**< Reserved */
    OD_H100C_GUARD_TIME         = 0x100CU,/**< Guard time */
    OD_H100D_LIFETIME_FACTOR    = 0x100DU,/**< Life time factor */
    OD_H100E_RSV                = 0x100EU,/**< Reserved */
    OD_H100F_RSV                = 0x100FU,/**< Reserved */
    OD_H1010_STORE_PARAM_FUNC   = 0x1010U,/**< Store params in persistent mem.*/
    OD_H1011_REST_PARAM_FUNC    = 0x1011U,/**< Restore default parameters */
    OD_H1012_COBID_TIME         = 0x1012U,/**< Timestamp message cob-id */
    OD_H1013_HIGH_RES_TIMESTAMP = 0x1013U,/**< High resolution timestamp */
    OD_H1014_COBID_EMERGENCY    = 0x1014U,/**< Emergency message cob-id */
    OD_H1015_INHIBIT_TIME_EMCY  = 0x1015U,/**< Inhibit time emergency message */
    OD_H1016_CONSUMER_HB_TIME   = 0x1016U,/**< Consumer heartbeat time */
    OD_H1017_PRODUCER_HB_TIME   = 0x1017U,/**< Producer heartbeat time */
    OD_H1018_IDENTITY_OBJECT    = 0x1018U,/**< Identity object */
    OD_H1019_SYNC_CNT_OVERFLOW  = 0x1019U,/**< Sync counter overflow value */
    OD_H1020_VERIFY_CONFIG      = 0x1020U,/**< Verify configuration */
    OD_H1021_STORE_EDS          = 0x1021U,/**< Store EDS */
    OD_H1022_STORE_FORMAT       = 0x1022U,/**< Store format */
    OD_H1023_OS_CMD             = 0x1023U,/**< OS command */
    OD_H1024_OS_CMD_MODE        = 0x1024U,/**< OS command mode */
    OD_H1025_OS_DBG_INTERFACE   = 0x1025U,/**< OS debug interface */
    OD_H1026_OS_PROMPT          = 0x1026U,/**< OS prompt */
    OD_H1027_MODULE_LIST        = 0x1027U,/**< Module list */
    OD_H1028_EMCY_CONSUMER      = 0x1028U,/**< Emergency consumer object */
    OD_H1029_ERR_BEHAVIOR       = 0x1029U,/**< Error behaviour */
    OD_H1200_SDO_SERVER_1_PARAM = 0x1200U,/**< SDO server parameter */
    OD_H1280_SDO_CLIENT_1_PARAM = 0x1280U,/**< SDO client parameter */
    OD_H1300_GFC_PARAM          = 0x1300U,/**< Global fail-safe command param */
    OD_H1301_SRDO_1_PARAM       = 0x1301U,/**< SRDO communication parameter */
    OD_H1381_SRDO_1_MAPPING     = 0x1381U,/**< SRDO mapping parameter */
    OD_H13FE_SRDO_VALID         = 0x13FEU,/**< SRDO Configuration valid */
    OD_H13FF_SRDO_CHECKSUM      = 0x13FFU,/**< SRDO configuration checksum */
    OD_H1400_RXPDO_1_PARAM      = 0x1400U,/**< RXPDO communication parameter */
    OD_H1600_RXPDO_1_MAPPING    = 0x1600U,/**< RXPDO mapping parameters */
    OD_H1800_TXPDO_1_PARAM      = 0x1800U,/**< TXPDO communication parameter */
    OD_H1A00_TXPDO_1_MAPPING    = 0x1A00U,/**< TXPDO mapping parameters */
} OD_ObjDicId_30x_t;


/**
 * Attributes (bit masks) for OD sub-object.
 */
typedef enum {
    ODA_SDO_R = 0x01, /**< SDO server may read from the variable */
    ODA_SDO_W = 0x02, /**< SDO server may write to the variable */
    ODA_SDO_RW = 0x03, /**< SDO server may read from or write to the variable */
    ODA_TPDO = 0x04, /**< Variable is mappable into TPDO (can be read) */
    ODA_RPDO = 0x08, /**< Variable is mappable into RPDO (can be written) */
    ODA_TRPDO = 0x0C, /**< Variable is mappable into TPDO or RPDO */
    ODA_TSRDO = 0x10, /**< Variable is mappable into transmitting SRDO */
    ODA_RSRDO = 0x20, /**< Variable is mappable into receiving SRDO */
    ODA_TRSRDO = 0x30, /**< Variable is mappable into tx or rx SRDO */
    ODA_MB = 0x40, /**< Variable is multi-byte ((u)int16_t to (u)int64_t) */
    ODA_RESERVED = 0x80, /**< Reserved for further use */
} OD_attributes_t;


/**
 * Return codes from OD access functions.
 *
 * @ref OD_getSDOabCode() can be used to retrive corresponding SDO abort code.
 */
typedef enum {
/* !!!! WARNING !!!!
 * If changing these values, change also OD_getSDOabCode() function!
 */
    /** Read/write is only partial, make more calls */
    ODR_PARTIAL = -1,
    /** SDO abort 0x00000000 - Read/write successfully finished */
    ODR_OK = 0,
    /** SDO abort 0x05040005 - Out of memory */
    ODR_OUT_OF_MEM = 1,
    /** SDO abort 0x06010000 - Unsupported access to an object */
    ODR_UNSUPP_ACCESS = 2,
    /** SDO abort 0x06010001 - Attempt to read a write only object */
    ODR_WRITEONLY = 3,
    /** SDO abort 0x06010002 - Attempt to write a read only object */
    ODR_READONLY = 4,
    /** SDO abort 0x06020000 - Object does not exist in the object dict. */
    ODR_IDX_NOT_EXIST = 5,
    /** SDO abort 0x06040041 - Object cannot be mapped to the PDO */
    ODR_NO_MAP = 6,
    /** SDO abort 0x06040042 - PDO length exceeded */
    ODR_MAP_LEN = 7,
    /** SDO abort 0x06040043 - General parameter incompatibility reasons */
    ODR_PAR_INCOMPAT = 8,
    /** SDO abort 0x06040047 - General internal incompatibility in device */
    ODR_DEV_INCOMPAT = 9,
    /** SDO abort 0x06060000 - Access failed due to hardware error */
    ODR_HW = 10,
    /** SDO abort 0x06070010 - Data type does not match */
    ODR_TYPE_MISMATCH = 11,
    /** SDO abort 0x06070012 - Data type does not match, length too high */
    ODR_DATA_LONG = 12,
    /** SDO abort 0x06070013 - Data type does not match, length too short */
    ODR_DATA_SHORT = 13,
    /** SDO abort 0x06090011 - Sub index does not exist */
    ODR_SUB_NOT_EXIST = 14,
    /** SDO abort 0x06090030 - Invalid value for parameter (download only) */
    ODR_INVALID_VALUE = 15,
    /** SDO abort 0x06090031 - Value range of parameter written too high */
    ODR_VALUE_HIGH = 16,
    /** SDO abort 0x06090032 - Value range of parameter written too low */
    ODR_VALUE_LOW = 17,
    /** SDO abort 0x06090036 - Maximum value is less than minimum value */
    ODR_MAX_LESS_MIN = 18,
    /** SDO abort 0x060A0023 - Resource not available: SDO connection */
    ODR_NO_RESOURCE = 19,
    /** SDO abort 0x08000000 - General error */
    ODR_GENERAL = 20,
    /** SDO abort 0x08000020 - Data cannot be transferred or stored to app */
    ODR_DATA_TRANSF = 21,
    /** SDO abort 0x08000021 - Data can't be transf (local control) */
    ODR_DATA_LOC_CTRL = 22,
    /** SDO abort 0x08000022 - Data can't be transf (present device state) */
    ODR_DATA_DEV_STATE = 23,
    /** SDO abort 0x08000023 - Object dictionary not present */
    ODR_OD_MISSING = 23,
    /** SDO abort 0x08000024 - No data available */
    ODR_NO_DATA = 25,
    /** Last element, number of responses */
    ODR_COUNT = 26
} ODR_t;


/**
 * IO stream structure, used for read/write access to OD variable.
 *
 * Structure is initialized with @ref OD_getSub() function.
 */
typedef struct {
    /** Pointer to original data object, defined by Object Dictionary. If
     * memory for data object is not specified by Object Dictionary, then
     * dataObjectOriginal is NULL. Default read/write functions operate on it.
     */
    void *dataObjectOriginal;
    /** Pointer to object, passed by @ref OD_extensionIO_init(). Can be used
     * inside read / write functions from IO extension.
     */
    void *object;
    /** Data length in bytes or 0, if length is not specified */
    OD_size_t dataLength;
    /** In case of large data, dataOffset indicates position of already
     * transferred data */
    OD_size_t dataOffset;
} OD_stream_t;


/**
 * Structure describing properties of a variable, located in specific index and
 * sub-index inside the Object Dictionary.
 *
 * Structure is initialized with @ref OD_getSub() function.
 */
typedef struct {
    /** Object Dictionary index */
    uint16_t index;
    /** Object Dictionary sub-index */
    uint8_t subIndex;
    /** Maximum sub-index in the OD object */
    uint8_t maxSubIndex;
    /** Group for non-volatile storage of the OD object */
    uint8_t storageGroup;
    /** Attribute bit-field of the OD sub-object, see @ref OD_attributes_t */
    OD_attr_t attribute;
    /**
     * Pointer to PDO flags bit-field. This is optional extension of OD object.
     * If OD object has enabled this extension, then each sub-element is coupled
     * with own flagsPDO variable of size 8 to 64 bits (size is configurable
     * by @ref OD_flagsPDO_t). Flag is useful, when variable is mapped to RPDO
     * or TPDO.
     *
     * If sub-element is mapped to RPDO, then bit0 is set to 1 each time, when
     * any RPDO writes new data into variable. Application may clear bit0.
     *
     * If sub-element is mapped to TPDO, then TPDO will set one bit on the time,
     * it is sent. First TPDO will set bit1, second TPDO will set bit2, etc. Up
     * to 63 TPDOs can use flagsPDO.
     *
     * Another functionality is with asynchronous TPDOs, to which variable may
     * be mapped. If corresponding bit is 0, TPDO will be sent. This means, that
     * if application sets variable pointed by flagsPDO to zero, it will trigger
     * sending all asynchronous TPDOs (up to first 63), to which variable is
     * mapped.
     */
    OD_flagsPDO_t *flagsPDO;
    /**
     * Function pointer for reading value from specified variable from Object
     * Dictionary. If OD variable is larger than buf, then this function must
     * be called several times. After completed successful read function returns
     * 'ODR_OK'. If read is partial, it returns 'ODR_PARTIAL'. In case of errors
     * function returns code similar to SDO abort code.
     *
     * Read can be restarted with @ref OD_rwRestart() function.
     *
     * At the moment, when Object Dictionary is initialised, every variable has
     * assigned the same "read" function. This default function simply copies
     * data from Object Dictionary variable. Application can bind its own "read"
     * function for specific object. In that case application is able to
     * calculate data for reading from own internal state at the moment of
     * "read" function call. For this functionality OD object must have IO
     * extension enabled. OD object must also be initialised with
     * @ref OD_extensionIO_init() function call.
     *
     * "read" function must always copy all own data to buf, except if "buf" is
     * not large enough. ("*returnCode" must not return 'ODR_PARTIAL', if there
     * is still space in "buf".)
     *
     * @param stream Object Dictionary stream object, returned from
     *               @ref OD_getSub() function, see @ref OD_stream_t.
     * @param subIndex Object Dictionary subIndex of the accessed element.
     * @param buf Pointer to external buffer, where to data will be copied.
     * @param count Size of the external buffer in bytes.
     * @param [out] returnCode Return value from @ref ODR_t.
     *
     * @return Number of bytes successfully read.
     */
    OD_size_t (*read)(OD_stream_t *stream, uint8_t subIndex,
                      void *buf, OD_size_t count, ODR_t *returnCode);
    /**
     * Function pointer for writing value into specified variable inside Object
     * Dictionary. If OD variable is larger than buf, then this function must
     * be called several times. After completed successful write function
     * returns 'ODR_OK'. If write is partial, it returns 'ODR_PARTIAL'. In case
     * of errors function returns code similar to SDO abort code.
     *
     * Write can be restarted with @ref OD_rwRestart() function.
     *
     * At the moment, when Object Dictionary is initialised, every variable has
     * assigned the same "write" function, which simply copies data to Object
     * Dictionary variable. Application can bind its own "write" function,
     * similar as it can bind "read" function.
     *
     * "write" function must always copy all available data from buf. If OD
     * variable expect more data, then "*returnCode" must return 'ODR_PARTIAL'.
     *
     * @param stream Object Dictionary stream object, returned from
     *               @ref OD_getSub() function, see @ref OD_stream_t.
     * @param subIndex Object Dictionary subIndex of the accessed element.
     * @param buf Pointer to external buffer, from where data will be copied.
     * @param count Size of the external buffer in bytes.
     * @param [out] returnCode Return value from ODR_t.
     *
     * @return Number of bytes successfully written, must be equal to count on
     * success or zero on error.
     */
    OD_size_t (*write)(OD_stream_t *stream, uint8_t subIndex,
                       const void *buf, OD_size_t count, ODR_t *returnCode);
} OD_subEntry_t;


/**
 * Helper structure for storing all objects necessary for frequent read from or
 * write to specific OD variable. Structure can be used by application and can
 * be filled inside and after @ref OD_getSub() function call.
 */
typedef struct {
    /** Object passed to read or write */
    OD_stream_t stream;
    /** Read function pointer, see @ref OD_subEntry_t */
    OD_size_t (*read)(OD_stream_t *stream, uint8_t subIndex,
                      void *buf, OD_size_t count, ODR_t *returnCode);
    /** Write function pointer, see @ref OD_subEntry_t */
    OD_size_t (*write)(OD_stream_t *stream, uint8_t subIndex,
                       const void *buf, OD_size_t count, ODR_t *returnCode);
} OD_IO_t;


/**
 * Object Dictionary entry for one OD object.
 *
 * OD entries are collected inside OD_t as array (list). Each OD entry contains
 * basic information about OD object (index, maxSubIndex and storageGroup) and
 * access function together with a pointer to other details of the OD object.
 */
typedef struct {
    /** Object Dictionary index */
    uint16_t index;
    /** Maximum sub-index in the OD object */
    uint8_t maxSubIndex;
    /** Group for non-volatile storage of the OD object */
    uint8_t storageGroup;
    /** Type of the odObject, indicated by @ref OD_objectTypes_t enumerator. */
    uint8_t odObjectType;
    /** OD object of type indicated by odObjectType, from which @ref OD_getSub()
     * fetches the information */
    const void *odObject;
} OD_entry_t;


/**
 * Object Dictionary
 */
typedef struct {
    /** Number of elements in the list, without last element, which is blank */
    uint16_t size;
    /** List OD entries (table of contents), ordered by index */
    const OD_entry_t *list;
} OD_t;


/**
 * Read value from original OD location
 *
 * This function can be used inside read / write functions, specified by
 * @ref OD_extensionIO_init(). It reads data directly from memory location
 * specified by Object dictionary. If no IO extension is used on OD entry, then
 * subEntry->read returned by @ref OD_getSub() equals to this function. See
 * also @ref OD_subEntry_t.
 */
OD_size_t OD_readOriginal(OD_stream_t *stream, uint8_t subIndex,
                          void *buf, OD_size_t count, ODR_t *returnCode);


/**
 * Write value to original OD location
 *
 * This function can be used inside read / write functions, specified by
 * @ref OD_extensionIO_init(). It writes data directly to memory location
 * specified by Object dictionary. If no IO extension is used on OD entry, then
 * subEntry->write returned by @ref OD_getSub() equals to this function. See
 * also @ref OD_subEntry_t.
 */
OD_size_t OD_writeOriginal(OD_stream_t *stream, uint8_t subIndex,
                           const void *buf, OD_size_t count, ODR_t *returnCode);


/**
 * Find OD entry in Object Dictionary
 *
 * @param od Object Dictionary
 * @param index CANopen Object Dictionary index of object in Object Dictionary
 *
 * @return Pointer to OD entry or NULL if not found
 */
const OD_entry_t *OD_find(const OD_t *od, uint16_t index);


/**
 * Find sub-object with specified sub-index on OD entry returned by OD_find.
 * Function populates subEntry and stream structures with sub-object data.
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param [out] subEntry Structure will be populated on success.
 * @param [out] stream Structure will be populated on success.
 * @param odOrig If true, then potential IO extension on entry will be
 * ignored and access to data entry in the original OD location will be returned
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success.
 */
ODR_t OD_getSub(const OD_entry_t *entry, uint8_t subIndex,
                OD_subEntry_t *subEntry, OD_stream_t *stream, bool_t odOrig);


/**
 * Return index from OD entry
 *
 * @param entry OD entry returned by @ref OD_find().
 *
 * @return OD index
 */
static inline uint16_t OD_getIndex(const OD_entry_t *entry) {
    return entry->index;
}


/**
 * Return maxSubIndex from OD entry
 *
 * @param entry OD entry returned by @ref OD_find().
 *
 * @return OD maxSubIndex
 */
static inline uint8_t OD_getMaxSubIndex(const OD_entry_t *entry) {
    return entry->maxSubIndex;
}


/**
 * Restart read or write operation on OD variable
 *
 * It is not necessary to call this function, if stream was initialised by
 * @ref OD_getSub(). It is also not necessary to call this function, if prevous
 * read or write was successfully finished.
 *
 * @param stream Object Dictionary stream object, returned from
 *               @ref OD_getSub() function, see @ref OD_stream_t.
 */
static inline void OD_rwRestart(OD_stream_t *stream) {
    stream->dataOffset = 0;
}


/**
 * Get SDO abort code from returnCode
 *
 * @param returnCode Returned from some OD access functions
 *
 * @return Corresponding @ref CO_SDO_abortCode_t
 */
uint32_t OD_getSDOabCode(ODR_t returnCode);


/**
 * Initialise extended OD object with own read/write functions
 *
 * This function works on OD object, which has IO extension enabled. It gives
 * application very powerful tool: definition of own IO access on own OD
 * object. Structure and attributes are the same as defined in original OD
 * object, but data are read directly from (or written directly to) application
 * specified object via custom function calls.
 *
 * If this function is not called yet, then normal access ("odOrig" argument is
 * false) to OD entry is disabled.
 *
 * @warning
 * Object dictionary storage works only directly on OD variables. It does not
 * access read function specified here. So, if extended OD objects needs to be
 * preserved, then @ref OD_writeOriginal can be used inside custom write
 * function.
 *
 * @warning
 * Read and write functions may be called from different threads, so critical
 * sections in custom functions must be protected with @ref CO_LOCK_OD() and
 * @ref CO_UNLOCK_OD().
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param object Object, which will be passed to read or write function.
 * @param read Read function pointer. If NULL, then read will be disabled.
 * @ref OD_readOriginal can be used here to keep original read function.
 * For function description see @ref OD_subEntry_t.
 * @param write Write function pointer. If NULL, then write will be disabled.
 * @ref OD_writeOriginal can be used here to keep original write function.
 * For function description see @ref OD_subEntry_t.
 *
 * @return true on success, false if OD object doesn't exist or is not extended.
 */
bool_t OD_extensionIO_init(const OD_entry_t *entry,
                           void *object,
                           OD_size_t (*read)(OD_stream_t *stream,
                                             uint8_t subIndex,
                                             void *buf,
                                             OD_size_t count,
                                             ODR_t *returnCode),
                           OD_size_t (*write)(OD_stream_t *stream,
                                              uint8_t subIndex,
                                              const void *buf,
                                              OD_size_t count,
                                              ODR_t *returnCode));


/**
 * @defgroup CO_ODgetSetters Getters and setters
 * @{
 *
 * Getter and setter helper functions for accessing different types of Object
 * Dictionary variables.
 */
/**
 * Get int8_t variable from Object Dictionary
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param [out] val Value will be written here.
 * @param odOrig If true, then potential IO extension on entry will be
 * ignored and data in the original OD location will be returned.
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success. Error, if
 * variable does not exist in object dictionary or it does not have the correct
 * length or other reason.
 */
ODR_t OD_get_i8(const OD_entry_t *entry, uint8_t subIndex,
                int8_t *val, bool_t odOrig);
/** Get int16_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_i16(const OD_entry_t *entry, uint8_t subIndex,
                 int16_t *val, bool_t odOrig);
/** Get int32_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_i32(const OD_entry_t *entry, uint8_t subIndex,
                 int32_t *val, bool_t odOrig);
/** Get int64_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_i64(const OD_entry_t *entry, uint8_t subIndex,
                 int64_t *val, bool_t odOrig);
/** Get uint8_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u8(const OD_entry_t *entry, uint8_t subIndex,
                uint8_t *val, bool_t odOrig);
/** Get uint16_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u16(const OD_entry_t *entry, uint8_t subIndex,
                 uint16_t *val, bool_t odOrig);
/** Get uint32_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u32(const OD_entry_t *entry, uint8_t subIndex,
                 uint32_t *val, bool_t odOrig);
/** Get uint64_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u64(const OD_entry_t *entry, uint8_t subIndex,
                 uint64_t *val, bool_t odOrig);
/** Get float32_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_r32(const OD_entry_t *entry, uint8_t subIndex,
                 float32_t *val, bool_t odOrig);
/** Get float64_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_r64(const OD_entry_t *entry, uint8_t subIndex,
                 float64_t *val, bool_t odOrig);

/**
 * Set int8_t variable in Object Dictionary
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param val Value to write.
 * @param odOrig If true, then potential IO extension on entry will be
 * ignored and data in the original OD location will be written.
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success. Error, if
 * variable does not exist in object dictionary or it does not have the correct
 * length or other reason.
 */
ODR_t OD_set_i8(const OD_entry_t *entry, uint8_t subIndex,
                int8_t val, bool_t odOrig);
/** Set int16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_i16(const OD_entry_t *entry, uint8_t subIndex,
                 int16_t val, bool_t odOrig);
/** Set int16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_i32(const OD_entry_t *entry, uint8_t subIndex,
                 int32_t val, bool_t odOrig);
/** Set int16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_i64(const OD_entry_t *entry, uint8_t subIndex,
                 int64_t val, bool_t odOrig);
/** Set uint8_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u8(const OD_entry_t *entry, uint8_t subIndex,
                uint8_t val, bool_t odOrig);
/** Set uint16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u16(const OD_entry_t *entry, uint8_t subIndex,
                 uint16_t val, bool_t odOrig);
/** Set uint32_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u32(const OD_entry_t *entry, uint8_t subIndex,
                 uint32_t val, bool_t odOrig);
/** Set uint64_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u64(const OD_entry_t *entry, uint8_t subIndex,
                 uint64_t val, bool_t odOrig);
/** Set float32_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_r32(const OD_entry_t *entry, uint8_t subIndex,
                 float32_t val, bool_t odOrig);
/** Set float64_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_r64(const OD_entry_t *entry, uint8_t subIndex,
                 float64_t val, bool_t odOrig);

/**
 * Get pointer to int8_t variable from Object Dictionary
 *
 * Function always returns "dataObjectOriginal" pointer, which points to data
 * in the original OD location. Take care, if IO extension is enabled on OD
 * entry.
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param [out] val Pointer to variable will be written here.
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success. Error, if
 * variable does not exist in object dictionary or it does not have the correct
 * length or other reason.
 */
ODR_t OD_getPtr_i8(const OD_entry_t *entry, uint8_t subIndex, int8_t **val);
/** Get pointer to int16_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_i16(const OD_entry_t *entry, uint8_t subIndex, int16_t **val);
/** Get pointer to int32_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_i32(const OD_entry_t *entry, uint8_t subIndex, int32_t **val);
/** Get pointer to int64_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_i64(const OD_entry_t *entry, uint8_t subIndex, int64_t **val);
/** Get pointer to uint8_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_u8(const OD_entry_t *entry, uint8_t subIndex, uint8_t **val);
/** Get pointer to uint16_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_u16(const OD_entry_t *entry, uint8_t subIndex, uint16_t **val);
/** Get pointer to uint32_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_u32(const OD_entry_t *entry, uint8_t subIndex, uint32_t **val);
/** Get pointer to uint64_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_u64(const OD_entry_t *entry, uint8_t subIndex, uint64_t **val);
/** Get pointer to float32_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_r32(const OD_entry_t *entry, uint8_t subIndex, float32_t **val);
/** Get pointer to float64_t variable from OD, see @ref OD_getPtr_i8 */
ODR_t OD_getPtr_r64(const OD_entry_t *entry, uint8_t subIndex, float64_t **val);
/** @} */ /* CO_ODgetSetters */


#if defined OD_DEFINITION || defined CO_DOXYGEN
/**
 * @defgroup CO_ODdefinition OD definition objects
 * @{
 *
 * Types and functions used only for definition of Object Dictionary
 */
/**
 * Types for OD object.
 */
typedef enum {
    /** This type corresponds to CANopen Object Dictionary object with object
     * code equal to VAR. OD object is type of @ref OD_obj_var_t and represents
     * single variable of any type (any length), located on sub-index 0. Other
     * sub-indexes are not used. */
    ODT_VAR = 0x01,
    /** This type corresponds to CANopen Object Dictionary object with object
     * code equal to ARRAY. OD object is type of @ref OD_obj_array_t and
     * represents array of variables with the same type, located on sub-indexes
     * above 0. Sub-index 0 is of type uint8_t and usually represents length of
     * the array. */
    ODT_ARR = 0x02,
    /** This type corresponds to CANopen Object Dictionary object with object
     * code equal to RECORD. This type of OD object represents structure of
     * the variables. Each variable from the structure can have own type and
     * own attribute. OD object is an array of elements of type
     * @ref OD_obj_var_t. Variable at sub-index 0 is of type uint8_t and usually
     * represents number of sub-elements in the structure. */
    ODT_REC = 0x03,

    /** Same as ODT_VAR, but extended with OD_obj_extended_t type. It includes
     * additional pointer to IO extension and PDO flags */
    ODT_EVAR = 0x11,
    /** Same as ODT_ARR, but extended with OD_obj_extended_t type */
    ODT_EARR = 0x12,
    /** Same as ODT_REC, but extended with OD_obj_extended_t type */
    ODT_EREC = 0x13,

    /** Mask for basic type */
    ODT_TYPE_MASK = 0x0F,
    /** Mask for extension */
    ODT_EXTENSION_MASK = 0x10
} OD_objectTypes_t;

/**
 * Object for single OD variable, used for "VAR" and "RECORD" type OD objects
 */
typedef struct {
    void *data; /**< Pointer to data */
    OD_attr_t attribute; /**< Attribute bitfield, see @ref OD_attributes_t */
    OD_size_t dataLength; /**< Data length in bytes */
} OD_obj_var_t;

/**
 * Object for OD array of variables, used for "ARRAY" type OD objects
 */
typedef struct {
    uint8_t *data0; /**< Pointer to data for sub-index 0 */
    void *data; /**< Pointer to array of data */
    OD_attr_t attribute0; /**< Attribute bitfield for sub-index 0, see
                               @ref OD_attributes_t */
    OD_attr_t attribute; /**< Attribute bitfield for array elements */
    OD_size_t dataElementLength; /**< Data length of array elements in bytes */
    OD_size_t dataElementSizeof; /**< Sizeof one array element in bytes */
} OD_obj_array_t;

/**
 * Object pointed by @ref OD_obj_extended_t contains application specified
 * parameters for extended OD object
 */
typedef struct {
    /** Object on which read and write will operate */
    void *object;
    /** Application specified function pointer, see @ref OD_subEntry_t. */
    OD_size_t (*read)(OD_stream_t *stream, uint8_t subIndex,
                      void *buf, OD_size_t count, ODR_t *returnCode);
    /** Application specified function pointer, see @ref OD_subEntry_t. */
    OD_size_t (*write)(OD_stream_t *stream, uint8_t subIndex,
                       const void *buf, OD_size_t count, ODR_t *returnCode);
} OD_extensionIO_t;

/**
 * Object for extended type of OD variable, configurable by
 * @ref OD_extensionIO_init() function
 */
typedef struct {
    /** Pointer to PDO flags bit-field, see @ref OD_subEntry_t, may be NULL. */
    OD_flagsPDO_t *flagsPDO;
    /** Pointer to application specified IO extension, may be NULL. */
    OD_extensionIO_t *extIO;
    /** Pointer to original odObject, see @ref OD_entry_t. */
    const void *odObjectOriginal;
} OD_obj_extended_t;

/** @} */ /* CO_ODdefinition */

#endif /* defined OD_DEFINITION */

/** @} */ /* CO_ODinterface */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_OD_INTERFACE_H */
