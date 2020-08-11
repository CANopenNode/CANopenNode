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
 * @defgroup CO_ODinterface Object Dictionary interface
 * @ingroup CO_CANopen_301
 * @{
 * See @ref CO_ODinterface_operation
 */
/**
 * @defgroup CO_ODinterface_operation Operation
 * @{
 * The CANopen Object Dictionary is essentially a grouping of objects accessible
 * via the network in an ordered pre-defined fashion.
 *
 * Each object within the object dictionary is addressed using a 16-bit index
 * and a 8-bit sub-index.
 *
 * ### Terms
 * The term **OD object** means object from Object Dictionary located at
 * specific 16-bit index. There are different types of OD objects in CANopen:
 * variables, arrays and records (structures). Each OD object contains pointer
 * to actual data, data length(s) and attribute(s). See @ref OD_objectTypes_t.
 *
 * The term **OD variable** is basic variable of specified type. For example:
 * int8_t, uint32_t, float64_t, ... or just sequence of binary data with known
 * or unknown data length. Each OD variable resides in Object dictionary at
 * specified 16-bit index and 8-bit sub-index.
 *
 * The term **OD entry** means structure element, which contains some basic
 * properties of the OD object, indication of type of OD object and pointer to
 * all necessary data for the OD object. An array of OD entries together with
 * information about total number of OD entries represents Object Dictionary as
 * defined inside CANopenNode. See @ref OD_entry_t and @ref OD_t.
 *
 * ### Access
 * Application and the stack have access to OD objects via universal @ref OD_t
 * object and @ref OD_find() function, no direct access to custom structures,
 * which define Object Dictionary, is required. Properties for specific
 * OD variable is fetched with @ref OD_getSub() function. And access to actual
 * variable is via @b read and @b write functions. Pointer to those two
 * functions is fetched by @ref OD_getSub(). See @ref OD_stream_t and
 * @ref OD_subEntry_t. See also shortcuts: @ref CO_ODgetSetters, for access to
 * data of different type.
 *
 * ### Optional extensions
 * There are some optional extensions to the Object Dictionary:
 *   * **PDO flags** informs application, if specific OD variable was received
 *     or sent by PDO. And also gives the application ability to request a TPDO,
 *     to which variable is possibly mapped.
 *   * **IO extension** gives the application ability to take full control over
 *     the OD object. Application can specify own @b read and @b write functions
 *     and own object, on which they operate.
 *
 * ### Example usage
 * @code
extern const OD_t ODxyz;

void myFunc(const OD_t *od) {
    ODR_t ret;
    const OD_entry_t *entry;
    OD_subEntry_t subEntry;
    OD_IO_t io1008;
    char buf[50];
    OD_size_t bytesRd;
    int error = 0;

    //Init IO for "Manufacturer device name" at index 0x1008, sub-index 0x00
    entry = OD_find(od, 0x1008);
    ret = OD_getSub(entry, 0x00, &subEntry, &io1008.stream);
    io1008.read = subEntry.read;
    //Read with io1008
    if (ret == ODR_OK)
        bytesRd = io1008.read(&io1008.stream, 0x00, &buf[0], sizeof(buf), &ret);
    if (ret != ODR_OK) error++;

    //Use helper and set "Producer heartbeat time" at index 0x1008, sub 0x00
    ret = OD_set_u16(OD_find(od, 0x1017), 0x00, 500);
    if (ret != ODR_OK) error++;
}
 * @endcode
 * There is no need to include ODxyt.h file, it is only necessary to know, we
 * have ODxyz defined somewhere.
 *
 * Second example is simpler and use helper function to access OD variable.
 * However it is not very efficient, because it goes through all search
 * procedures.
 *
 * If access to the same variable is very frequent, it is better to
 * use first example. After initialization, application has to remember only
 * "io1008" object. Frequent reading of the variable is then very efficient.
 *
 * ### Simple access to OD via globals
 * Some simple user applications can also access some OD variables directly via
 * globals.
 *
 * @warning
 * If OD object has IO extension enabled, then direct access to its OD variables
 * must not be used. Only valid access is via read or write or helper functions.
 *
 * @code
#include ODxyz.h

void myFuncGlob(void) {
    //Direct address instead of OD_find()
    const OD_entry_t *entry_errReg = ODxyz_1001_errorRegister;

    //Direct access to OD variable
    uint32_t devType = ODxyz_0.x1000_deviceType;
    ODxyz_0.x1018_identity.serialNumber = 0x12345678;
}
 * @endcode
 * @} */

/**
 * @defgroup CO_ODinterface_OD_example Object Dictionary example
 * @{
 * Actual Object dictionary for one CANopen device is defined by pair of
 * OD_xyz.h and ODxyz.h files.
 *
 * "xyz" is name of Object Dictionary, usually "0" is used for default.
 * Configuration with multiple Object Dictionaries is also possible.
 *
 * Data for OD definition are arranged inside multiple structures. Structures
 * are different for different configuration of OD. Data objects, created with
 * those structures, are constant or are variable.
 *
 * Actual OD variables are arranged inside multiple structures, so called
 * storage groups. Selected groups can be stored to non-volatile memory.
 *
 * @warning
 * Manual editing of ODxyz.h/.c files is very error-prone.
 *
 * Pair of ODxyz.h/.c files can be generated by OD editor tool. The tool can
 * edit standard CANopen device description file in xml format. Xml file may
 * include also some non-standard elements, specific to CANopenNode. Xml file is
 * then used for automatic generation of ODxyz.h/.c files.
 *
 * ### Example ODxyz.h file
 * @code
typedef struct {
    uint32_t x1000_deviceType;
    uint8_t x1001_errorRegister;
    struct {
        uint8_t maxSubIndex;
        uint32_t vendorID;
        uint32_t productCode;
        uint32_t revisionNumber;
        uint32_t serialNumber;
    } x1018_identity;
} ODxyz_0_t;

typedef struct {
    uint8_t x1001_errorRegister;
} ODxyz_1_t;

extern ODxyz_0_t ODxyz_0;
extern ODxyz_1_t ODxyz_1;
extern const OD_t ODxyz;

#define ODxyz_1000_deviceType &ODxyz.list[0]
#define ODxyz_1001_errorRegister &ODxyz.list[1]
#define ODxyz_1018_identity &ODxyz.list[2]
 * @endcode
 *
 * ### Example ODxyz.c file
 * @code
#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "ODxyz.h"

typedef struct {
    OD_extensionIO_t xio_1001_errorRegister;
} ODxyz_ext_t;

typedef struct {
    OD_obj_var_t o_1000_deviceType;
    OD_obj_var_t orig_1001_errorRegister;
    OD_obj_extended_t o_1001_errorRegister;
    OD_obj_var_t o_1018_identity[5];
} ODxyz_objs_t;

ODxyz_0_t ODxyz_0 = {
    .x1000_deviceType = 0L,
    .x1018_identity = {
        .maxSubIndex = 4,
        .vendorID = 0L,
        .productCode = 0L,
        .revisionNumber = 0L,
        .serialNumber = 0L,
    },
};

ODxyz_1_t ODxyz_1 = {
    .x1001_errorRegister = 0,
};

static ODxyz_ext_t ODxyz_ext = {0};

static const ODxyz_objs_t ODxyz_objs = {
    .o_1000_deviceType = {
        .data = &ODxyz_0.x1000_deviceType,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4,
    },
    .orig_1001_errorRegister = {
        .data = &ODxyz_1.x1001_errorRegister,
        .attribute = ODA_SDO_R,
        .dataLength = 1,
    },
    .o_1001_errorRegister = {
        .flagsPDO = NULL,
        .extIO = &ODxyz_ext.xio_1001_errorRegister,
        .odObjectOriginal = &ODxyz_objs.orig_1001_errorRegister,
    },
    .o_1018_identity = {
        {
            .data = &ODxyz_0.x1018_identity.maxSubIndex,
            .attribute = ODA_SDO_R,
            .dataLength = 1,
        },
        {
            .data = &ODxyz_0.x1018_identity.vendorID,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4,
        },
        {
            .data = &ODxyz_0.x1018_identity.productCode,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4,
        },
        {
            .data = &ODxyz_0.x1018_identity.revisionNumber,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4,
        },
        {
            .data = &ODxyz_0.x1018_identity.serialNumber,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4,
        },
    }
};

const OD_t ODxyz = {
    3, {
    {0x1000, 0, 0, ODT_VAR, &ODxyz_objs.o_1000_deviceType},
    {0x1001, 0, 1, ODT_EVAR, &ODxyz_objs.o_1001_errorRegister},
    {0x1018, 4, 0, ODT_REC, &ODxyz_objs.o_1018_identity},
    {0x0000, 0, 0, 0, NULL}
}};
 * @endcode
 * @} */


/**
 * @defgroup CO_ODinterface_XDD XML device description
 * @{
 * CANopen device description - XML schema definition - is specified by CiA 311
 * standard.
 *
 * CiA 311 complies with standard ISO 15745-1:2005/Amd1 (Industrial automation
 * systems and integration - Open systems application integration framework).
 *
 * CANopen device description is basically a XML file with all the information
 * about CANopen device. The larges part of the file is a list of all object
 * dictionary variables with all necessary properties and documentation. This
 * file can be edited with OD editor application and can be used as data source,
 * from which Object dictionary for CANopenNode is generated. Furthermore, this
 * file can be used with CANopen configuration tool, which interacts with
 * CANopen devices on running CANopen network.
 *
 * XML schema definitions are available at: http://www.canopen.org/xml/1.1
 * One of the tools for viewing XML schemas is "xsddiagram"
 * (https://github.com/dgis/xsddiagram).
 *
 * CANopen specifies also another type of files for CANopen device description.
 * These are EDS files, which are in INI format. It is possible to convert
 * between those two formats. But CANopenNode uses XML format.
 *
 * The device description file has "XDD" file extension. The name of this file
 * shall contain the vendor-ID of the CANopen device in the form of 8
 * hexadecimal digits in any position of the name and separated with
 * underscores. For example "name1_12345678_name2.XDD".
 *
 * CANopenNode includes multiple profile definition files, one for each CANopen
 * object. Those files have "XPD" extension. They are in the same XML format as
 * XDD files. The XML editor tool can use XPD files to insert prepared data
 * into device description file (XDD), which is being edited.
 *
 * ### XDD, XPD file example
 * @code{.xml}
<?xml version="1.0" encoding="utf-8"?>
<ISO15745ProfileContainer
  xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
  xmlns="http://www.canopen.org/xml/1.0">
  <ISO15745Profile>
    <ProfileHeader>...</ProfileHeader>
    <ProfileBody
      xsi:type="ProfileBody_Device_CANopen"
      fileName="..." fileCreator="..." fileCreationDate="..." fileVersion="...">
      <DeviceIdentity>...</DeviceIdentity>
      <DeviceFunction>...</DeviceFunction>
      <ApplicationProcess>
        <dataTypeList>...</dataTypeList>
        <parameterList>
          <parameter uniqueID="UID_PARAM_1000" access="read">
            <label lang="en">Device type</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <UINT/>
            <defaultValue value="0x00000000" />
          </parameter>
          <parameter uniqueID="UID_PARAM_1001" access="read">
            <label lang="en">Error register</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <BYTE/>
            <defaultValue value="0" />
            <property name="CO_storageGroup" value="1">
            <property name="CO_extensionIO" value="true">
          </parameter>
          <parameter uniqueID="UID_PARAM_1018">
            <label lang="en">Identity</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <dataTypeIDRef uniqueIDRef="..." />
          </parameter>
          <parameter uniqueID="UID_PARAM_101800" access="read">
            <label lang="en">max sub-index</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <USINT/>
            <defaultValue value="4" />
          </parameter>
          <parameter uniqueID="UID_PARAM_101801" access="read">
            <label lang="en">Vendor-ID</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <UINT/>
            <defaultValue value="0x00000000" />
          </parameter>
          <parameter uniqueID="UID_PARAM_101802" access="read">
            <label lang="en">Product code</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <UINT/>
            <defaultValue value="0x00000000" />
          </parameter>
          <parameter uniqueID="UID_PARAM_101803" access="read">
            <label lang="en">Revision number</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <UINT/>
            <defaultValue value="0x00000000" />
          </parameter>
          <parameter uniqueID="UID_PARAM_101804" access="read">
            <label lang="en">Serial number</label>
            <description lang="en">...</description>
            <denotation>...</denotation>
            <UINT/>
            <defaultValue value="0x00000000" />
          </parameter>
        </parameterList>
        <parameterGroupList>
          <parameterGroup uniqueID="UID_PG_CO_COMM">
            <label lang="en">CANopen Communication Parameters</label>
            <description lang="en">...</description>
            <parameterGroup uniqueID="UID_PG_CO_COMM_COMMON">
              <label lang="en">CANopen Common Communication Parameters</label>
              <description lang="en">...</description>
              <parameterRef uniqueIDRef="UID_PARAM_1000" />
              <parameterRef uniqueIDRef="UID_PARAM_1001" />
              <parameterRef uniqueIDRef="UID_PARAM_1018" />
            </parameterGroup>
          </parameterGroup>
        </parameterGroupList>
      </ApplicationProcess>
    </ProfileBody>
  </ISO15745Profile>
  <ISO15745Profile>
    <ProfileHeader>...</ProfileHeader>
    <ProfileBody
      xsi:type="ProfileBody_CommunicationNetwork_CANopen"
      fileName="..." fileCreator="..." fileCreationDate="..." fileVersion="...">
      <ApplicationLayers>
        <CANopenObjectList>
          <CANopenObject index="1000" name="Device type"
            objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_1000" />
          <CANopenObject index="1001" name="Error register"
            objectType="7" PDOmapping="TPDO" uniqueIDRef="UID_PARAM_1001" />
          <CANopenObject index="1018" name="Identity"
            objectType="9" subNumber="5" uniqueIDRef="UID_PARAM_1018">
            <CANopenSubObject subIndex="00" name="max sub-index"
              objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101800" />
            <CANopenSubObject subIndex="01" name="Vendor-ID"
              objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101801" />
            <CANopenSubObject subIndex="02" name="Product code"
              objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101802" />
            <CANopenSubObject subIndex="03" name="Revision number"
              objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101803" />
            <CANopenSubObject subIndex="04" name="Serial number"
              objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101804" />
          </CANopenObject>
        </CANopenObjectList>
      </ApplicationLayers>
      <TransportLayers>...</TransportLayers>
    </ProfileBody>
  </ISO15745Profile>
</ISO15745ProfileContainer>
 * @endcode
 *
 * ### Parameter description
 * Above XML file example shows necessary data for OD interface used by
 * CANopenNode and other parameters required by the standard. Standard specifies
 * many other parameters, which are not used by CANopenNode for simplicity.
 *
 * XML file is divided into two parts:
 *   1. "ProfileBody_Device_CANopen" - more standardized information
 *   2. "ProfileBody_CommunicationNetwork_CANopen" - communication related info
 *
 * Most important part of the XML file is definition of each OD object. All
 * OD objects are listed in "CANopenObjectList", which resides in the second
 * part of the XML file. Each "CANopenObject" and "CANopenSubObject" of the list
 * contains a link to parameter ("uniqueIDRef"), which resides in the first
 * part of the XML file. So data for each OD object is split between two parts
 * of the XML file.
 *
 * #### &lt;CANopenObject&gt;
 *   * "index" (required) - Object dictionary index
 *   * "name" (required) - Name of the parameter
 *   * "objectType" (required) - "7"=VAR, "8"=ARRAY, "9"=RECORD
 *   * "subNumber" (required if "objectType" is "8" or "9")
 *   * "PDOmapping" (optional if "objectType" is "7", default is "no"):
 *     * "no" - mapping not allowed
 *     * "default" - not used, same as "optional"
 *     * "optional" - mapping allowed to TPDO or RPDO
 *     * "TPDO" - mapping allowed to TPDO
 *     * "RPDO" mapping allowed to RPDO
 *   * "uniqueIDRef" (required or see below) - Reference to &lt;parameter&gt;
 *
 * #### &lt;CANopenSubObject&gt;
 *   * "subIndex" (required) - Object dictionary sub-index
 *   * "name" (required) - Name of the parameter
 *   * "objectType" (required, always "7")
 *   * "PDOmapping" (optional, same as above, default is "no")
 *   * "uniqueIDRef" (required or see below) - Reference to &lt;parameter&gt;
 *
 * #### uniqueIDRef
 * This is required attribute from "CANopenObject" and "CANopenSubObject". It
 * contains reference to &lt;parameter&gt; in "ProfileBody_Device_CANopen"
 * section of the XML file. There are additional necessary properties.
 *
 * If "uniqueIDRef" attribute is not specified and "objectType" is 7(VAR), then
 * "CANopenObject" or "CANopenSubObject" must contain additional attributes:
 *   * "dataType" (required for VAR) - CANopen basic data type, see below
 *   * "accessType" (required for VAR) - "ro", "wo", "rw" or "const"
 *   * "defaultValue" (optional) - Default value for the variable.
 *   * "denotation" (optional) - Not used by CANopenNode.
 *
 * #### &lt;parameter&gt;
 *   * "uniqueID" (required)
 *   * "access" (required for VAR) - can be one of:
 *     * "const" - same as "read"
 *     * "read" - only read access with SDO or PDO
 *     * "write" - only write access with SDO or PDO
 *     * "readWrite" - read or write access with SDO or PDO
 *     * "readWriteInput" - same as "readWrite"
 *     * "readWriteOutput" - same as "readWrite"
 *     * "noAccess" - object will be in Object Dictionary, but no access.
 *   * &lt;label lang="en"&gt; (required)
 *   * &lt;description lang="en"&gt; (required)
 *   * &lt;UINT and similar/&gt; (required) - Basic or complex data type. Basic
 *     data type (for VAR) is specified in IEC 61131-3 (see below). If data type
 *     is complex (ARRAY or RECORD), then &lt;dataTypeIDRef&gt; must be
 *     specified and entry must be added in the &lt;dataTypeList&gt;. Such
 *     definition of complex data types is required by the standard, but it is
 *     not required by CANopenNode.
 *   * &lt;defaultValue&gt; (optional for VAR) - Default value for the variable.
 *
 * Additional, optional, CANopenNode specific properties, which can be used
 * inside parameters describing &lt;CANopenObject&gt;:
 *   * &lt;property name="CO_storageGroup" value="..."&gt; - group into which
 *     the C variable will belong. Variables from specific storage group may
 *     then be stored into non-volatile memory, automatically or by SDO command.
 *     Valid value is from 0x00 (default - not stored) to 0x7F.
 *   * &lt;property name="CO_extensionIO" value="..."&gt; - Valid value is
 *     "false" (default) or "true", if IO extension is enabled.
 *   * &lt;property name="CO_flagsPDO" value="..."&gt; - Valid value is
 *     "false" (default) or "true", if PDO flags are enabled.
 *   * &lt;property name="CO_countLabel" value="..."&gt; - Valid value is
 *     any string without spaces. OD exporter will generate a macro for each
 *     different string. For example, if four OD objects have "CO_countLabel"
 *     set to "TPDO", then macro "#define ODxyz_NO_TPDO 4" will be generated by
 *     OD exporter.
 *
 * Additional, optional, CANopenNode specific property, which can be used
 * inside parameters describing "VAR":
 *   * &lt;property name="CO_accessSRDO" value="..."&gt; - Valid values are:
 *     "tx", "rx", "trx", "no"(default).
 *   * &lt;property name="CO_byteLength" value="..."&gt; - Length of the
 *     variable in bytes, optionaly used by string or domain. If CO_byteLength
 *     is not specified for string, then length is calculated from string
 *     &lt;defaultValue&gt;.
 *
 * #### CANopen basic data types
 * | CANopenNode  | IEC 61131-3  | CANopen         | dataType |
 * | ------------ | ------------ | --------------- | -------- |
 * | bool_t       | BOOL         | BOOLEAN         | 0x01     |
 * | int8_t       | CHAR, SINT   | INTEGER8        | 0x02     |
 * | int16_t      | INT          | INTEGER16       | 0x03     |
 * | int32_t      | DINT         | INTEGER32       | 0x04     |
 * | int64_t      | LINT         | INTEGER64       | 0x15     |
 * | uint8_t      | BYTE, USINT  | UNSIGNED8       | 0x05     |
 * | uint16_t     | WORD, UINT   | UNSIGNED16      | 0x06     |
 * | uint32_t     | DWORD, UDINT | UNSIGNED32      | 0x07     |
 * | uint64_t     | LWORD, ULINT | UNSIGNED64      | 0x1B     |
 * | float32_t    | REAL         | REAL32          | 0x08     |
 * | float64_t    | LREAL        | REAL64          | 0x11     |
 * | bytestring_t | STRING       | VISIBLE_STRING  | 0x09     |
 * | bytestring_t | -            | OCTET_STRING    | 0x0A     |
 * | bytestring_t | WSTRING      | UNICODE_STRING  | 0x0B     |
 * | bytestring_t | -            | TIME_OF_DAY     | 0x0C     |
 * | bytestring_t | -            | TIME_DIFFERENCE | 0x0D     |
 * | bytestring_t | -            | DOMAIN          | 0x0F     |
 * | bytestring_t | BITSTRING    | -               | -        |
 *
 * #### &lt;parameterGroupList&gt;
 * This is optional element and is not required by standard, nor by CANopenNode.
 * This can be very useful for documentation, which can be organised into
 * multiple chapters with multiple levels. CANopen objects can then be organised
 * in any way, not only by index.
 *
 * #### Other elements
 * Other elements listed in the above XML example are required by the standard
 * and does not influence the CANopenNode object dictionary generator.
 * @} */


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
    OD_H1015_INHIBIT_TIME_MSG   = 0x1015U,/**< Inhibit time message */
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
    OD_H1200_SDO_SERVER_PARAM   = 0x1200U,/**< SDO server parameters */
    OD_H1280_SDO_CLIENT_PARAM   = 0x1280U,/**< SDO client parameters */
    OD_H1400_RXPDO_1_PARAM      = 0x1400U,/**< RXPDO communication parameter */
    OD_H1401_RXPDO_2_PARAM      = 0x1401U,/**< RXPDO communication parameter */
    OD_H1402_RXPDO_3_PARAM      = 0x1402U,/**< RXPDO communication parameter */
    OD_H1403_RXPDO_4_PARAM      = 0x1403U,/**< RXPDO communication parameter */
    OD_H1600_RXPDO_1_MAPPING    = 0x1600U,/**< RXPDO mapping parameters */
    OD_H1601_RXPDO_2_MAPPING    = 0x1601U,/**< RXPDO mapping parameters */
    OD_H1602_RXPDO_3_MAPPING    = 0x1602U,/**< RXPDO mapping parameters */
    OD_H1603_RXPDO_4_MAPPING    = 0x1603U,/**< RXPDO mapping parameters */
    OD_H1800_TXPDO_1_PARAM      = 0x1800U,/**< TXPDO communication parameter */
    OD_H1801_TXPDO_2_PARAM      = 0x1801U,/**< TXPDO communication parameter */
    OD_H1802_TXPDO_3_PARAM      = 0x1802U,/**< TXPDO communication parameter */
    OD_H1803_TXPDO_4_PARAM      = 0x1803U,/**< TXPDO communication parameter */
    OD_H1A00_TXPDO_1_MAPPING    = 0x1A00U,/**< TXPDO mapping parameters */
    OD_H1A01_TXPDO_2_MAPPING    = 0x1A01U,/**< TXPDO mapping parameters */
    OD_H1A02_TXPDO_3_MAPPING    = 0x1A02U,/**< TXPDO mapping parameters */
    OD_H1A03_TXPDO_4_MAPPING    = 0x1A03U /**< TXPDO mapping parameters */
} OD_ObjDicId_301_t;


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
    ODA_NOINIT = 0x80, /**< Variable has no initial value. Can be used with
    OD objects, which has IO extension enabled. Object dictionary does not
    reserve memory for the variable and storage is not used. */
} OD_attributes_t;


/**
 * Return codes from OD access functions
 */
typedef enum {
/* !!!! WARNING !!!! */
/* If changing these values, change also OD_getSDOabCode() function! */
    ODR_PARTIAL        = -1, /**< Read/write is only partial, make more calls */
    ODR_OK             = 0,  /**< Read/write successfully finished */
    ODR_OUT_OF_MEM     = 1,  /**< Out of memory */
    ODR_UNSUPP_ACCESS  = 2,  /**< Unsupported access to an object */
    ODR_WRITEONLY      = 3,  /**< Attempt to read a write only object */
    ODR_READONLY       = 4,  /**< Attempt to write a read only object */
    ODR_IDX_NOT_EXIST  = 5,  /**< Object does not exist in the object dict. */
    ODR_NO_MAP         = 6,  /**< Object cannot be mapped to the PDO */
    ODR_MAP_LEN        = 7,  /**< PDO length exceeded */
    ODR_PAR_INCOMPAT   = 8,  /**< General parameter incompatibility reasons */
    ODR_DEV_INCOMPAT   = 9,  /**< General internal incompatibility in device */
    ODR_HW             = 10, /**< Access failed due to hardware error */
    ODR_TYPE_MISMATCH  = 11, /**< Data type does not match */
    ODR_DATA_LONG      = 12, /**< Data type does not match, length too high */
    ODR_DATA_SHORT     = 13, /**< Data type does not match, length too short */
    ODR_SUB_NOT_EXIST  = 14, /**< Sub index does not exist */
    ODR_INVALID_VALUE  = 15, /**< Invalid value for parameter (download only) */
    ODR_VALUE_HIGH     = 16, /**< Value range of parameter written too high */
    ODR_VALUE_LOW      = 17, /**< Value range of parameter written too low */
    ODR_MAX_LESS_MIN   = 18, /**< Maximum value is less than minimum value */
    ODR_NO_RESOURCE    = 19, /**< Resource not available: SDO connection */
    ODR_GENERAL        = 20, /**< General error */
    ODR_DATA_TRANSF    = 21, /**< Data cannot be transferred or stored to app */
    ODR_DATA_LOC_CTRL  = 22, /**< Data can't be transf (local control) */
    ODR_DATA_DEV_STATE = 23, /**< Data can't be transf (present device state) */
    ODR_OD_MISSING     = 23, /**< Object dictionary not present */
    ODR_NO_DATA        = 25, /**< No data available */
    ODR_COUNT          = 26  /**< Last element, number of responses */
} ODR_t;


/**
 * IO stream structure, used for read/write access to OD variable.
 *
 * Structure is initialized with @ref OD_getSub() function.
 */
typedef struct {
    /** Pointer to data object, on which will operate read/write function */
    void *dataObject;
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
     * mapped. */
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
    OD_entry_t list[];
} OD_t;


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
 * @warning If this function is called on OD object, which has IO extension
 * enabled and @ref OD_extensionIO_init() was not (yet) called on that object,
 * then subEntry and stream structures are populated with properties of
 * "original OD object". Call to this function after @ref OD_extensionIO_init()
 * will populate subEntry and stream structures with properties of
 * "newly initialised OD object". This is something very different.
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param [out] subEntry Structure will be populated on success.
 * @param [out] stream Structure will be populated on success.
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success.
 */
ODR_t OD_getSub(const OD_entry_t *entry, uint8_t subIndex,
                OD_subEntry_t *subEntry, OD_stream_t *stream);


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
 * One more feature is available on IO extended object. Before application calls
 * @ref OD_extensionIO_init() it can read original OD object, which can contain
 * initial values for the data. And also, as any data from OD, data can be
 * loaded from or saved to nonvolatile storage.
 *
 * @warning
 * Read and write functions may be called from different threads, so critical
 * sections in custom functions must be protected with @ref CO_LOCK_OD() and
 * @ref CO_UNLOCK_OD().
 *
 * See also warning in @ref OD_getSub() function.
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param object Object, which will be passed to read or write function, must
 *               not be NULL.
 * @param read Read function pointer, see @ref OD_subEntry_t.
 * @param write Write function pointer, see @ref OD_subEntry_t.
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
 * Helper function for no read access
 *
 * This function can be used by application as argument to
 * @ref OD_extensionIO_init(). It only returns ODR_WRITEONLY as returnCode.
 */
OD_size_t OD_readDisabled(OD_stream_t *stream, uint8_t subIndex,
                          void *buf, OD_size_t count,
                          ODR_t *returnCode);

/**
 * Helper function for no write access
 *
 * This function can be used by application as argument to
 * @ref OD_extensionIO_init(). It only returns ODR_READONLY as returnCode.
 */
OD_size_t OD_writeDisabled(OD_stream_t *stream, uint8_t subIndex,
                           void *buf, OD_size_t count,
                           ODR_t *returnCode);


/**
 * Update storage group data from OD object with IO extension.
 *
 * This function must be called, before OD variables from specified storageGroup
 * will be save to non-volatile memory. This function must be called, because
 * some OD objects have IO extension enabled. And those OD object are connected
 * with application code, which have own control over the entire OD object data.
 * Application does not use original data from the storageGroup. For that reason
 * this function scans entire object dictionary, reads data from necessary
 * OD objects and copies them to the original storageGroup.
 *
 * @param od Object Dictionary
 * @param storageGroup Group of data to update.
 */
void OD_updateStorageGroup(OD_t *od, uint8_t storageGroup);


/**
 * @defgroup CO_ODgetSetters Getters and setters
 * @{
 *
 * Getter and setter helpre functions for accessing different types of Object
 * Dictionary variables.
 */
/**
 * Get int8_t variable from Object Dictionary
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param [out] val Value will be written there.
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success.
 */
ODR_t OD_get_i8(const OD_entry_t *entry, uint16_t subIndex, int8_t *val);
/** Get int16_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_i16(const OD_entry_t *entry, uint16_t subIndex, int16_t *val);
/** Get int32_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_i32(const OD_entry_t *entry, uint16_t subIndex, int32_t *val);
/** Get int64_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_i64(const OD_entry_t *entry, uint16_t subIndex, int64_t *val);
/** Get uint8_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u8(const OD_entry_t *entry, uint16_t subIndex, uint8_t *val);
/** Get uint16_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u16(const OD_entry_t *entry, uint16_t subIndex, uint16_t *val);
/** Get uint32_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u32(const OD_entry_t *entry, uint16_t subIndex, uint32_t *val);
/** Get uint64_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_u64(const OD_entry_t *entry, uint16_t subIndex, uint64_t *val);
/** Get float32_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_r32(const OD_entry_t *entry, uint16_t subIndex, float32_t *val);
/** Get float64_t variable from Object Dictionary, see @ref OD_get_i8 */
ODR_t OD_get_r64(const OD_entry_t *entry, uint16_t subIndex, float64_t *val);

/**
 * Set int8_t variable in Object Dictionary
 *
 * @param entry OD entry returned by @ref OD_find().
 * @param subIndex Sub-index of the variable from the OD object.
 * @param val Value to write.
 *
 * @return Value from @ref ODR_t, "ODR_OK" in case of success.
 */
ODR_t OD_set_i8(const OD_entry_t *entry, uint16_t subIndex, int8_t val);
/** Set int16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_i16(const OD_entry_t *entry, uint16_t subIndex, int16_t val);
/** Set int16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_i32(const OD_entry_t *entry, uint16_t subIndex, int32_t val);
/** Set int16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_i64(const OD_entry_t *entry, uint16_t subIndex, int64_t val);
/** Set uint8_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u8(const OD_entry_t *entry, uint16_t subIndex, uint8_t val);
/** Set uint16_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u16(const OD_entry_t *entry, uint16_t subIndex, uint16_t val);
/** Set uint32_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u32(const OD_entry_t *entry, uint16_t subIndex, uint32_t val);
/** Set uint64_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_u64(const OD_entry_t *entry, uint16_t subIndex, uint64_t val);
/** Set float32_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_r32(const OD_entry_t *entry, uint16_t subIndex, float32_t val);
/** Set float64_t variable in Object Dictionary, see @ref OD_set_i8 */
ODR_t OD_set_r64(const OD_entry_t *entry, uint16_t subIndex, float64_t val);
/** @} */ /* CO_ODgetSetters */


/**
 * @defgroup CO_ODdefinition OD definition objects
 * @{
 *
 * Types and functions used only for definition of Object Dictionary
 */
#if defined OD_DEFINITION || defined CO_DOXYGEN

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

#endif /* defined OD_DEFINITION */

/** @} */ /* CO_ODdefinition */


/** @} */

#ifdef __cplusplus
}
#endif /*__cplusplus*/

#endif /* CO_OD_INTERFACE_H */
