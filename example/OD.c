/*******************************************************************************
    CANopen Object Dictionary definition for CANopenNode V4

    This file was automatically generated with
    libedssharp Object Dictionary Editor v0.8-99-g0425f94

    https://github.com/CANopenNode/CANopenNode
    https://github.com/robincornelius/libedssharp

    DON'T EDIT THIS FILE MANUALLY, UNLESS YOU KNOW WHAT YOU ARE DOING !!!!
*******************************************************************************/

#define OD_DEFINITION
#include "301/CO_ODinterface.h"
#include "OD.h"

#if CO_VERSION_MAJOR < 4
#error This Object dictionary is compatible with CANopenNode V4.0 and above!
#endif

/*******************************************************************************
    OD data initialization of all groups
*******************************************************************************/
OD_PERSIST_COMM_t OD_PERSIST_COMM = {
    .x1000_deviceType = 0x00000000,
    .x1005_COB_ID_SYNCMessage = 0x00000080,
    .x1006_communicationCyclePeriod = 0x00000000,
    .x1007_synchronousWindowLength = 0x00000000,
    .x1012_COB_IDTimeStampObject = 0x00000100,
    .x1014_COB_ID_EMCY = 0x00000080,
    .x1015_inhibitTimeEMCY = 0x0000,
    .x1016_consumerHeartbeatTime_sub0 = 0x08,
    .x1016_consumerHeartbeatTime = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    .x1017_producerHeartbeatTime = 0x0000,
    .x1018_identity = {
        .highestSub_indexSupported = 0x04,
        .vendor_ID = 0x00000000,
        .productCode = 0x00000000,
        .revisionNumber = 0x00000000,
        .serialNumber = 0x00000000
    },
    .x1019_synchronousCounterOverflowValue = 0x00,
    .x1280_SDOClientParameter = {
        .highestSub_indexSupported = 0x03,
        .COB_IDClientToServerTx = 0x80000000,
        .COB_IDServerToClientRx = 0x80000000,
        .node_IDOfTheSDOServer = 0x01
    },
    .x1400_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x80000200,
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1401_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x80000300,
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1402_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x80000400,
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1403_RPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x05,
        .COB_IDUsedByRPDO = 0x80000500,
        .transmissionType = 0xFE,
        .eventTimer = 0x0000
    },
    .x1600_RPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1601_RPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1602_RPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1603_RPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1800_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0xC0000180,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer = 0x0000,
        .SYNCStartValue = 0x00
    },
    .x1801_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0xC0000280,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer = 0x0000,
        .SYNCStartValue = 0x00
    },
    .x1802_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0xC0000380,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer = 0x0000,
        .SYNCStartValue = 0x00
    },
    .x1803_TPDOCommunicationParameter = {
        .highestSub_indexSupported = 0x06,
        .COB_IDUsedByTPDO = 0xC0000480,
        .transmissionType = 0xFE,
        .inhibitTime = 0x0000,
        .eventTimer = 0x0000,
        .SYNCStartValue = 0x00
    },
    .x1A00_TPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1A01_TPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1A02_TPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    },
    .x1A03_TPDOMappingParameter = {
        .numberOfMappedApplicationObjectsInPDO = 0x00,
        .applicationObject_1 = 0x00000000,
        .applicationObject_2 = 0x00000000,
        .applicationObject_3 = 0x00000000,
        .applicationObject_4 = 0x00000000,
        .applicationObject_5 = 0x00000000,
        .applicationObject_6 = 0x00000000,
        .applicationObject_7 = 0x00000000,
        .applicationObject_8 = 0x00000000
    }
};

OD_RAM_t OD_RAM = {
    .x1001_errorRegister = 0x00,
    .x1003_pre_definedErrorField_sub0 = 0x00,
    .x1003_pre_definedErrorField = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000},
    .x1010_storeParameters_sub0 = 0x04,
    .x1010_storeParameters = {0x00000001, 0x00000001, 0x00000001, 0x00000001},
    .x1011_restoreDefaultParameters_sub0 = 0x04,
    .x1011_restoreDefaultParameters = {0x00000001, 0x00000001, 0x00000001, 0x00000001},
    .x1200_SDOServerParameter = {
        .highestSub_indexSupported = 0x02,
        .COB_IDClientToServerRx = 0x00000600,
        .COB_IDServerToClientTx = 0x00000580
    }
};


/*******************************************************************************
    IO extensions and flagsPDO (configurable by application)
*******************************************************************************/
typedef struct {
    OD_extensionIO_t xio_1003_pre_definedErrorField;
    OD_extensionIO_t xio_1005_COB_ID_SYNCMessage;
    OD_extensionIO_t xio_1006_communicationCyclePeriod;
    OD_extensionIO_t xio_1007_synchronousWindowLength;
    OD_extensionIO_t xio_1010_storeParameters;
    OD_extensionIO_t xio_1011_restoreDefaultParameters;
    OD_extensionIO_t xio_1012_COB_IDTimeStampObject;
    OD_extensionIO_t xio_1014_COB_ID_EMCY;
    OD_extensionIO_t xio_1015_inhibitTimeEMCY;
    OD_extensionIO_t xio_1016_consumerHeartbeatTime;
    OD_extensionIO_t xio_1017_producerHeartbeatTime;
    OD_extensionIO_t xio_1200_SDOServerParameter;
    OD_extensionIO_t xio_1280_SDOClientParameter;
    OD_extensionIO_t xio_1400_RPDOCommunicationParameter;
    OD_extensionIO_t xio_1401_RPDOCommunicationParameter;
    OD_extensionIO_t xio_1402_RPDOCommunicationParameter;
    OD_extensionIO_t xio_1403_RPDOCommunicationParameter;
    OD_extensionIO_t xio_1600_RPDOMappingParameter;
    OD_extensionIO_t xio_1601_RPDOMappingParameter;
    OD_extensionIO_t xio_1602_RPDOMappingParameter;
    OD_extensionIO_t xio_1603_RPDOMappingParameter;
    OD_extensionIO_t xio_1800_TPDOCommunicationParameter;
    OD_extensionIO_t xio_1801_TPDOCommunicationParameter;
    OD_extensionIO_t xio_1802_TPDOCommunicationParameter;
    OD_extensionIO_t xio_1803_TPDOCommunicationParameter;
    OD_extensionIO_t xio_1A00_TPDOMappingParameter;
    OD_extensionIO_t xio_1A01_TPDOMappingParameter;
    OD_extensionIO_t xio_1A02_TPDOMappingParameter;
    OD_extensionIO_t xio_1A03_TPDOMappingParameter;
} ODExts_t;

static ODExts_t ODExts = {0};


/*******************************************************************************
    All OD objects (const)
*******************************************************************************/
typedef struct {
    OD_obj_var_t o_1000_deviceType;
    OD_obj_var_t o_1001_errorRegister;
    OD_obj_array_t o_1003_pre_definedErrorField;
    OD_obj_extended_t oE_1003_pre_definedErrorField;
    OD_obj_var_t o_1005_COB_ID_SYNCMessage;
    OD_obj_extended_t oE_1005_COB_ID_SYNCMessage;
    OD_obj_var_t o_1006_communicationCyclePeriod;
    OD_obj_extended_t oE_1006_communicationCyclePeriod;
    OD_obj_var_t o_1007_synchronousWindowLength;
    OD_obj_extended_t oE_1007_synchronousWindowLength;
    OD_obj_array_t o_1010_storeParameters;
    OD_obj_extended_t oE_1010_storeParameters;
    OD_obj_array_t o_1011_restoreDefaultParameters;
    OD_obj_extended_t oE_1011_restoreDefaultParameters;
    OD_obj_var_t o_1012_COB_IDTimeStampObject;
    OD_obj_extended_t oE_1012_COB_IDTimeStampObject;
    OD_obj_var_t o_1014_COB_ID_EMCY;
    OD_obj_extended_t oE_1014_COB_ID_EMCY;
    OD_obj_var_t o_1015_inhibitTimeEMCY;
    OD_obj_extended_t oE_1015_inhibitTimeEMCY;
    OD_obj_array_t o_1016_consumerHeartbeatTime;
    OD_obj_extended_t oE_1016_consumerHeartbeatTime;
    OD_obj_var_t o_1017_producerHeartbeatTime;
    OD_obj_extended_t oE_1017_producerHeartbeatTime;
    OD_obj_record_t o_1018_identity[5];
    OD_obj_var_t o_1019_synchronousCounterOverflowValue;
    OD_obj_record_t o_1200_SDOServerParameter[3];
    OD_obj_extended_t oE_1200_SDOServerParameter;
    OD_obj_record_t o_1280_SDOClientParameter[4];
    OD_obj_extended_t oE_1280_SDOClientParameter;
    OD_obj_record_t o_1400_RPDOCommunicationParameter[4];
    OD_obj_extended_t oE_1400_RPDOCommunicationParameter;
    OD_obj_record_t o_1401_RPDOCommunicationParameter[4];
    OD_obj_extended_t oE_1401_RPDOCommunicationParameter;
    OD_obj_record_t o_1402_RPDOCommunicationParameter[4];
    OD_obj_extended_t oE_1402_RPDOCommunicationParameter;
    OD_obj_record_t o_1403_RPDOCommunicationParameter[4];
    OD_obj_extended_t oE_1403_RPDOCommunicationParameter;
    OD_obj_record_t o_1600_RPDOMappingParameter[9];
    OD_obj_extended_t oE_1600_RPDOMappingParameter;
    OD_obj_record_t o_1601_RPDOMappingParameter[9];
    OD_obj_extended_t oE_1601_RPDOMappingParameter;
    OD_obj_record_t o_1602_RPDOMappingParameter[9];
    OD_obj_extended_t oE_1602_RPDOMappingParameter;
    OD_obj_record_t o_1603_RPDOMappingParameter[9];
    OD_obj_extended_t oE_1603_RPDOMappingParameter;
    OD_obj_record_t o_1800_TPDOCommunicationParameter[6];
    OD_obj_extended_t oE_1800_TPDOCommunicationParameter;
    OD_obj_record_t o_1801_TPDOCommunicationParameter[6];
    OD_obj_extended_t oE_1801_TPDOCommunicationParameter;
    OD_obj_record_t o_1802_TPDOCommunicationParameter[6];
    OD_obj_extended_t oE_1802_TPDOCommunicationParameter;
    OD_obj_record_t o_1803_TPDOCommunicationParameter[6];
    OD_obj_extended_t oE_1803_TPDOCommunicationParameter;
    OD_obj_record_t o_1A00_TPDOMappingParameter[9];
    OD_obj_extended_t oE_1A00_TPDOMappingParameter;
    OD_obj_record_t o_1A01_TPDOMappingParameter[9];
    OD_obj_extended_t oE_1A01_TPDOMappingParameter;
    OD_obj_record_t o_1A02_TPDOMappingParameter[9];
    OD_obj_extended_t oE_1A02_TPDOMappingParameter;
    OD_obj_record_t o_1A03_TPDOMappingParameter[9];
    OD_obj_extended_t oE_1A03_TPDOMappingParameter;
} ODObjs_t;

static const ODObjs_t ODObjs = {
    .o_1000_deviceType = {
        .data = &OD_PERSIST_COMM.x1000_deviceType,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataLength = 4
    },
    .o_1001_errorRegister = {
        .data = &OD_RAM.x1001_errorRegister,
        .attribute = ODA_SDO_R | ODA_TRPDO,
        .dataLength = 1
    },
    .o_1003_pre_definedErrorField = {
        .data0 = &OD_RAM.x1003_pre_definedErrorField_sub0,
        .data = &OD_RAM.x1003_pre_definedErrorField[0],
        .attribute0 = ODA_SDO_RW,
        .attribute = ODA_SDO_R | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    .oE_1003_pre_definedErrorField = {
        .extIO = &ODExts.xio_1003_pre_definedErrorField,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1003_pre_definedErrorField
    },
    .o_1005_COB_ID_SYNCMessage = {
        .data = &OD_PERSIST_COMM.x1005_COB_ID_SYNCMessage,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    .oE_1005_COB_ID_SYNCMessage = {
        .extIO = &ODExts.xio_1005_COB_ID_SYNCMessage,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1005_COB_ID_SYNCMessage
    },
    .o_1006_communicationCyclePeriod = {
        .data = &OD_PERSIST_COMM.x1006_communicationCyclePeriod,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    .oE_1006_communicationCyclePeriod = {
        .extIO = &ODExts.xio_1006_communicationCyclePeriod,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1006_communicationCyclePeriod
    },
    .o_1007_synchronousWindowLength = {
        .data = &OD_PERSIST_COMM.x1007_synchronousWindowLength,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    .oE_1007_synchronousWindowLength = {
        .extIO = &ODExts.xio_1007_synchronousWindowLength,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1007_synchronousWindowLength
    },
    .o_1010_storeParameters = {
        .data0 = &OD_RAM.x1010_storeParameters_sub0,
        .data = &OD_RAM.x1010_storeParameters[0],
        .attribute0 = ODA_SDO_R,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    .oE_1010_storeParameters = {
        .extIO = &ODExts.xio_1010_storeParameters,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1010_storeParameters
    },
    .o_1011_restoreDefaultParameters = {
        .data0 = &OD_RAM.x1011_restoreDefaultParameters_sub0,
        .data = &OD_RAM.x1011_restoreDefaultParameters[0],
        .attribute0 = ODA_SDO_R,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    .oE_1011_restoreDefaultParameters = {
        .extIO = &ODExts.xio_1011_restoreDefaultParameters,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1011_restoreDefaultParameters
    },
    .o_1012_COB_IDTimeStampObject = {
        .data = &OD_PERSIST_COMM.x1012_COB_IDTimeStampObject,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    .oE_1012_COB_IDTimeStampObject = {
        .extIO = &ODExts.xio_1012_COB_IDTimeStampObject,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1012_COB_IDTimeStampObject
    },
    .o_1014_COB_ID_EMCY = {
        .data = &OD_PERSIST_COMM.x1014_COB_ID_EMCY,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 4
    },
    .oE_1014_COB_ID_EMCY = {
        .extIO = &ODExts.xio_1014_COB_ID_EMCY,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1014_COB_ID_EMCY
    },
    .o_1015_inhibitTimeEMCY = {
        .data = &OD_PERSIST_COMM.x1015_inhibitTimeEMCY,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    .oE_1015_inhibitTimeEMCY = {
        .extIO = &ODExts.xio_1015_inhibitTimeEMCY,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1015_inhibitTimeEMCY
    },
    .o_1016_consumerHeartbeatTime = {
        .data0 = &OD_PERSIST_COMM.x1016_consumerHeartbeatTime_sub0,
        .data = &OD_PERSIST_COMM.x1016_consumerHeartbeatTime[0],
        .attribute0 = ODA_SDO_R,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataElementLength = 4,
        .dataElementSizeof = sizeof(uint32_t)
    },
    .oE_1016_consumerHeartbeatTime = {
        .extIO = &ODExts.xio_1016_consumerHeartbeatTime,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1016_consumerHeartbeatTime
    },
    .o_1017_producerHeartbeatTime = {
        .data = &OD_PERSIST_COMM.x1017_producerHeartbeatTime,
        .attribute = ODA_SDO_RW | ODA_MB,
        .dataLength = 2
    },
    .oE_1017_producerHeartbeatTime = {
        .extIO = &ODExts.xio_1017_producerHeartbeatTime,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1017_producerHeartbeatTime
    },
    .o_1018_identity = {
        {
            .data = &OD_PERSIST_COMM.x1018_identity.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1018_identity.vendor_ID,
            .subIndex = 1,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1018_identity.productCode,
            .subIndex = 2,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1018_identity.revisionNumber,
            .subIndex = 3,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1018_identity.serialNumber,
            .subIndex = 4,
            .attribute = ODA_SDO_R | ODA_MB,
            .dataLength = 4
        }
    },
    .o_1019_synchronousCounterOverflowValue = {
        .data = &OD_PERSIST_COMM.x1019_synchronousCounterOverflowValue,
        .attribute = ODA_SDO_RW,
        .dataLength = 1
    },
    .o_1200_SDOServerParameter = {
        {
            .data = &OD_RAM.x1200_SDOServerParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_RAM.x1200_SDOServerParameter.COB_IDClientToServerRx,
            .subIndex = 1,
            .attribute = ODA_SDO_R | ODA_TPDO | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_RAM.x1200_SDOServerParameter.COB_IDServerToClientTx,
            .subIndex = 2,
            .attribute = ODA_SDO_R | ODA_TPDO | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1200_SDOServerParameter = {
        .extIO = &ODExts.xio_1200_SDOServerParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1200_SDOServerParameter
    },
    .o_1280_SDOClientParameter = {
        {
            .data = &OD_PERSIST_COMM.x1280_SDOClientParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1280_SDOClientParameter.COB_IDClientToServerTx,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_TRPDO | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1280_SDOClientParameter.COB_IDServerToClientRx,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_TRPDO | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1280_SDOClientParameter.node_IDOfTheSDOServer,
            .subIndex = 3,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    .oE_1280_SDOClientParameter = {
        .extIO = &ODExts.xio_1280_SDOClientParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1280_SDOClientParameter
    },
    .o_1400_RPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1400_RPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1400_RPDOCommunicationParameter.COB_IDUsedByRPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1400_RPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1400_RPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        }
    },
    .oE_1400_RPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1400_RPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1400_RPDOCommunicationParameter
    },
    .o_1401_RPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1401_RPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1401_RPDOCommunicationParameter.COB_IDUsedByRPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1401_RPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1401_RPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        }
    },
    .oE_1401_RPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1401_RPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1401_RPDOCommunicationParameter
    },
    .o_1402_RPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1402_RPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1402_RPDOCommunicationParameter.COB_IDUsedByRPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1402_RPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1402_RPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        }
    },
    .oE_1402_RPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1402_RPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1402_RPDOCommunicationParameter
    },
    .o_1403_RPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1403_RPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1403_RPDOCommunicationParameter.COB_IDUsedByRPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1403_RPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1403_RPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        }
    },
    .oE_1403_RPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1403_RPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1403_RPDOCommunicationParameter
    },
    .o_1600_RPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1600_RPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1600_RPDOMappingParameter = {
        .extIO = &ODExts.xio_1600_RPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1600_RPDOMappingParameter
    },
    .o_1601_RPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1601_RPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1601_RPDOMappingParameter = {
        .extIO = &ODExts.xio_1601_RPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1601_RPDOMappingParameter
    },
    .o_1602_RPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1602_RPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1602_RPDOMappingParameter = {
        .extIO = &ODExts.xio_1602_RPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1602_RPDOMappingParameter
    },
    .o_1603_RPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1603_RPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1603_RPDOMappingParameter = {
        .extIO = &ODExts.xio_1603_RPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1603_RPDOMappingParameter
    },
    .o_1800_TPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1800_TPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1800_TPDOCommunicationParameter.COB_IDUsedByTPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1800_TPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1800_TPDOCommunicationParameter.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1800_TPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1800_TPDOCommunicationParameter.SYNCStartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    .oE_1800_TPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1800_TPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1800_TPDOCommunicationParameter
    },
    .o_1801_TPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.COB_IDUsedByTPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1801_TPDOCommunicationParameter.SYNCStartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    .oE_1801_TPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1801_TPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1801_TPDOCommunicationParameter
    },
    .o_1802_TPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.COB_IDUsedByTPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1802_TPDOCommunicationParameter.SYNCStartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    .oE_1802_TPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1802_TPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1802_TPDOCommunicationParameter
    },
    .o_1803_TPDOCommunicationParameter = {
        {
            .data = &OD_PERSIST_COMM.x1803_TPDOCommunicationParameter.highestSub_indexSupported,
            .subIndex = 0,
            .attribute = ODA_SDO_R,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1803_TPDOCommunicationParameter.COB_IDUsedByTPDO,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1803_TPDOCommunicationParameter.transmissionType,
            .subIndex = 2,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1803_TPDOCommunicationParameter.inhibitTime,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1803_TPDOCommunicationParameter.eventTimer,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 2
        },
        {
            .data = &OD_PERSIST_COMM.x1803_TPDOCommunicationParameter.SYNCStartValue,
            .subIndex = 6,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        }
    },
    .oE_1803_TPDOCommunicationParameter = {
        .extIO = &ODExts.xio_1803_TPDOCommunicationParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1803_TPDOCommunicationParameter
    },
    .o_1A00_TPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A00_TPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1A00_TPDOMappingParameter = {
        .extIO = &ODExts.xio_1A00_TPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1A00_TPDOMappingParameter
    },
    .o_1A01_TPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A01_TPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1A01_TPDOMappingParameter = {
        .extIO = &ODExts.xio_1A01_TPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1A01_TPDOMappingParameter
    },
    .o_1A02_TPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A02_TPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1A02_TPDOMappingParameter = {
        .extIO = &ODExts.xio_1A02_TPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1A02_TPDOMappingParameter
    },
    .o_1A03_TPDOMappingParameter = {
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.numberOfMappedApplicationObjectsInPDO,
            .subIndex = 0,
            .attribute = ODA_SDO_RW,
            .dataLength = 1
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_1,
            .subIndex = 1,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_2,
            .subIndex = 2,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_3,
            .subIndex = 3,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_4,
            .subIndex = 4,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_5,
            .subIndex = 5,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_6,
            .subIndex = 6,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_7,
            .subIndex = 7,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        },
        {
            .data = &OD_PERSIST_COMM.x1A03_TPDOMappingParameter.applicationObject_8,
            .subIndex = 8,
            .attribute = ODA_SDO_RW | ODA_MB,
            .dataLength = 4
        }
    },
    .oE_1A03_TPDOMappingParameter = {
        .extIO = &ODExts.xio_1A03_TPDOMappingParameter,
        .flagsPDO = NULL,
        .odObjectOriginal = &ODObjs.o_1A03_TPDOMappingParameter
    }
};


/*******************************************************************************
    Object dictionary
*******************************************************************************/
static const OD_entry_t ODList[] = {
    {0x1000, 0x01, ODT_VAR, &ODObjs.o_1000_deviceType},
    {0x1001, 0x01, ODT_VAR, &ODObjs.o_1001_errorRegister},
    {0x1003, 0x09, ODT_EARR, &ODObjs.oE_1003_pre_definedErrorField},
    {0x1005, 0x01, ODT_EVAR, &ODObjs.oE_1005_COB_ID_SYNCMessage},
    {0x1006, 0x01, ODT_EVAR, &ODObjs.oE_1006_communicationCyclePeriod},
    {0x1007, 0x01, ODT_EVAR, &ODObjs.oE_1007_synchronousWindowLength},
    {0x1010, 0x05, ODT_EARR, &ODObjs.oE_1010_storeParameters},
    {0x1011, 0x05, ODT_EARR, &ODObjs.oE_1011_restoreDefaultParameters},
    {0x1012, 0x01, ODT_EVAR, &ODObjs.oE_1012_COB_IDTimeStampObject},
    {0x1014, 0x01, ODT_EVAR, &ODObjs.oE_1014_COB_ID_EMCY},
    {0x1015, 0x01, ODT_EVAR, &ODObjs.oE_1015_inhibitTimeEMCY},
    {0x1016, 0x09, ODT_EARR, &ODObjs.oE_1016_consumerHeartbeatTime},
    {0x1017, 0x01, ODT_EVAR, &ODObjs.oE_1017_producerHeartbeatTime},
    {0x1018, 0x05, ODT_REC, &ODObjs.o_1018_identity},
    {0x1019, 0x01, ODT_VAR, &ODObjs.o_1019_synchronousCounterOverflowValue},
    {0x1200, 0x03, ODT_EREC, &ODObjs.oE_1200_SDOServerParameter},
    {0x1280, 0x04, ODT_EREC, &ODObjs.oE_1280_SDOClientParameter},
    {0x1400, 0x04, ODT_EREC, &ODObjs.oE_1400_RPDOCommunicationParameter},
    {0x1401, 0x04, ODT_EREC, &ODObjs.oE_1401_RPDOCommunicationParameter},
    {0x1402, 0x04, ODT_EREC, &ODObjs.oE_1402_RPDOCommunicationParameter},
    {0x1403, 0x04, ODT_EREC, &ODObjs.oE_1403_RPDOCommunicationParameter},
    {0x1600, 0x09, ODT_EREC, &ODObjs.oE_1600_RPDOMappingParameter},
    {0x1601, 0x09, ODT_EREC, &ODObjs.oE_1601_RPDOMappingParameter},
    {0x1602, 0x09, ODT_EREC, &ODObjs.oE_1602_RPDOMappingParameter},
    {0x1603, 0x09, ODT_EREC, &ODObjs.oE_1603_RPDOMappingParameter},
    {0x1800, 0x06, ODT_EREC, &ODObjs.oE_1800_TPDOCommunicationParameter},
    {0x1801, 0x06, ODT_EREC, &ODObjs.oE_1801_TPDOCommunicationParameter},
    {0x1802, 0x06, ODT_EREC, &ODObjs.oE_1802_TPDOCommunicationParameter},
    {0x1803, 0x06, ODT_EREC, &ODObjs.oE_1803_TPDOCommunicationParameter},
    {0x1A00, 0x09, ODT_EREC, &ODObjs.oE_1A00_TPDOMappingParameter},
    {0x1A01, 0x09, ODT_EREC, &ODObjs.oE_1A01_TPDOMappingParameter},
    {0x1A02, 0x09, ODT_EREC, &ODObjs.oE_1A02_TPDOMappingParameter},
    {0x1A03, 0x09, ODT_EREC, &ODObjs.oE_1A03_TPDOMappingParameter},
    {0x0000, 0x00, 0, NULL}
};

const OD_t OD = {
    (sizeof(ODList) / sizeof(ODList[0])) - 1,
    &ODList[0]
};
