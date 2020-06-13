/*
 * Main CANopen stack file. It combines Object dictionary (CO_OD) and all other
 * CANopen source files. Configuration information are read from CO_OD.h file.
 *
 * @file        CANopen.c
 * @ingroup     CO_CANopen
 * @author      Janez Paternoster
 * @copyright   2010 - 2020 Janez Paternoster
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


#include "CANopen.h"

#include <stdlib.h>


/* Global variables ***********************************************************/
/* #define CO_USE_GLOBALS */    /* If defined, global variables will be used
                                   instead of dynamically allocated. */
extern const CO_OD_entry_t CO_OD[CO_OD_NoOfElements];   /* Object Dictionary */
static CO_t COO;                    /* Pointers to CANopen objects */
CO_t *CO = NULL;                    /* Pointer to COO */

static CO_CANrx_t          *CO_CANmodule_rxArray0;
static CO_CANtx_t          *CO_CANmodule_txArray0;
static CO_OD_extension_t   *CO_SDO_ODExtensions;
static CO_HBconsNode_t     *CO_HBcons_monitoredNodes;

#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII) && !defined CO_GTWA_ENABLE
#define CO_GTWA_ENABLE      true
#endif
#if CO_NO_TRACE > 0
static uint32_t            *CO_traceTimeBuffers[CO_NO_TRACE];
static int32_t             *CO_traceValueBuffers[CO_NO_TRACE];
static uint32_t             CO_traceBufferSize[CO_NO_TRACE];
#endif
#ifndef CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS
#define CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS 0
#endif


/* Verify number of CANopenNode objects from CO_OD.h **************************/
#if    CO_NO_SYNC                                 >  1     \
    || CO_NO_EMERGENCY                            != 1     \
    || (CO_NO_SDO_SERVER < 1 || CO_NO_SDO_SERVER > 128)    \
    || CO_NO_TIME                                 >  1     \
    || CO_NO_SDO_CLIENT                           > 128    \
    || (CO_NO_RPDO < 1 || CO_NO_RPDO > 0x200)              \
    || (CO_NO_TPDO < 1 || CO_NO_TPDO > 0x200)              \
    || ODL_consumerHeartbeatTime_arrayLength      == 0     \
    || ODL_errorStatusBits_stringLength           < 10     \
    || CO_NO_LSS_SLAVE                            >  1     \
    || CO_NO_LSS_MASTER                           >  1
#error Features from CO_OD.h file are not corectly configured for this project!
#endif

/* Indexes of CO_CANrx_t objects in CO_CANmodule_t and total number of them. **/
#define CO_RXCAN_NMT      0
#define CO_RXCAN_SYNC    (CO_RXCAN_NMT      + CO_NO_NMT)
#define CO_RXCAN_EMERG   (CO_RXCAN_SYNC     + CO_NO_SYNC)
#define CO_RXCAN_TIME    (CO_RXCAN_EMERG    + CO_NO_EM_CONS)
#define CO_RXCAN_RPDO    (CO_RXCAN_TIME     + CO_NO_TIME)
#define CO_RXCAN_SDO_SRV (CO_RXCAN_RPDO     + CO_NO_RPDO)
#define CO_RXCAN_SDO_CLI (CO_RXCAN_SDO_SRV  + CO_NO_SDO_SERVER)
#define CO_RXCAN_CONS_HB (CO_RXCAN_SDO_CLI  + CO_NO_SDO_CLIENT)
#define CO_RXCAN_LSS_SLV (CO_RXCAN_CONS_HB  + CO_NO_HB_CONS)
#define CO_RXCAN_LSS_MST (CO_RXCAN_LSS_SLV  + CO_NO_LSS_SLAVE)
#define CO_RXCAN_NO_MSGS (CO_NO_NMT         + \
                          CO_NO_SYNC        + \
                          CO_NO_EM_CONS     + \
                          CO_NO_TIME        + \
                          CO_NO_RPDO        + \
                          CO_NO_SDO_SERVER  + \
                          CO_NO_SDO_CLIENT  + \
                          CO_NO_HB_CONS     + \
                          CO_NO_LSS_SLAVE   + \
                          CO_NO_LSS_MASTER)

/* Indexes of CO_CANtx_t objects in CO_CANmodule_t and total number of them. **/
#define CO_TXCAN_NMT      0
#define CO_TXCAN_SYNC    (CO_TXCAN_NMT      + CO_NO_NMT_MST)
#define CO_TXCAN_EMERG   (CO_TXCAN_SYNC     + CO_NO_SYNC)
#define CO_TXCAN_TIME    (CO_TXCAN_EMERG    + CO_NO_EMERGENCY)
#define CO_TXCAN_TPDO    (CO_TXCAN_TIME     + CO_NO_TIME)
#define CO_TXCAN_SDO_SRV (CO_TXCAN_TPDO     + CO_NO_TPDO)
#define CO_TXCAN_SDO_CLI (CO_TXCAN_SDO_SRV  + CO_NO_SDO_SERVER)
#define CO_TXCAN_HB      (CO_TXCAN_SDO_CLI  + CO_NO_SDO_CLIENT)
#define CO_TXCAN_LSS_SLV (CO_TXCAN_HB       + CO_NO_HB_PROD)
#define CO_TXCAN_LSS_MST (CO_TXCAN_LSS_SLV  + CO_NO_LSS_SLAVE)
#define CO_TXCAN_NO_MSGS (CO_NO_NMT_MST     + \
                          CO_NO_SYNC        + \
                          CO_NO_EMERGENCY   + \
                          CO_NO_TIME        + \
                          CO_NO_TPDO        + \
                          CO_NO_SDO_SERVER  + \
                          CO_NO_SDO_CLIENT  + \
                          CO_NO_HB_PROD     + \
                          CO_NO_LSS_SLAVE   + \
                          CO_NO_LSS_MASTER)


/* Create objects from heap ***************************************************/
#ifndef CO_USE_GLOBALS
CO_ReturnError_t CO_new(uint32_t *heapMemoryUsed) {
    int16_t i;
    uint16_t errCnt = 0;
    uint32_t CO_memoryUsed = 0;

    /* If CANopen was initialized before, return. */
    if (CO != NULL) {
        return CO_ERROR_NO;
    }

    /* globals */
    CO = &COO;

    /* CANmodule */
    CO->CANmodule[0] = (CO_CANmodule_t *)calloc(1, sizeof(CO_CANmodule_t));
    if (CO->CANmodule[0] == NULL) errCnt++;
    CO_CANmodule_rxArray0 =
        (CO_CANrx_t *)calloc(CO_RXCAN_NO_MSGS, sizeof(CO_CANrx_t));
    if (CO_CANmodule_rxArray0 == NULL) errCnt++;
    CO_CANmodule_txArray0 =
        (CO_CANtx_t *)calloc(CO_TXCAN_NO_MSGS, sizeof(CO_CANtx_t));
    if (CO_CANmodule_txArray0 == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_CANmodule_t) +
                     sizeof(CO_CANrx_t) * CO_RXCAN_NO_MSGS +
                     sizeof(CO_CANtx_t) * CO_TXCAN_NO_MSGS;

    /* SDOserver */
    for (i = 0; i < CO_NO_SDO_SERVER; i++) {
        CO->SDO[i] = (CO_SDO_t *)calloc(1, sizeof(CO_SDO_t));
        if (CO->SDO[i] == NULL) errCnt++;
    }
    CO_SDO_ODExtensions = (CO_OD_extension_t *)calloc(
        CO_OD_NoOfElements, sizeof(CO_OD_extension_t));
    if (CO_SDO_ODExtensions == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_SDO_t) * CO_NO_SDO_SERVER +
                     sizeof(CO_OD_extension_t) * CO_OD_NoOfElements;

    /* Emergency */
    CO->em = (CO_EM_t *)calloc(1, sizeof(CO_EM_t));
    if (CO->em == NULL) errCnt++;
    CO->emPr = (CO_EMpr_t *)calloc(1, sizeof(CO_EMpr_t));
    if (CO->emPr == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_EM_t) + sizeof(CO_EMpr_t);

    /* NMT_Heartbeat */
    CO->NMT = (CO_NMT_t *)calloc(1, sizeof(CO_NMT_t));
    if (CO->NMT == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_NMT_t);

#if CO_NO_SYNC == 1
    /* SYNC */
    CO->SYNC = (CO_SYNC_t *)calloc(1, sizeof(CO_SYNC_t));
    if (CO->SYNC == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_SYNC_t);
#endif

#if CO_NO_TIME == 1
    /* TIME */
    CO->TIME = (CO_TIME_t *)calloc(1, sizeof(CO_TIME_t));
    if (CO->TIME == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_TIME_t);
#else
    CO->TIME = NULL;
#endif

    /* RPDO */
    for (i = 0; i < CO_NO_RPDO; i++) {
        CO->RPDO[i] = (CO_RPDO_t *)calloc(1, sizeof(CO_RPDO_t));
        if (CO->RPDO[i] == NULL) errCnt++;
    }
    CO_memoryUsed += sizeof(CO_RPDO_t) * CO_NO_RPDO;

    /* TPDO */
    for (i = 0; i < CO_NO_TPDO; i++) {
        CO->TPDO[i] = (CO_TPDO_t *)calloc(1, sizeof(CO_TPDO_t));
        if (CO->TPDO[i] == NULL) errCnt++;
    }
    CO_memoryUsed += sizeof(CO_TPDO_t) * CO_NO_TPDO;

    /* Heartbeat consumer */
    CO->HBcons = (CO_HBconsumer_t *)calloc(1, sizeof(CO_HBconsumer_t));
    if (CO->HBcons == NULL) errCnt++;
    CO_HBcons_monitoredNodes =
        (CO_HBconsNode_t *)calloc(CO_NO_HB_CONS, sizeof(CO_HBconsNode_t));
    if (CO_HBcons_monitoredNodes == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_HBconsumer_t) +
                     sizeof(CO_HBconsNode_t) * CO_NO_HB_CONS;

#if CO_NO_SDO_CLIENT != 0
    /* SDOclient */
    for (i = 0; i < CO_NO_SDO_CLIENT; i++) {
        CO->SDOclient[i] =
            (CO_SDOclient_t *)calloc(1, sizeof(CO_SDOclient_t));
        if (CO->SDOclient[i] == NULL) errCnt++;
    }
    CO_memoryUsed += sizeof(CO_SDOclient_t) * CO_NO_SDO_CLIENT;
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    /* LEDs */
    CO->LEDs = (CO_LEDs_t *)calloc(1, sizeof(CO_LEDs_t));
    if (CO->LEDs == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_LEDs_t);
#endif

#if CO_NO_LSS_SLAVE == 1
    /* LSSslave */
    CO->LSSslave = (CO_LSSslave_t *)calloc(1, sizeof(CO_LSSslave_t));
    if (CO->LSSslave == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_LSSslave_t);
#endif

#if CO_NO_LSS_MASTER == 1
    /* LSSmaster */
    CO->LSSmaster = (CO_LSSmaster_t *)calloc(1, sizeof(CO_LSSmaster_t));
    if (CO->LSSmaster == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_LSSmaster_t);
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Gateway-ascii */
    CO->gtwa = (CO_GTWA_t *)calloc(1, sizeof(CO_GTWA_t));
    if (CO->gtwa == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_GTWA_t);
#endif

#if CO_NO_TRACE > 0
    /* Trace */
    for (i = 0; i < CO_NO_TRACE; i++) {
        CO->trace[i] = (CO_trace_t *)calloc(1, sizeof(CO_trace_t));
        if (CO->trace[i] == NULL) errCnt++;
    }
    CO_memoryUsed += sizeof(CO_trace_t) * CO_NO_TRACE;
    for (i = 0; i < CO_NO_TRACE; i++) {
        CO_traceTimeBuffers[i] =
            (uint32_t *)calloc(OD_traceConfig[i].size, sizeof(uint32_t));
        CO_traceValueBuffers[i] =
            (int32_t *)calloc(OD_traceConfig[i].size, sizeof(int32_t));
        if (CO_traceTimeBuffers[i] != NULL &&
            CO_traceValueBuffers[i] != NULL) {
            CO_traceBufferSize[i] = OD_traceConfig[i].size;
        } else {
            CO_traceBufferSize[i] = 0;
        }
        CO_memoryUsed += CO_traceBufferSize[i] * sizeof(uint32_t) * 2;
    }
#endif

    if(heapMemoryUsed != NULL) {
        *heapMemoryUsed = CO_memoryUsed;
    }

    return (errCnt == 0) ? CO_ERROR_NO : CO_ERROR_OUT_OF_MEMORY;
}


/******************************************************************************/
void CO_delete(void *CANptr) {
    int16_t i;

    CO_CANsetConfigurationMode(CANptr);

    /* If CANopen isn't initialized, return. */
    if (CO == NULL) {
        return;
    }

    CO_CANmodule_disable(CO->CANmodule[0]);

#if CO_NO_TRACE > 0
    /* Trace */
    for (i = 0; i < CO_NO_TRACE; i++) {
        free(CO->trace[i]);
        free(CO_traceTimeBuffers[i]);
        free(CO_traceValueBuffers[i]);
    }
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Gateway-ascii */
    free(CO->gtwa);
#endif

#if CO_NO_LSS_MASTER == 1
    /* LSSmaster */
    free(CO->LSSmaster);
#endif

#if CO_NO_LSS_SLAVE == 1
    /* LSSslave */
    free(CO->LSSslave);
#endif

#if CO_NO_SDO_CLIENT != 0
    /* SDOclient */
    for (i = 0; i < CO_NO_SDO_CLIENT; i++) {
        free(CO->SDOclient[i]);
    }
#endif

    /* Heartbeat consumer */
    free(CO_HBcons_monitoredNodes);
    free(CO->HBcons);

    /* TPDO */
    for (i = 0; i < CO_NO_TPDO; i++) {
        free(CO->TPDO[i]);
    }

    /* RPDO */
    for (i = 0; i < CO_NO_RPDO; i++) {
        free(CO->RPDO[i]);
    }

#if CO_NO_TIME == 1
    /* TIME */
    free(CO->TIME);
#endif

#if CO_NO_SYNC == 1
    /* SYNC */
    free(CO->SYNC);
#endif

    /* NMT_Heartbeat */
    free(CO->NMT);

    /* Emergency */
    free(CO->emPr);
    free(CO->em);

    /* SDOserver */
    free(CO_SDO_ODExtensions);
    for (i = 0; i < CO_NO_SDO_SERVER; i++) {
        free(CO->SDO[i]);
    }

    /* CANmodule */
    free(CO_CANmodule_txArray0);
    free(CO_CANmodule_rxArray0);
    free(CO->CANmodule[0]);

    /* globals */
    CO = NULL;
}
#endif /* #ifndef CO_USE_GLOBALS */


/* Alternatively create objects as globals ************************************/
#ifdef CO_USE_GLOBALS
    static CO_CANmodule_t       COO_CANmodule;
    static CO_CANrx_t           COO_CANmodule_rxArray0[CO_RXCAN_NO_MSGS];
    static CO_CANtx_t           COO_CANmodule_txArray0[CO_TXCAN_NO_MSGS];
    static CO_SDO_t             COO_SDO[CO_NO_SDO_SERVER];
    static CO_OD_extension_t    COO_SDO_ODExtensions[CO_OD_NoOfElements];
    static CO_EM_t              COO_EM;
    static CO_EMpr_t            COO_EMpr;
    static CO_NMT_t             COO_NMT;
#if CO_NO_SYNC == 1
    static CO_SYNC_t            COO_SYNC;
#endif
#if CO_NO_TIME == 1
    static CO_TIME_t            COO_TIME;
#endif
    static CO_RPDO_t            COO_RPDO[CO_NO_RPDO];
    static CO_TPDO_t            COO_TPDO[CO_NO_TPDO];
    static CO_HBconsumer_t      COO_HBcons;
    static CO_HBconsNode_t      COO_HBcons_monitoredNodes[CO_NO_HB_CONS];
#if CO_NO_SDO_CLIENT != 0
    static CO_SDOclient_t       COO_SDOclient[CO_NO_SDO_CLIENT];
#endif
#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    static CO_LEDs_t            COO_LEDs;
#endif
#if CO_NO_LSS_SLAVE == 1
    static CO_LSSslave_t        COO_LSSslave;
#endif
#if CO_NO_LSS_MASTER == 1
    static CO_LSSmaster_t       COO_LSSmaster;
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    static CO_GTWA_t            COO_gtwa;
#endif
#if CO_NO_TRACE > 0
  #ifndef CO_TRACE_BUFFER_SIZE_FIXED
    #define CO_TRACE_BUFFER_SIZE_FIXED 100
  #endif
    static CO_trace_t           COO_trace[CO_NO_TRACE];
    static uint32_t             COO_traceTimeBuffers[CO_NO_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
    static int32_t              COO_traceValueBuffers[CO_NO_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
#endif

CO_ReturnError_t CO_new(uint32_t *heapMemoryUsed) {
    int16_t i;

    /* If CANopen was initialized before, return. */
    if (CO != NULL) {
        return CO_ERROR_NO;
    }

    /* globals */
    CO = &COO;

    /* CANmodule */
    CO->CANmodule[0] = &COO_CANmodule;
    CO_CANmodule_rxArray0 = &COO_CANmodule_rxArray0[0];
    CO_CANmodule_txArray0 = &COO_CANmodule_txArray0[0];

    /* SDOserver */
    for (i = 0; i < CO_NO_SDO_SERVER; i++) {
        CO->SDO[i] = &COO_SDO[i];
    }
    CO_SDO_ODExtensions = &COO_SDO_ODExtensions[0];

    /* Emergency */
    CO->em = &COO_EM;
    CO->emPr = &COO_EMpr;

    /* NMT_Heartbeat */
    CO->NMT = &COO_NMT;

#if CO_NO_SYNC == 1
    /* SYNC */
    CO->SYNC = &COO_SYNC;
#endif

#if CO_NO_TIME == 1
    /* TIME */
    CO->TIME = &COO_TIME;
#else
    CO->TIME = NULL;
#endif

    /* RPDO */
    for (i = 0; i < CO_NO_RPDO; i++) {
        CO->RPDO[i] = &COO_RPDO[i];
    }

    /* TPDO */
    for (i = 0; i < CO_NO_TPDO; i++) {
        CO->TPDO[i] = &COO_TPDO[i];
    }

    /* Heartbeat consumer */
    CO->HBcons = &COO_HBcons;
    CO_HBcons_monitoredNodes = &COO_HBcons_monitoredNodes[0];

#if CO_NO_SDO_CLIENT != 0
    /* SDOclient */
    for (i = 0; i < CO_NO_SDO_CLIENT; i++) {
        CO->SDOclient[i] = &COO_SDOclient[i];
    }
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    /* LEDs */
    CO->LEDs = &COO_LEDs;
#endif

#if CO_NO_LSS_SLAVE == 1
    /* LSSslave */
    CO->LSSslave = &COO_LSSslave;
#endif

#if CO_NO_LSS_MASTER == 1
    /* LSSmaster */
    CO->LSSmaster = &COO_LSSmaster;
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Gateway-ascii */
    CO->gtwa = &COO_gtwa;
#endif

#if CO_NO_TRACE > 0
    /* Trace */
    for (i = 0; i < CO_NO_TRACE; i++) {
        CO->trace[i] = &COO_trace[i];
        CO_traceTimeBuffers[i] = &COO_traceTimeBuffers[i][0];
        CO_traceValueBuffers[i] = &COO_traceValueBuffers[i][0];
        CO_traceBufferSize[i] = CO_TRACE_BUFFER_SIZE_FIXED;
    }
#endif

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_delete(void *CANptr) {
    CO_CANsetConfigurationMode(CANptr);

    /* globals */
    CO = NULL;
}
#endif /* #ifdef CO_USE_GLOBALS */


/******************************************************************************/
CO_ReturnError_t CO_CANinit(void *CANptr,
                            uint16_t bitRate)
{
    CO_ReturnError_t err;

    CO->CANmodule[0]->CANnormal = false;
    CO_CANsetConfigurationMode(CANptr);

    /* CANmodule */
    err = CO_CANmodule_init(CO->CANmodule[0],
                            CANptr,
                            CO_CANmodule_rxArray0,
                            CO_RXCAN_NO_MSGS,
                            CO_CANmodule_txArray0,
                            CO_TXCAN_NO_MSGS,
                            bitRate);

    return err;
}


/******************************************************************************/
#if CO_NO_LSS_SLAVE == 1
CO_ReturnError_t CO_LSSinit(uint8_t *nodeId,
                            uint16_t *bitRate)
{
    CO_LSS_address_t lssAddress;
    CO_ReturnError_t err;

    /* LSSslave */
    lssAddress.identity.productCode = OD_identity.productCode;
    lssAddress.identity.revisionNumber = OD_identity.revisionNumber;
    lssAddress.identity.serialNumber = OD_identity.serialNumber;
    lssAddress.identity.vendorID = OD_identity.vendorID;

    err = CO_LSSslave_init(CO->LSSslave,
                           lssAddress,
                           bitRate,
                           nodeId,
                           CO->CANmodule[0],
                           CO_RXCAN_LSS_SLV,
                           CO_CAN_ID_LSS_SRV,
                           CO->CANmodule[0],
                           CO_TXCAN_LSS_SLV,
                           CO_CAN_ID_LSS_CLI);

    return err;
}
#endif /* CO_NO_LSS_SLAVE == 1 */


/******************************************************************************/
CO_ReturnError_t CO_CANopenInit(uint8_t nodeId) {
    int16_t i;
    CO_ReturnError_t err;

    /* Verify CANopen Node-ID */
    CO->nodeIdUnconfigured = false;
#if CO_NO_LSS_SLAVE == 1
    if (nodeId == CO_LSS_NODE_ID_ASSIGNMENT) {
        CO->nodeIdUnconfigured = true;
    }
    else
#endif
    if (nodeId < 1 || nodeId > 127) {
        return CO_ERROR_PARAMETERS;
    }

    /* Verify parameters from CO_OD */
    if (sizeof(OD_TPDOCommunicationParameter_t) != sizeof(CO_TPDOCommPar_t) ||
        sizeof(OD_TPDOMappingParameter_t)       != sizeof(CO_TPDOMapPar_t)  ||
        sizeof(OD_RPDOCommunicationParameter_t) != sizeof(CO_RPDOCommPar_t) ||
        sizeof(OD_RPDOMappingParameter_t)       != sizeof(CO_RPDOMapPar_t)) {
        return CO_ERROR_PARAMETERS;
    }
#if CO_NO_SDO_CLIENT != 0
    if (sizeof(OD_SDOClientParameter_t)         != sizeof(CO_SDOclientPar_t)) {
        return CO_ERROR_PARAMETERS;
    }
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    /* LEDs */
    err = CO_LEDs_init(CO->LEDs);

    if (err) return err;
#endif

#if CO_NO_LSS_SLAVE == 1
    if (CO->nodeIdUnconfigured) {
        return CO_ERROR_NODE_ID_UNCONFIGURED_LSS;
    }
#endif

    /* SDOserver */
    for (i = 0; i < CO_NO_SDO_SERVER; i++) {
        uint32_t COB_IDClientToServer;
        uint32_t COB_IDServerToClient;
        if (i == 0) {
            /*Default SDO server must be located at first index*/
            COB_IDClientToServer = CO_CAN_ID_RSDO + nodeId;
            COB_IDServerToClient = CO_CAN_ID_TSDO + nodeId;
        }
        else {
            COB_IDClientToServer =
                OD_SDOServerParameter[i].COB_IDClientToServer;
            COB_IDServerToClient =
                OD_SDOServerParameter[i].COB_IDServerToClient;
        }

        err = CO_SDO_init(CO->SDO[i],
                          COB_IDClientToServer,
                          COB_IDServerToClient,
                          OD_H1200_SDO_SERVER_PARAM + i,
                          i == 0 ? 0 : CO->SDO[0],
                          &CO_OD[0],
                          CO_OD_NoOfElements,
                          CO_SDO_ODExtensions,
                          nodeId,
                          1000,
                          CO->CANmodule[0],
                          CO_RXCAN_SDO_SRV + i,
                          CO->CANmodule[0],
                          CO_TXCAN_SDO_SRV + i);

        if (err) return err;
    }


    /* Emergency */
    err = CO_EM_init(CO->em,
                     CO->emPr,
                     CO->SDO[0],
                     &OD_errorStatusBits[0],
                     ODL_errorStatusBits_stringLength,
                     &OD_errorRegister,
                     &OD_preDefinedErrorField[0],
                     ODL_preDefinedErrorField_arrayLength,
                     CO->CANmodule[0],
                     CO_RXCAN_EMERG,
                     CO->CANmodule[0],
                     CO_TXCAN_EMERG,
                     (uint16_t)CO_CAN_ID_EMERGENCY + nodeId);

    if (err) return err;

    /* NMT_Heartbeat */
    err = CO_NMT_init(CO->NMT,
                      CO->emPr,
                      nodeId,
                      500,
                      CO->CANmodule[0],
                      CO_RXCAN_NMT,
                      CO_CAN_ID_NMT_SERVICE,
                      CO->CANmodule[0],
                      CO_TXCAN_NMT,
                      CO_CAN_ID_NMT_SERVICE,
                      CO->CANmodule[0],
                      CO_TXCAN_HB,
                      CO_CAN_ID_HEARTBEAT + nodeId);

    if (err) return err;

#if CO_NO_SYNC == 1
    /* SYNC */
    err = CO_SYNC_init(CO->SYNC,
                       CO->em,
                       CO->SDO[0],
                       &CO->NMT->operatingState,
                       OD_COB_ID_SYNCMessage,
                       OD_communicationCyclePeriod,
                       OD_synchronousCounterOverflowValue,
                       CO->CANmodule[0],
                       CO_RXCAN_SYNC,
                       CO->CANmodule[0],
                       CO_TXCAN_SYNC);

    if (err) return err;
#endif

#if CO_NO_TIME == 1
    /* TIME */
    err = CO_TIME_init(CO->TIME,
                       CO->em,
                       CO->SDO[0],
                       &CO->NMT->operatingState,
                       OD_COB_ID_TIME,
                       0,
                       CO->CANmodule[0],
                       CO_RXCAN_TIME,
                       CO->CANmodule[0],
                       CO_TXCAN_TIME);

    if (err) return err;
#endif

    /* RPDO */
    for (i = 0; i < CO_NO_RPDO; i++) {
        CO_CANmodule_t *CANdevRx = CO->CANmodule[0];
        uint16_t CANdevRxIdx = CO_RXCAN_RPDO + i;

        err = CO_RPDO_init(CO->RPDO[i],
                           CO->em,
                           CO->SDO[0],
#if (CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE
                           CO->SYNC,
#endif
                           &CO->NMT->operatingState,
                           nodeId,
                           ((i < 4) ? (CO_CAN_ID_RPDO_1 + i * 0x100) : 0),
                           0,
                           (CO_RPDOCommPar_t*)&OD_RPDOCommunicationParameter[i],
                           (CO_RPDOMapPar_t *)&OD_RPDOMappingParameter[i],
                           OD_H1400_RXPDO_1_PARAM + i,
                           OD_H1600_RXPDO_1_MAPPING + i,
                           CANdevRx,
                           CANdevRxIdx);

        if (err) return err;
    }

    /* TPDO */
    for (i = 0; i < CO_NO_TPDO; i++) {
        err = CO_TPDO_init(CO->TPDO[i],
                           CO->em,
                           CO->SDO[0],
#if (CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE
                           CO->SYNC,
#endif
                           &CO->NMT->operatingState,
                           nodeId,
                           ((i < 4) ? (CO_CAN_ID_TPDO_1 + i * 0x100) : 0),
                           0,
                           (CO_TPDOCommPar_t*)&OD_TPDOCommunicationParameter[i],
                           (CO_TPDOMapPar_t *)&OD_TPDOMappingParameter[i],
                           OD_H1800_TXPDO_1_PARAM + i,
                           OD_H1A00_TXPDO_1_MAPPING + i,
                           CO->CANmodule[0],
                           CO_TXCAN_TPDO + i);

        if (err) return err;
    }

    /* Heartbeat consumer */
    err = CO_HBconsumer_init(CO->HBcons,
                             CO->em,
                             CO->SDO[0],
                             &OD_consumerHeartbeatTime[0],
                             CO_HBcons_monitoredNodes,
                             CO_NO_HB_CONS,
                             CO->CANmodule[0],
                             CO_RXCAN_CONS_HB);

    if (err) return err;

#if CO_NO_SDO_CLIENT != 0
    /* SDOclient */
    for (i = 0; i < CO_NO_SDO_CLIENT; i++) {
        err = CO_SDOclient_init(CO->SDOclient[i],
                                (void *)CO->SDO[0],
                                (CO_SDOclientPar_t *)&OD_SDOClientParameter[i],
                                CO->CANmodule[0],
                                CO_RXCAN_SDO_CLI + i,
                                CO->CANmodule[0],
                                CO_TXCAN_SDO_CLI + i);

        if (err) return err;
    }
#endif

#if CO_NO_LSS_MASTER == 1
    /* LSSmaster */
    err = CO_LSSmaster_init(CO->LSSmaster,
                            CO_LSSmaster_DEFAULT_TIMEOUT,
                            CO->CANmodule[0],
                            CO_RXCAN_LSS_MST,
                            CO_CAN_ID_LSS_CLI,
                            CO->CANmodule[0],
                            CO_TXCAN_LSS_MST,
                            CO_CAN_ID_LSS_SRV);

    if (err) return err;
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Gateway-ascii */
    err = CO_GTWA_init(CO->gtwa,
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
                       CO->SDOclient[0],
                       500,
                       false,
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT
                       CO->NMT,
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
                       CO->LSSmaster,
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
                       CO->LEDs,
#endif
                       0);
    if (err) return err;
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */

#if CO_NO_TRACE > 0
    /* Trace */
    for (i = 0; i < CO_NO_TRACE; i++) {
        CO_trace_init(CO->trace[i],
                      CO->SDO[0],
                      OD_traceConfig[i].axisNo,
                      CO_traceTimeBuffers[i],
                      CO_traceValueBuffers[i],
                      CO_traceBufferSize[i],
                      &OD_traceConfig[i].map,
                      &OD_traceConfig[i].format,
                      &OD_traceConfig[i].trigger,
                      &OD_traceConfig[i].threshold,
                      &OD_trace[i].value,
                      &OD_trace[i].min,
                      &OD_trace[i].max,
                      &OD_trace[i].triggerTime,
                      OD_INDEX_TRACE_CONFIG + i,
                      OD_INDEX_TRACE + i);
    }
#endif

    return CO_ERROR_NO;
}


/******************************************************************************/
CO_NMT_reset_cmd_t CO_process(CO_t *co,
                              uint32_t timeDifference_us,
                              uint32_t *timerNext_us)
{
    uint8_t i;
    bool_t NMTisPreOrOperational = false;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

    CO_CANmodule_process(CO->CANmodule[0]);

#if CO_NO_LSS_SLAVE == 1
    bool_t resetLSS = CO_LSSslave_process(co->LSSslave);
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    bool_t unc = co->nodeIdUnconfigured;
    uint16_t CANerrorStatus = CO->CANmodule[0]->CANerrorStatus;
    CO_LEDs_process(co->LEDs,
                    timeDifference_us,
                    unc ? CO_NMT_INITIALIZING : co->NMT->operatingState,
    #if CO_NO_LSS_SLAVE == 1
                    CO_LSSslave_getState(co->LSSslave)
                        == CO_LSS_STATE_CONFIGURATION,
    #else
                    false,
    #endif
                    (CANerrorStatus & CO_CAN_ERRTX_BUS_OFF) != 0,
                    (CANerrorStatus & CO_CAN_ERR_WARN_PASSIVE) != 0,
                    0, /* RPDO event timer timeout */
                    unc ? false : CO_isError(co->em, CO_EM_SYNC_TIME_OUT),
                    unc ? false : (CO_isError(co->em, CO_EM_HEARTBEAT_CONSUMER)
                        || CO_isError(co->em, CO_EM_HB_CONSUMER_REMOTE_RESET)),
                    OD_errorRegister != 0,
                    CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS,
                    timerNext_us);
#endif /* (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE */

#if CO_NO_LSS_SLAVE == 1
    if (resetLSS) {
        reset = CO_RESET_COMM;
    }
    if (co->nodeIdUnconfigured) {
        return reset;
    }
#endif

    if (co->NMT->operatingState == CO_NMT_PRE_OPERATIONAL ||
        co->NMT->operatingState == CO_NMT_OPERATIONAL)
        NMTisPreOrOperational = true;

    /* SDOserver */
    for (i = 0; i < CO_NO_SDO_SERVER; i++) {
        CO_SDO_process(co->SDO[i],
                       NMTisPreOrOperational,
                       timeDifference_us,
                       timerNext_us);
    }

    /* Emergency */
    CO_EM_process(co->emPr,
                  NMTisPreOrOperational,
                  timeDifference_us,
                  OD_inhibitTimeEMCY,
                  timerNext_us);

    /* NMT_Heartbeat */
    reset = CO_NMT_process(co->NMT,
                           timeDifference_us,
                           OD_producerHeartbeatTime,
                           OD_NMTStartup,
                           OD_errorRegister,
                           OD_errorBehavior,
                           timerNext_us);

#if CO_NO_TIME == 1
    /* TIME */
    CO_TIME_process(co->TIME,
                    timeDifference_us);
#endif

    /* Heartbeat consumer */
    CO_HBconsumer_process(co->HBcons,
                          NMTisPreOrOperational,
                          timeDifference_us,
                          timerNext_us);

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Gateway-ascii */
    CO_GTWA_process(co->gtwa,
                    CO_GTWA_ENABLE,
                    timeDifference_us,
                    timerNext_us);
#endif

    return reset;
}


/******************************************************************************/
#if CO_NO_SYNC == 1
bool_t CO_process_SYNC(CO_t *co,
                       uint32_t timeDifference_us,
                       uint32_t *timerNext_us)
{
    bool_t syncWas = false;

#if CO_NO_LSS_SLAVE == 1
    if (co->nodeIdUnconfigured) {
        return syncWas;
    }
#endif

    const CO_SYNC_status_t sync_process = CO_SYNC_process(co->SYNC,
                                   timeDifference_us,
                                   OD_synchronousWindowLength,
                                   timerNext_us);

    switch (sync_process) {
    case CO_SYNC_NONE:
        break;
    case CO_SYNC_RECEIVED:
        syncWas = true;
        break;
    case CO_SYNC_OUTSIDE_WINDOW:
        CO_CANclearPendingSyncPDOs(co->CANmodule[0]);
        break;
    }

    return syncWas;
}
#endif /* CO_NO_SYNC == 1 */


/******************************************************************************/
void CO_process_RPDO(CO_t *co,
                     bool_t syncWas)
{
    int16_t i;

#if CO_NO_LSS_SLAVE == 1
    if (co->nodeIdUnconfigured) {
        return;
    }
#endif

    for (i = 0; i < CO_NO_RPDO; i++) {
        CO_RPDO_process(co->RPDO[i], syncWas);
    }
}


/******************************************************************************/
void CO_process_TPDO(CO_t *co,
                     bool_t syncWas,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
    int16_t i;

#if CO_NO_LSS_SLAVE == 1
    if (co->nodeIdUnconfigured) {
        return;
    }
#endif

    /* Verify PDO Change Of State and process PDOs */
    for (i = 0; i < CO_NO_TPDO; i++) {
        if (!co->TPDO[i]->sendRequest)
            co->TPDO[i]->sendRequest = CO_TPDOisCOS(co->TPDO[i]);
        CO_TPDO_process(co->TPDO[i], syncWas, timeDifference_us, timerNext_us);
    }
}
