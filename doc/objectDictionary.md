Object Dictionary
=================

Definitions from CiA 301 {#definitions-from-cia-301}
----------------------------------------------------
The **Object Dictionary** is a collection of all the data items which have an influence on the behavior of the application objects, the communication objects and the state machine used on this device. It serves as an interface between the communication and the application.
The object dictionary is essentially a grouping of objects accessible via the network in an ordered pre-defined fashion. Each object within the object dictionary is addressed using a 16-bit index and a 8-bit sub-index.

A **SDO** (Service Data Object) is providing direct access to object entries of a CANopen device's object dictionary. As these object entries may contain data of arbitrary size and data type. SDOs may be used to transfer multiple data sets (each containing an arbitrary large block of data) from a client to a server and vice versa. The client shall control via a multiplexer (index and sub-index of the object dictionary) which data set shall be transferred. The content of the data set is defined within the object dictionary.

A **PDO** (Process Data Object) is providing real-time data transfer of object entries of a CANopen device's object dictionary. The transfer of PDO is performed with no protocol overhead. The PDO correspond to objects in the object dictionary and provide the interface to the application objects. Data type and mapping of application objects into a PDO is determined by a corresponding PDO mapping structure within the object dictionary.


Operation {#operation}
----------------------
### Terms
The term **OD object** means object from object dictionary located at specific 16-bit index. There are different types of OD objects in CANopen: variables, arrays and records (structures). Each OD object contains pointer to actual data, data length(s) and attribute(s). See @ref OD_objectTypes_t.

The term **OD variable** is basic variable of specified type. For example: int8_t, uint32_t, float64_t, ... or just sequence of binary data with known or unknown data length. Each OD variable resides in Object dictionary at specified 16-bit index and 8-bit sub-index.

The term **OD entry** means structure element, which contains some basic properties of the OD object, indication of type of OD object and pointer to all necessary data for the OD object. An array of OD entries together with information about total number of OD entries represents object dictionary as defined inside CANopenNode. See @ref OD_entry_t and @ref OD_t.

### Access
Application and the stack have access to OD objects via universal @ref OD_t object and @ref OD_find() function. No direct access to custom structures, which define object dictionary, is required. Properties for specific OD variable is fetched with @ref OD_getSub() function. Access to actual variable is via **read** and **write** functions. Pointer to those two functions is fetched by @ref OD_getSub(). See @ref OD_stream_t. See also shortcuts: @ref CO_ODgetSetters, for access to data of different type.

Note that OD variables can be accessed from different threads. CANopenNode basically runs in two threads: fast real-time (PDO processing, etc.) and time non-critical mainline (SDO etc.). Both threads have access to OD variables, so care must be taken into account. CANopenNode uses locking mechanism, where SDO server prevents execution of the real-time thread at the moment it reads or writes OD variable. The same protection of the OD variables is necessary in @ref CO_storage. For more information see @ref CO_critical_sections in CO_driver.h.

### Example usage
```c
extern OD_t *ODxyz;

void myFunc(OD_t *od) {
    ODR_t odRet;
    OD_entry_t *entry;
    OD_IO_t io1008;
    char buf[50];
    OD_size_t bytesRd;
    int error = 0;

    /* Init IO for "Manufacturer device name" at index 0x1008, sub-index 0x00 */
    entry = OD_find(od, 0x1008);
    odRet = OD_getSub(entry, 0x00, &io1008, false);
    /* Read with io1008, subindex = 0x00 */
    if (odRet == ODR_OK) {
        /* Locking is necessary from mainline thread, but must not be used from
         * timer interval (real-time) thread. Locking is not necessary in the
         * CANoopen initialization section. Locking is also not necessary, if
         * OD variable is not mappable to PDO and not accessed from RT thread.*/
        CO_LOCK_OD(CANmodule);
        odRet = io1008.read(&io1008.stream, &buf[0], sizeof(buf), &bytesRd);
        CO_UNLOCK_OD(CANmodule);
    }
    if (odRet != ODR_OK) error++;

    /* Use helper and set "Producer heartbeat time" at index 0x1017, sub 0x00 */
    CO_LOCK_OD(CANmodule); /* may not be necessary, see comment above */
    odRet = OD_set_u16(OD_find(od, 0x1017), 0x00, 500, false);
    CO_UNLOCK_OD(CANmodule);
    if (odRet != ODR_OK) error++;
}
```
There is no need to include ODxyt.h file, it is only necessary to know, we have ODxyz defined somewhere.

Second example is simpler and use helper function to access OD variable. However it is not very efficient, because it goes through all search procedures.

If access to the same variable is very frequent, it is better to use first example. After initialization, application has to remember only "io1008" object. Frequent reading of the variable is then very efficient.

### Simple access to OD via globals
Some simple user applications can also access some OD variables directly via globals.

@warning
If OD object has OD extension enabled, then direct access to its OD variables must not be used. Only valid access is via read or write or helper functions.

```c
#include ODxyz.h

void myFuncGlob(void) {
    //Direct address instead of OD_find()
    OD_entry_t *entry_errReg = ODxyz_1001_errorRegister;

    //Direct access to OD variable
    uint32_t devType = ODxyz_0.x1000_deviceType;
    ODxyz_0.x1018_identity.serialNumber = 0x12345678;
}
```


Object Dictionary Example {#object-dictionary-example}
------------------------------------------------------
Actual Object dictionary for one CANopen device is defined by pair of OD_xyz.h and ODxyz.c files.

Suffix "xyz" is unique name of the object dictionary. If single default object dictionary is used, suffix is omitted. Such way configuration with multiple object dictionaries is possible.

Data for OD definition are arranged inside multiple structures. Structures are different for different configuration of OD. Data objects, created with those structures, are constant or are variable.

Actual OD variables are arranged inside multiple structures, so called storage groups. Selected groups can optionally be stored to non-volatile memory.

@warning
Manual editing of ODxyz.h/.c files is very error-prone.

Pair of ODxyz.h/.c files can be generated by OD editor tool. The tool can edit standard CANopen device description file in xml format. Xml file may include also some non-standard elements, specific to CANopenNode. Xml file is then used for automatic generation of ODxyz.h/.c files.

### Example ODxyz.h file
```c
/* OD data declaration of all groups ******************************************/
typedef struct {
    uint32_t x1000_deviceType;
    struct {
        uint8_t maxSubIndex;
        uint32_t vendorID;
        uint32_t productCode;
        uint32_t revisionNumber;
        uint32_t serialNumber;
    } x1018_identity;
} ODxyz_PERSIST_COMM_t;

typedef struct {
    uint8_t x1001_errorRegister;
    uint8_t x1003_preDefinedErrorField_sub0;
    uint32_t x1003_preDefinedErrorField[8];
} ODxyz_RAM_t;

extern ODxyz_PERSIST_COMM_t ODxyz_PERSIST_COMM;
extern ODxyz_RAM_t ODxyz_RAM;
extern OD_t *ODxyz;

/* Object dictionary entries - shortcuts **************************************/
#define ODxyz_ENTRY_H1000 &ODxyz->list[0]
#define ODxyz_ENTRY_H1001 &ODxyz->list[1]
#define ODxyz_ENTRY_H1003 &ODxyz->list[2]
#define ODxyz_ENTRY_H1018 &ODxyz->list[3]

#define ODxyz_ENTRY_H1000_deviceType &ODxyz->list[0]
#define ODxyz_ENTRY_H1001_errorRegister &ODxyz->list[1]
#define ODxyz_ENTRY_H1003_preDefinedErrorField &ODxyz->list[2]
#define ODxyz_ENTRY_H1018_identity &ODxyz->list[3]
```

### Example ODxyz.c file
```c
#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "ODxyz.h"

/* OD data initialization of all groups ***************************************/
ODxyz_PERSIST_COMM_t ODxyz_PERSIST_COMM = {
    .x1000_deviceType = 0,
    .x1018_identity = {
        .maxSubIndex = 4,
        .vendorID = 0,
        .productCode = 0,
        .revisionNumber = 0,
        .serialNumber = 0
    }
};

ODxyz_RAM_t ODxyz_RAM = {
    .x1001_errorRegister = 0,
    .x1003_preDefinedErrorField_sub0 = 0,
    .x1003_preDefinedErrorField = {0, 0, 0, 0, 0, 0, 0, 0}
};

/* All OD objects (constant) **************************************************/
typedef struct {
    OD_obj_var_t o_1000_deviceType;
    OD_obj_var_t o_1001_errorRegister;
    OD_obj_array_t o_1003_preDefinedErrorField;
    OD_obj_record_t o_1018_identity[5];
} ODxyzObjs_t;

static CO_PROGMEM ODxyzObjs_t ODxyzObjs = {
    .o_1000_deviceType = {
        .dataOrig = &ODxyz_PERSIST_COMM.x1000_deviceType,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    .o_1001_errorRegister = {
        .dataOrig = &ODxyz_RAM.x1001_errorRegister,
        .attribute = ODA_SDO_R,
        .dataLength = 1
    },
    .o_1003_preDefinedErrorField = {
        .dataOrig0 = &ODxyz_RAM.x1003_preDefinedErrorField_sub0,
        .dataOrig = &ODxyz_RAM.x1003_preDefinedErrorField[0],
        .attribute0 = ODA_SDO_RW,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    .o_1018_identity = {
        {
            .data = &ODxyz_PERSIST_COMM.x1018_identity.maxSubIndex,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &ODxyz_PERSIST_COMM.x1018_identity.vendorID,
            .subIndex = 1,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &ODxyz_PERSIST_COMM.x1018_identity.productCode,
            .subIndex = 2,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &ODxyz_PERSIST_COMM.x1018_identity.revisionNumber,
            .subIndex = 3,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &ODxyz_PERSIST_COMM.x1018_identity.serialNumber,
            .subIndex = 4,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        }
    }
};

/* Object dictionary **********************************************************/
static OD_entry_t ODxyzList[] = {
    {0x1000, 0x01, ODT_VAR, &ODxyzObjs.o_1000_deviceType, NULL},
    {0x1001, 0x01, ODT_VAR, &ODxyzObjs.o_1001_errorRegister, NULL},
    {0x1003, 0x09, ODT_VAR, &ODxyzObjs.o_1003_preDefinedErrorField, NULL},
    {0x1018, 0x05, ODT_REC, &ODxyzObjs.o_1018_identity, NULL},
    {0x0000, 0x00, 0, NULL, NULL}
};

OD_t _ODxyz = {
    (sizeof(ODxyzList) / sizeof(ODxyzList[0])) - 1,
    &ODxyzList[0]
};

OD_t *ODxyz = &_ODxyz;
```


XML Device Description {#xml-device-description}
------------------------------------------------
CANopen device description - XML schema definition - is specified by CiA 311 standard.

CiA 311 complies with standard ISO 15745-1:2005/Amd1 (Industrial automation systems and integration - Open systems application integration framework).

CANopen device description is basically a XML file with all the information about CANopen device. The larges part of the file is a list of all object dictionary variables with all necessary properties and documentation. This file can be edited with OD editor application and can be used as data source, from which Object dictionary for CANopenNode is generated. Furthermore, this file can be used with CANopen configuration tool, which interacts with CANopen devices on running CANopen network.

XML schema definitions are available at: http://www.canopen.org/xml/1.1 One of the tools for viewing XML schemas and validating XDD project files is "xsddiagram" (https://github.com/dgis/xsddiagram). Command line alternative to XDD file validation against schema is: `xmlstarlet val --err --xsd 311/CANopen.xsd project_file.xdd`

CANopen specifies also another type of files for CANopen device description. These are EDS files, which are in INI format. It is possible to convert between those two formats. But CANopenNode uses XML format.

The device description file has "XDD" file extension. The name of this file shall contain the vendor-ID of the CANopen device in the form of 8 hexadecimal digits in any position of the name and separated with underscores. For example "name1_12345678_name2.XDD".

CANopenNode includes multiple profile definition files, one for each CANopen object. Those files have "XPD" extension. They are in the same XML format as XDD files. The XML editor tool can use XPD files to insert prepared data into device description file (XDD), which is being edited.

There are also device configuration files with "XDC" extension. They are describing a configured CANopen device and include additional elements, such as default value, denominator and device commissioning elements. Similar as "dcf" files in INI format.

### XDD, XPD file example
```xml
<?xml version="1.0" encoding="utf-8"?>
<ISO15745ProfileContainer xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns="http://www.canopen.org/xml/1.1">
  <ISO15745Profile>
    <ProfileHeader>...</ProfileHeader>
    <ProfileBody xsi:type="ProfileBody_Device_CANopen" fileName="..." fileCreator="..." fileCreationDate="..." fileVersion="...">
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
            <property name="CO_storageGroup" value="RAM">
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
      </ApplicationProcess>
    </ProfileBody>
  </ISO15745Profile>
  <ISO15745Profile>
    <ProfileHeader>...</ProfileHeader>
    <ProfileBody xsi:type="ProfileBody_CommunicationNetwork_CANopen" fileName="..." fileCreator="..." fileCreationDate="..." fileVersion="...">
      <ApplicationLayers>
        <CANopenObjectList>
          <CANopenObject index="1000" name="Device type" objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_1000" />
          <CANopenObject index="1001" name="Error register" objectType="7" PDOmapping="TPDO" uniqueIDRef="UID_PARAM_1001" />
          <CANopenObject index="1018" name="Identity" objectType="9" subNumber="5" uniqueIDRef="UID_PARAM_1018">
            <CANopenSubObject subIndex="00" name="max sub-index" objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101800" />
            <CANopenSubObject subIndex="01" name="Vendor-ID" objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101801" />
            <CANopenSubObject subIndex="02" name="Product code" objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101802" />
            <CANopenSubObject subIndex="03" name="Revision number" objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101803" />
            <CANopenSubObject subIndex="04" name="Serial number" objectType="7" PDOmapping="no" uniqueIDRef="UID_PARAM_101804" />
          </CANopenObject>
        </CANopenObjectList>
      </ApplicationLayers>
      <TransportLayers>...</TransportLayers>
    </ProfileBody>
  </ISO15745Profile>
</ISO15745ProfileContainer>
```

### Parameter description
Above XML file example shows necessary data for OD interface used by CANopenNode and other parameters required by the standard. Standard specifies many other parameters, which are not used by CANopenNode for simplicity.

XML file is divided into two parts:
  1. "ProfileBody_Device_CANopen" - more standardized information
  2. "ProfileBody_CommunicationNetwork_CANopen" - CANopen specific information

Largest part of the XML file is definition of each OD object. All OD objects are listed in "CANopenObjectList", which resides in the second part of the XML file. Each "CANopenObject" and "CANopenSubObject" of the list contains a link to parameter ("uniqueIDRef"), which resides in the first part of the XML file. So data for each OD object is split between two parts of the XML file.

#### &lt;CANopenObject&gt;
  * "index" (required) - Object dictionary index
  * "name" (required) - Name of the parameter
  * "objectType" (required) - "7"=VAR, "8"=ARRAY, "9"=RECORD
  * "subNumber" (required if "objectType" is "8" or "9")
  * "PDOmapping" (optional if "objectType" is "7"):
    * "no" - mapping not allowed
    * "default" - not used, same as "optional"
    * "optional" - mapping allowed to TPDO or RPDO
    * "TPDO" - mapping allowed to TPDO
    * "RPDO" mapping allowed to RPDO
  * "uniqueIDRef" (required or see below) - Reference to &lt;parameter&gt;

#### &lt;CANopenSubObject&gt;
  * "subIndex" (required) - Object dictionary sub-index
  * "name" (required) - Name of the parameter
  * "objectType" (required, always "7")
  * "PDOmapping" (optional, same as above)
  * "uniqueIDRef" (required or see below) - Reference to &lt;parameter&gt;

#### uniqueIDRef
This is required attribute from "CANopenObject" and "CANopenSubObject". It contains reference to &lt;parameter&gt; in "ProfileBody_Device_CANopen" section of the XML file.

If "uniqueIDRef" attribute is not specified and "objectType" is 7(VAR), then "CANopenObject" or "CANopenSubObject" must contain additional attributes:
  * "dataType" (required for VAR) - CANopen basic data type, see below
  * "accessType" (required for VAR) - "ro", "wo", "rw" or "const"
  * "defaultValue" (optional) - Default value for the variable.

#### &lt;parameter&gt;
  * &quot;**uniqueID**&quot; (required)
  * &quot;**access**&quot; (required for VAR) - can be one of:
    * "const" - same as "read"
    * "read" - only read access with SDO or PDO
    * "write" - only write access with SDO or PDO
    * "readWrite" - read or write access with SDO or PDO
    * "readWriteInput" - same as "readWrite"
    * "readWriteOutput" - same as "readWrite"
    * "noAccess" - object will be in object dictionary, but no access.
  * &lt;**label** lang="en"&gt;, &lt;**description** lang="en"&gt; (required) - several elements in multiple languages possible
  * &lt;**INT and similar**/&gt; (required) - Basic or complex data type. Basic data type (for VAR) is specified in IEC 61131-3 (see below). If data type is complex (ARRAY or RECORD), then **&lt;dataTypeIDRef&gt;** must be specified and entry must be added in the **&lt;dataTypeList&gt;**. Such definition of complex data types is required by the standard, but it is not required by CANopenNode.
  * &lt;**defaultValue**&gt; (optional for VAR) - Default value for the variable. If it is empty string, then data is not stored inside object dictionary. Application should provide own data via OD extension.
  * &lt;**property** name="..." value="..."&gt; (optional) - Multiple elements may contain additional custom properties. CANopenNode uses custom properties with following names:
    * &quot;**CO_disabled**&quot; (used for base OD entry) - Valid value is "false" (default) or "true". if OD entry is disabled, then it will not be present in generated Object Dictionary .h and .c files.
    * &quot;**CO_countLabel**&quot; (used for base OD entry) - Valid value is any string without spaces. OD exporter will generate a macro for each different string. For example, if four OD objects have "CO_countLabel" set to "TPDO", then macro "#define ODxyz_CNT_TPDO 4" will be generated by OD exporter. "CO_countLabel" is not required for configuration with multiple object dictionaries (CO_MULTIPLE_OD), although may be useful. Default is "".
    * &quot;**CO_storageGroup**&quot; (used for base OD entry) - group name (string) into which the C variable will belong. Variables from specific storage group may then be stored into non-volatile memory, automatically or by SDO command. Default is "RAM". Please note: if &lt;defaultValue&gt; is empty string at specific subindex, then no storage group will be used for that variable.
    * &quot;**CO_accessSRDO**&quot; (used for each "VAR") - Valid values are: "tx", "rx", "trx", "no"(default).
    * &quot;**CO_stringLengthMin**&quot; (used for each "VAR") - Minimum length of the string. Used with "VISIBLE_STRING" and "UNICODE_STRING". If CO_stringLengthMin is smaller than length of the string in &lt;defaultValue&gt;, then it is ignored. Byte length of unicode string is 2 * CO_stringLengthMin. If &lt;defaultValue&gt; is empty and CO_stringLengthMin is 0, then data is not stored inside object dictionary. Default is 0.

#### Other elements
Other elements listed in the above XML example are required by the standard. There are also many other elements, not listed above. All those does not influence the CANopenNode object dictionary exporter.


### CANopen basic data types
| CANopenNode    | IEC 61131-3    | CANopen         | dataType |
| -------------- | -------------- | --------------- | -------- |
| bool_t         | BOOL           | BOOLEAN         | 0x01     |
| int8_t         | SINT, (CHAR)   | INTEGER8        | 0x02     |
| int16_t        | INT            | INTEGER16       | 0x03     |
| int32_t        | DINT           | INTEGER32       | 0x04     |
| int64_t        | LINT           | INTEGER64       | 0x15     |
| uint8_t        | USINT, (BYTE)  | UNSIGNED8       | 0x05     |
| uint16_t       | UINT, (WORD)   | UNSIGNED16      | 0x06     |
| uint32_t       | UDINT, (DWORD) | UNSIGNED32      | 0x07     |
| uint64_t       | ULINT, (LWORD) | UNSIGNED64      | 0x1B     |
| float32_t      | REAL           | REAL32          | 0x08     |
| float64_t      | LREAL          | REAL64          | 0x11     |
| -              | (1)            | INTEGER24       | 0x10     |
| -              | (1)            | INTEGER40       | 0x12     |
| -              | (1)            | INTEGER48       | 0x13     |
| -              | (1)            | INTEGER56       | 0x14     |
| -              | (1)            | UNSIGNED24      | 0x16     |
| -              | (1)            | UNSIGNED40      | 0x18     |
| -              | (1)            | UNSIGNED48      | 0x19     |
| -              | (1)            | UNSIGNED56      | 0x1A     |
| char []        | STRING (2)     | VISIBLE_STRING  | 0x09     |
| uint8_t []     | BITSTRING (3)  | OCTET_STRING    | 0x0A     |
| uint16_t []    | WSTRING (2)    | UNICODE_STRING  | 0x0B     |
| -              | (1)            | TIME_OF_DAY     | 0x0C     |
| -              | (1)            | TIME_DIFFERENCE | 0x0D     |
| app. specific  | BITSTRING (4)  | DOMAIN          | 0x0F     |

(1) Data is translated into OCTET_STRING.

(2) Additional property "CO_stringLengthMin" indicates the minimum length of the string in CANopenNode. Property is ignored, if strlen(defaultValue) is larger. Otherwise remaining data locations are filled with zeroes. Strings always have additional element on the end of the array with value=0, which is never written (string is always null terminated). All strings have additional attribute "ODA_STR" in object dictionary. Attribute enables SDO to transfer data of length, which corresponds to actual string length. (Actual size of data read or written by SDO may be smaller than data size of the OD variable.) VISIBLE_STRING may contain control characters and UTF-8 characters, which should work on most target systems (like printf("%s", ...)). So VISIBLE_STRING may be more usable for printing complete set of unicode characters than UNICODE_STRING.

(3) Default value for BITSTRING (OCTET_STRING) is written as space separated, two hex digit bytes in little-endian format.

(4) Default value for DOMAIN is stored as empty string.


Object Dictionary Requirements By CANopenNode {#object-dictionary-requirements-by-canopennode}
----------------------------------------------------------------------------------------------
* **Used by** column indicates CANopenNode object or its part, which uses the OD object. It also indicates, if OD object is required or optional for actual configuration. For the configuration of the CANopenNode objects see [Stack configuration](301/CO_config.h). If CANopenNode object or its part is disabled in stack configuration, then OD object is not used. Note that OD objects: 1000, 1001 and 1017 and 1018 are mandatory for CANopen.
* **CO_countLabel** column indicates, which value must have property "CO_countLabel" inside OD object.

| index | Description                   | Used by              | CO_countLabel |
| ----- | ----------------------------- | ---------------------| ------------- |
| 1000  | Device type                   | CANopen, req         | NMT           |
| 1001  | Error register                | CANopen, EM, req     | EM            |
| 1002  | Manufacturer status register  |                      |               |
| 1003  | Pre-defined error field       | EM_HISTORY, opt      |               |
| 1005  | COB-ID SYNC message           | SYNC, req            | SYNC          |
| 1006  | Communication cycle period    | SYNC_PRODUCER, req   | SYNC_PROD     |
| 1007  | Synchronous window length     | SYNC, opt            |               |
| 1008  | Manufacturer device name      |                      |               |
| 1009  | Manufacturer hardware version |                      |               |
| 100A  | Manufacturer software version |                      |               |
| 100C  | Guard time                    |                      |               |
| 100D  | Life time factor              |                      |               |
| 1010  | Store parameters              |                      | STORAGE       |
| 1011  | Restore default parameters    |                      |               |
| 1012  | COB-ID time stamp object      | TIME, req            | TIME          |
| 1013  | High resolution time stamp    |                      |               |
| 1014  | COB-ID EMCY                   | EM_PRODUCER, req     | EM_PROD       |
| 1015  | Inhibit time EMCY             | EM_PROD_INHIBIT, opt |               |
| 1016  | Consumer heartbeat time       | HB_CONS, req         | HB_CONS       |
| 1017  | Producer heartbeat time       | CANopen, NMT, req    | HB_PROD       |
| 1018  | Identity object               | CANopen, LSS_SL, req |               |
| 1019  | Synch. counter overflow value | SYNC, opt            |               |
| 1020  | Verify configuration          |                      |               |
| 1021  | Store EDS                     |                      |               |
| 1022  | Store format                  |                      |               |
| 1023  | OS command                    |                      |               |
| 1024  | OS command mode               |                      |               |
| 1025  | OS debugger interface         |                      |               |
| 1026  | OS prompt                     |                      |               |
| 1027  | Module list                   |                      |               |
| 1028  | Emergency consumer object     |                      |               |
| 1029  | Error behavior object         |                      |               |
| 1200  | SDO server parameter (first)  | SDO optional         | SDO_SRV       |
| 1201+ | SDO server parameter          | SDO+, req            | SDO_SRV       |
| 1280+ | SDO client parameter          | SDO_CLI, req         | SDO_CLI       |
| 1300  | Global fail-safe command par  | GFC, req             | GFC           |
| 1301+ | SRDO communication parameter  | SRDO, req            | SRDO          |
| 1381+ | SRDO mapping parameter        | SRDO, req            |               |
| 13FE  | Configuration valid           | SRDO, req            |               |
| 13FF  | Safety configuration checksum | SRDO, req            |               |
| 1400+ | RPDO communication parameter  | RPDO, req            | RPDO          |
| 1600+ | RPDO mapping parameter        | RPDO, req            |               |
| 1800+ | TPDO communication parameter  | TPDO, req            | TPDO          |
| 1A00+ | TPDO mapping parameter        | TPDO, req            |               |
| 1FA0+ | Object scanner list           |                      |               |
| 1FD0+ | Object dispatching list       |                      |               |
|       | Custom objects                |                      |               |
| any   | Error status bits             | EM_STATUS_BITS, opt  |               |
| any+  | Trace                         | TRACE, req           | TRACE         |
