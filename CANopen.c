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


/* If defined, global variables will be used, otherwise CANopen objects will
   be generated with calloc(). */
/* #define CO_USE_GLOBALS */

/* If defined, the user provides an own implemetation for calculating the
 * CRC16 CCITT checksum. */
/* #define CO_USE_OWN_CRC16 */

#ifndef CO_USE_GLOBALS
    #include <stdlib.h> /*  for malloc, free */
    static uint32_t CO_memoryUsed = 0; /* informative */
#endif


/* Global variables ***********************************************************/
    extern const CO_OD_entry_t CO_OD[CO_OD_NoOfElements];  /* Object Dictionary array */
    static CO_t COO;
    CO_t *CO = NULL;

    static CO_CANrx_t          *CO_CANmodule_rxArray0;
    static CO_CANtx_t          *CO_CANmodule_txArray0;
    static CO_OD_extension_t   *CO_SDO_ODExtensions;
    static CO_HBconsNode_t     *CO_HBcons_monitoredNodes;
#if CO_NO_TRACE > 0
    static uint32_t            *CO_traceTimeBuffers[CO_NO_TRACE];
    static int32_t             *CO_traceValueBuffers[CO_NO_TRACE];
  #ifdef CO_USE_GLOBALS
  #ifndef CO_TRACE_BUFFER_SIZE_FIXED
    #define CO_TRACE_BUFFER_SIZE_FIXED 100
  #endif
  #endif
#endif


/* Verify features from CO_OD *************************************************/
    /* generate error, if features are not correctly configured for this project */
    #if        CO_NO_NMT_MASTER                           >  1     \
            || CO_NO_SYNC                                 >  1     \
            || CO_NO_EMERGENCY                            != 1     \
            || CO_NO_SDO_SERVER                           == 0     \
            || CO_NO_TIME                                 >  1     \
            || CO_NO_SDO_CLIENT                           > 128    \
            || (CO_NO_RPDO < 1 || CO_NO_RPDO > 0x200)              \
            || (CO_NO_TPDO < 1 || CO_NO_TPDO > 0x200)              \
            || ODL_consumerHeartbeatTime_arrayLength      == 0     \
            || ODL_errorStatusBits_stringLength           < 10     \
            || CO_NO_LSS_SERVER                           >  1     \
            || CO_NO_LSS_CLIENT                           >  1     \
            || (CO_NO_LSS_SERVER > 0 && CO_NO_LSS_CLIENT > 0)
        #error Features from CO_OD.h file are not corectly configured for this project!
    #endif


/* Indexes for CANopenNode message objects ************************************/
    #ifdef ODL_consumerHeartbeatTime_arrayLength
        #define CO_NO_HB_CONS   ODL_consumerHeartbeatTime_arrayLength
    #else
        #define CO_NO_HB_CONS   0
    #endif
    #define CO_NO_HB_PROD      1                                      /*  Producer Heartbeat Cont */

    #define CO_RXCAN_NMT       0                                      /*  index for NMT message */
    #define CO_RXCAN_SYNC      1                                      /*  index for SYNC message */
    #define CO_RXCAN_EMERG    (CO_RXCAN_SYNC+CO_NO_SYNC)              /*  index for Emergency message */
    #define CO_RXCAN_TIME     (CO_RXCAN_EMERG+CO_NO_EMERGENCY)        /*  index for TIME message */
    #define CO_RXCAN_RPDO     (CO_RXCAN_TIME+CO_NO_TIME)              /*  start index for RPDO messages */
    #define CO_RXCAN_SDO_SRV  (CO_RXCAN_RPDO+CO_NO_RPDO)              /*  start index for SDO server message (request) */
    #define CO_RXCAN_SDO_CLI  (CO_RXCAN_SDO_SRV+CO_NO_SDO_SERVER)     /*  start index for SDO client message (response) */
    #define CO_RXCAN_CONS_HB  (CO_RXCAN_SDO_CLI+CO_NO_SDO_CLIENT)     /*  start index for Heartbeat Consumer messages */
    #define CO_RXCAN_LSS      (CO_RXCAN_CONS_HB+CO_NO_HB_CONS)        /*  index for LSS rx message */
    /* total number of received CAN messages */
    #define CO_RXCAN_NO_MSGS (\
        1 + \
        CO_NO_SYNC + \
        CO_NO_EMERGENCY + \
        CO_NO_TIME + \
        CO_NO_RPDO + \
        CO_NO_SDO_SERVER + \
        CO_NO_SDO_CLIENT + \
        CO_NO_HB_CONS + \
        CO_NO_LSS_SERVER + \
        CO_NO_LSS_CLIENT + \
        0 \
    )

    #define CO_TXCAN_NMT       0                                      /*  index for NMT master message */
    #define CO_TXCAN_SYNC      CO_TXCAN_NMT+CO_NO_NMT_MASTER          /*  index for SYNC message */
    #define CO_TXCAN_EMERG    (CO_TXCAN_SYNC+CO_NO_SYNC)              /*  index for Emergency message */
    #define CO_TXCAN_TIME     (CO_TXCAN_EMERG+CO_NO_EMERGENCY)        /*  index for TIME message */
    #define CO_TXCAN_TPDO     (CO_TXCAN_TIME+CO_NO_TIME)              /*  start index for TPDO messages */
    #define CO_TXCAN_SDO_SRV  (CO_TXCAN_TPDO+CO_NO_TPDO)              /*  start index for SDO server message (response) */
    #define CO_TXCAN_SDO_CLI  (CO_TXCAN_SDO_SRV+CO_NO_SDO_SERVER)     /*  start index for SDO client message (request) */
    #define CO_TXCAN_HB       (CO_TXCAN_SDO_CLI+CO_NO_SDO_CLIENT)     /*  index for Heartbeat message */
    #define CO_TXCAN_LSS      (CO_TXCAN_HB+CO_NO_HB_PROD)             /*  index for LSS tx message */
    /* total number of transmitted CAN messages */
    #define CO_TXCAN_NO_MSGS ( \
        CO_NO_NMT_MASTER + \
        CO_NO_SYNC + \
        CO_NO_EMERGENCY + \
        CO_NO_TIME + \
        CO_NO_TPDO + \
        CO_NO_SDO_SERVER + \
        CO_NO_SDO_CLIENT + \
        CO_NO_HB_PROD + \
        CO_NO_LSS_SERVER + \
        CO_NO_LSS_CLIENT + \
        0\
    )


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
#if CO_NO_LSS_SERVER == 1
    static CO_LSSslave_t        CO0_LSSslave;
#endif
#if CO_NO_LSS_CLIENT == 1
    static CO_LSSmaster_t       CO0_LSSmaster;
#endif
#if CO_NO_SDO_CLIENT != 0
    static CO_SDOclient_t       COO_SDOclient[CO_NO_SDO_CLIENT];
#endif
#if CO_NO_TRACE > 0
    static CO_trace_t           COO_trace[CO_NO_TRACE];
    static uint32_t             COO_traceTimeBuffers[CO_NO_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
    static int32_t              COO_traceValueBuffers[CO_NO_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
#endif
#endif

/* These declarations here are needed in the case the switches for the project
    change the visibility in the headers in a way that the compiler doesn't see an declaration anymore */

#if CO_NO_LSS_SERVER == 0 /* LSS Server means LSS slave */

CO_ReturnError_t CO_new(void);

CO_ReturnError_t CO_CANinit(
        void                   *CANdriverState,
        uint16_t                bitRate);

CO_ReturnError_t CO_LSSinit(
        uint8_t                 nodeId,
        uint16_t                bitRate);

CO_ReturnError_t CO_CANopenInit(
        uint8_t                 nodeId);

#else /* CO_NO_LSS_SERVER == 0 */

CO_ReturnError_t CO_init(
        void                   *CANdriverState,
        uint8_t                 nodeId,
        uint16_t                bitRate);

#endif /* CO_NO_LSS_SERVER == 0 */


/* Helper function for NMT master *********************************************/
#if CO_NO_NMT_MASTER == 1
    CO_CANtx_t *NMTM_txBuff = 0;

    CO_ReturnError_t CO_sendNMTcommand(CO_t *co, uint8_t command, uint8_t nodeID){
        if(NMTM_txBuff == 0){
            /* error, CO_CANtxBufferInit() was not called for this buffer. */
            return CO_ERROR_TX_UNCONFIGURED; /* -11 */
        }
        NMTM_txBuff->data[0] = command;
        NMTM_txBuff->data[1] = nodeID;

        CO_ReturnError_t error = CO_ERROR_NO;

        /* Apply NMT command also to this node, if set so. */
        if(nodeID == 0 || nodeID == co->NMT->nodeId){
            switch(command){
                case CO_NMT_ENTER_OPERATIONAL:
                    if((*co->NMT->emPr->errorRegister) == 0) {
                        co->NMT->operatingState = CO_NMT_OPERATIONAL;
                    }
                    break;
                case CO_NMT_ENTER_STOPPED:
                    co->NMT->operatingState = CO_NMT_STOPPED;
                    break;
                case CO_NMT_ENTER_PRE_OPERATIONAL:
                    co->NMT->operatingState = CO_NMT_PRE_OPERATIONAL;
                    break;
                case CO_NMT_RESET_NODE:
                    co->NMT->resetCommand = CO_RESET_APP;
                    break;
                case CO_NMT_RESET_COMMUNICATION:
                    co->NMT->resetCommand = CO_RESET_COMM;
                    break;
                default:
                    error = CO_ERROR_ILLEGAL_ARGUMENT;
                    break;

            }
        }

        if(error == CO_ERROR_NO)
            return CO_CANsend(co->CANmodule[0], NMTM_txBuff); /* 0 = success */
        else
        {
            return error;
        }

    }
#endif


#if CO_NO_TRACE > 0
static uint32_t CO_traceBufferSize[CO_NO_TRACE];
#endif

/******************************************************************************/
CO_ReturnError_t CO_new(void)
{
    int16_t i;
#ifndef CO_USE_GLOBALS
    uint16_t errCnt;
#endif

    /* Verify parameters from CO_OD */
    if(   sizeof(OD_TPDOCommunicationParameter_t) != sizeof(CO_TPDOCommPar_t)
       || sizeof(OD_TPDOMappingParameter_t) != sizeof(CO_TPDOMapPar_t)
       || sizeof(OD_RPDOCommunicationParameter_t) != sizeof(CO_RPDOCommPar_t)
       || sizeof(OD_RPDOMappingParameter_t) != sizeof(CO_RPDOMapPar_t))
    {
        return CO_ERROR_PARAMETERS;
    }

    #if CO_NO_SDO_CLIENT != 0
    if(sizeof(OD_SDOClientParameter_t) != sizeof(CO_SDOclientPar_t)){
        return CO_ERROR_PARAMETERS;
    }
    #endif


    /* Initialize CANopen object */
#ifdef CO_USE_GLOBALS
    CO = &COO;

    CO_memset((uint8_t*)CO, 0, sizeof(CO_t));
    CO->CANmodule[0]                    = &COO_CANmodule;
    CO_CANmodule_rxArray0               = &COO_CANmodule_rxArray0[0];
    CO_CANmodule_txArray0               = &COO_CANmodule_txArray0[0];
    for(i=0; i<CO_NO_SDO_SERVER; i++)
        CO->SDO[i]                      = &COO_SDO[i];
    CO_SDO_ODExtensions                 = &COO_SDO_ODExtensions[0];
    CO->em                              = &COO_EM;
    CO->emPr                            = &COO_EMpr;
    CO->NMT                             = &COO_NMT;
  #if CO_NO_SYNC == 1
    CO->SYNC                            = &COO_SYNC;
  #endif
  #if CO_NO_TIME == 1
    CO->TIME                            = &COO_TIME;
  #endif
    for(i=0; i<CO_NO_RPDO; i++)
        CO->RPDO[i]                     = &COO_RPDO[i];
    for(i=0; i<CO_NO_TPDO; i++)
        CO->TPDO[i]                     = &COO_TPDO[i];
    CO->HBcons                          = &COO_HBcons;
    CO_HBcons_monitoredNodes            = &COO_HBcons_monitoredNodes[0];
  #if CO_NO_LSS_SERVER == 1
    CO->LSSslave                        = &CO0_LSSslave;
  #endif
  #if CO_NO_LSS_CLIENT == 1
    CO->LSSmaster                       = &CO0_LSSmaster;
  #endif
  #if CO_NO_SDO_CLIENT != 0
    for(i=0; i<CO_NO_SDO_CLIENT; i++) {
      CO->SDOclient[i]                  = &COO_SDOclient[i];
    }
  #endif
  #if CO_NO_TRACE > 0
    for(i=0; i<CO_NO_TRACE; i++) {
        CO->trace[i]                    = &COO_trace[i];
        CO_traceTimeBuffers[i]          = &COO_traceTimeBuffers[i][0];
        CO_traceValueBuffers[i]         = &COO_traceValueBuffers[i][0];
        CO_traceBufferSize[i]           = CO_TRACE_BUFFER_SIZE_FIXED;
    }
  #endif
#else
    if(CO == NULL){    /* Use malloc only once */
        CO = &COO;
        CO->CANmodule[0]                    = (CO_CANmodule_t *)    calloc(1, sizeof(CO_CANmodule_t));
        CO_CANmodule_rxArray0               = (CO_CANrx_t *)        calloc(CO_RXCAN_NO_MSGS, sizeof(CO_CANrx_t));
        CO_CANmodule_txArray0               = (CO_CANtx_t *)        calloc(CO_TXCAN_NO_MSGS, sizeof(CO_CANtx_t));
        for(i=0; i<CO_NO_SDO_SERVER; i++){
            CO->SDO[i]                      = (CO_SDO_t *)          calloc(1, sizeof(CO_SDO_t));
        }
        CO_SDO_ODExtensions                 = (CO_OD_extension_t*)  calloc(CO_OD_NoOfElements, sizeof(CO_OD_extension_t));
        CO->em                              = (CO_EM_t *)           calloc(1, sizeof(CO_EM_t));
        CO->emPr                            = (CO_EMpr_t *)         calloc(1, sizeof(CO_EMpr_t));
        CO->NMT                             = (CO_NMT_t *)          calloc(1, sizeof(CO_NMT_t));
      #if CO_NO_SYNC == 1
        CO->SYNC                            = (CO_SYNC_t *)         calloc(1, sizeof(CO_SYNC_t));
      #endif
      #if CO_NO_TIME == 1
        CO->TIME                            = (CO_TIME_t *)         calloc(1, sizeof(CO_TIME_t));
      #endif
        for(i=0; i<CO_NO_RPDO; i++){
            CO->RPDO[i]                     = (CO_RPDO_t *)         calloc(1, sizeof(CO_RPDO_t));
        }
        for(i=0; i<CO_NO_TPDO; i++){
            CO->TPDO[i]                     = (CO_TPDO_t *)         calloc(1, sizeof(CO_TPDO_t));
        }
        CO->HBcons                          = (CO_HBconsumer_t *)   calloc(1, sizeof(CO_HBconsumer_t));
        CO_HBcons_monitoredNodes            = (CO_HBconsNode_t *)   calloc(CO_NO_HB_CONS, sizeof(CO_HBconsNode_t));
      #if CO_NO_LSS_SERVER == 1
        CO->LSSslave                        = (CO_LSSslave_t *)     calloc(1, sizeof(CO_LSSslave_t));
      #endif
      #if CO_NO_LSS_CLIENT == 1
        CO->LSSmaster                       = (CO_LSSmaster_t *)    calloc(1, sizeof(CO_LSSmaster_t));
      #endif
      #if CO_NO_SDO_CLIENT != 0
        for(i=0; i<CO_NO_SDO_CLIENT; i++){
            CO->SDOclient[i]                = (CO_SDOclient_t *)    calloc(1, sizeof(CO_SDOclient_t));
        }
      #endif
      #if CO_NO_TRACE > 0
        for(i=0; i<CO_NO_TRACE; i++) {
            CO->trace[i]                    = (CO_trace_t *)        calloc(1, sizeof(CO_trace_t));
            CO_traceTimeBuffers[i]          = (uint32_t *)          calloc(OD_traceConfig[i].size, sizeof(uint32_t));
            CO_traceValueBuffers[i]         = (int32_t *)           calloc(OD_traceConfig[i].size, sizeof(int32_t));
            if(CO_traceTimeBuffers[i] != NULL && CO_traceValueBuffers[i] != NULL) {
                CO_traceBufferSize[i] = OD_traceConfig[i].size;
            } else {
                CO_traceBufferSize[i] = 0;
            }
        }
      #endif
    }

    CO_memoryUsed = sizeof(CO_CANmodule_t)
                  + sizeof(CO_CANrx_t) * CO_RXCAN_NO_MSGS
                  + sizeof(CO_CANtx_t) * CO_TXCAN_NO_MSGS
                  + sizeof(CO_SDO_t) * CO_NO_SDO_SERVER
                  + sizeof(CO_OD_extension_t) * CO_OD_NoOfElements
                  + sizeof(CO_EM_t)
                  + sizeof(CO_EMpr_t)
                  + sizeof(CO_NMT_t)
  #if CO_NO_SYNC == 1
                  + sizeof(CO_SYNC_t)
  #endif
  #if CO_NO_TIME == 1
                  + sizeof(CO_TIME_t)
  #endif
                  + sizeof(CO_RPDO_t) * CO_NO_RPDO
                  + sizeof(CO_TPDO_t) * CO_NO_TPDO
                  + sizeof(CO_HBconsumer_t)
                  + sizeof(CO_HBconsNode_t) * CO_NO_HB_CONS
  #if CO_NO_LSS_SERVER == 1
                  + sizeof(CO_LSSslave_t)
  #endif
  #if CO_NO_LSS_CLIENT == 1
                  + sizeof(CO_LSSmaster_t)
  #endif
  #if CO_NO_SDO_CLIENT != 0
                  + sizeof(CO_SDOclient_t) * CO_NO_SDO_CLIENT
  #endif
                  + 0;
  #if CO_NO_TRACE > 0
    CO_memoryUsed += sizeof(CO_trace_t) * CO_NO_TRACE;
    for(i=0; i<CO_NO_TRACE; i++) {
        CO_memoryUsed += CO_traceBufferSize[i] * 8;
    }
  #endif

    errCnt = 0;
    if(CO->CANmodule[0]                 == NULL) errCnt++;
    if(CO_CANmodule_rxArray0            == NULL) errCnt++;
    if(CO_CANmodule_txArray0            == NULL) errCnt++;
    for(i=0; i<CO_NO_SDO_SERVER; i++){
        if(CO->SDO[i]                   == NULL) errCnt++;
    }
    if(CO_SDO_ODExtensions              == NULL) errCnt++;
    if(CO->em                           == NULL) errCnt++;
    if(CO->emPr                         == NULL) errCnt++;
    if(CO->NMT                          == NULL) errCnt++;
  #if CO_NO_SYNC == 1
    if(CO->SYNC                         == NULL) errCnt++;
  #endif
  #if CO_NO_TIME == 1
    if(CO->TIME                     	== NULL) errCnt++;
  #endif
    for(i=0; i<CO_NO_RPDO; i++){
        if(CO->RPDO[i]                  == NULL) errCnt++;
    }
    for(i=0; i<CO_NO_TPDO; i++){
        if(CO->TPDO[i]                  == NULL) errCnt++;
    }
    if(CO->HBcons                       == NULL) errCnt++;
    if(CO_HBcons_monitoredNodes         == NULL) errCnt++;
  #if CO_NO_LSS_SERVER == 1
    if(CO->LSSslave                     == NULL) errCnt++;
  #endif
  #if CO_NO_LSS_CLIENT == 1
    if(CO->LSSmaster                    == NULL) errCnt++;
  #endif
  #if CO_NO_SDO_CLIENT != 0
    for(i=0; i<CO_NO_SDO_CLIENT; i++){
        if(CO->SDOclient[i]             == NULL) errCnt++;
    }
  #endif
  #if CO_NO_TRACE > 0
    for(i=0; i<CO_NO_TRACE; i++) {
        if(CO->trace[i]                 == NULL) errCnt++;
    }
  #endif

    if(errCnt != 0) return CO_ERROR_OUT_OF_MEMORY;
#endif
    return CO_ERROR_NO;
}


/******************************************************************************/
CO_ReturnError_t CO_CANinit(
        void                   *CANdriverState,
        uint16_t                bitRate)
{
    CO_ReturnError_t err;

    CO->CANmodule[0]->CANnormal = false;
    CO_CANsetConfigurationMode(CANdriverState);

    err = CO_CANmodule_init(
            CO->CANmodule[0],
            CANdriverState,
            CO_CANmodule_rxArray0,
            CO_RXCAN_NO_MSGS,
            CO_CANmodule_txArray0,
            CO_TXCAN_NO_MSGS,
            bitRate);

    return err;
}


/******************************************************************************/
#if CO_NO_LSS_SERVER == 1
CO_ReturnError_t CO_LSSinit(
        uint8_t                 nodeId,
        uint16_t                bitRate)
{
    CO_LSS_address_t lssAddress;
    CO_ReturnError_t err;

    lssAddress.identity.productCode = OD_identity.productCode;
    lssAddress.identity.revisionNumber = OD_identity.revisionNumber;
    lssAddress.identity.serialNumber = OD_identity.serialNumber;
    lssAddress.identity.vendorID = OD_identity.vendorID;
    err = CO_LSSslave_init(
            CO->LSSslave,
            lssAddress,
            bitRate,
            nodeId,
            CO->CANmodule[0],
            CO_RXCAN_LSS,
            CO_CAN_ID_LSS_SRV,
            CO->CANmodule[0],
            CO_TXCAN_LSS,
            CO_CAN_ID_LSS_CLI);

    return err;
}
#endif /* CO_NO_LSS_SERVER == 1 */


/******************************************************************************/
CO_ReturnError_t CO_CANopenInit(
        uint8_t                 nodeId)
{
    int16_t i;
    CO_ReturnError_t err;

    /* Verify CANopen Node-ID */
    if(nodeId<1 || nodeId>127) {
        return CO_ERROR_PARAMETERS;
    }

    for (i=0; i<CO_NO_SDO_SERVER; i++)
    {
        uint32_t COB_IDClientToServer;
        uint32_t COB_IDServerToClient;
        if(i==0){
            /*Default SDO server must be located at first index*/
            COB_IDClientToServer = CO_CAN_ID_RSDO + nodeId;
            COB_IDServerToClient = CO_CAN_ID_TSDO + nodeId;
        }else{
            COB_IDClientToServer = OD_SDOServerParameter[i].COB_IDClientToServer;
            COB_IDServerToClient = OD_SDOServerParameter[i].COB_IDServerToClient;
        }

        err = CO_SDO_init(
                CO->SDO[i],
                COB_IDClientToServer,
                COB_IDServerToClient,
                OD_H1200_SDO_SERVER_PARAM+i,
                i==0 ? 0 : CO->SDO[0],
               &CO_OD[0],
                CO_OD_NoOfElements,
                CO_SDO_ODExtensions,
                nodeId,
                CO->CANmodule[0],
                CO_RXCAN_SDO_SRV+i,
                CO->CANmodule[0],
                CO_TXCAN_SDO_SRV+i);
    }

    if(err){return err;}


    err = CO_EM_init(
            CO->em,
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

    if(err){return err;}


    err = CO_NMT_init(
            CO->NMT,
            CO->emPr,
            nodeId,
            500,
            CO->CANmodule[0],
            CO_RXCAN_NMT,
            CO_CAN_ID_NMT_SERVICE,
            CO->CANmodule[0],
            CO_TXCAN_HB,
            CO_CAN_ID_HEARTBEAT + nodeId);

    if(err){return err;}


#if CO_NO_NMT_MASTER == 1
    NMTM_txBuff = CO_CANtxBufferInit(/* return pointer to 8-byte CAN data buffer, which should be populated */
            CO->CANmodule[0], /* pointer to CAN module used for sending this message */
            CO_TXCAN_NMT,     /* index of specific buffer inside CAN module */
            0x0000,           /* CAN identifier */
            0,                /* rtr */
            2,                /* number of data bytes */
            0);               /* synchronous message flag bit */
#endif
#if CO_NO_LSS_CLIENT == 1
    err = CO_LSSmaster_init(
            CO->LSSmaster,
            CO_LSSmaster_DEFAULT_TIMEOUT,
            CO->CANmodule[0],
            CO_RXCAN_LSS,
            CO_CAN_ID_LSS_CLI,
            CO->CANmodule[0],
            CO_TXCAN_LSS,
            CO_CAN_ID_LSS_SRV);

    if(err){return err;}

#endif

#if CO_NO_SYNC == 1
    err = CO_SYNC_init(
            CO->SYNC,
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

    if(err){return err;}
#endif

#if CO_NO_TIME == 1
    err = CO_TIME_init(
            CO->TIME,
            CO->em,
            CO->SDO[0],
            &CO->NMT->operatingState,
            OD_COB_ID_TIME,
            0,
            CO->CANmodule[0],
            CO_RXCAN_TIME,
            CO->CANmodule[0],
            CO_TXCAN_TIME);

    if(err){return err;}
#endif

    for(i=0; i<CO_NO_RPDO; i++){
        CO_CANmodule_t *CANdevRx = CO->CANmodule[0];
        uint16_t CANdevRxIdx = CO_RXCAN_RPDO + i;

        err = CO_RPDO_init(
                CO->RPDO[i],
                CO->em,
                CO->SDO[0],
                CO->SYNC,
               &CO->NMT->operatingState,
                nodeId,
                ((i<4) ? (CO_CAN_ID_RPDO_1+i*0x100) : 0),
                0,
                (CO_RPDOCommPar_t*) &OD_RPDOCommunicationParameter[i],
                (CO_RPDOMapPar_t*) &OD_RPDOMappingParameter[i],
                OD_H1400_RXPDO_1_PARAM+i,
                OD_H1600_RXPDO_1_MAPPING+i,
                CANdevRx,
                CANdevRxIdx);

        if(err){return err;}
    }

    for(i=0; i<CO_NO_TPDO; i++){
        err = CO_TPDO_init(
                CO->TPDO[i],
                CO->em,
                CO->SDO[0],
                CO->SYNC,
               &CO->NMT->operatingState,
                nodeId,
                ((i<4) ? (CO_CAN_ID_TPDO_1+i*0x100) : 0),
                0,
                (CO_TPDOCommPar_t*) &OD_TPDOCommunicationParameter[i],
                (CO_TPDOMapPar_t*) &OD_TPDOMappingParameter[i],
                OD_H1800_TXPDO_1_PARAM+i,
                OD_H1A00_TXPDO_1_MAPPING+i,
                CO->CANmodule[0],
                CO_TXCAN_TPDO+i);

        if(err){return err;}
    }


    err = CO_HBconsumer_init(
            CO->HBcons,
            CO->em,
            CO->SDO[0],
           &OD_consumerHeartbeatTime[0],
            CO_HBcons_monitoredNodes,
            CO_NO_HB_CONS,
            CO->CANmodule[0],
            CO_RXCAN_CONS_HB);

    if(err){return err;}


#if CO_NO_SDO_CLIENT != 0

    for(i=0; i<CO_NO_SDO_CLIENT; i++){

        err = CO_SDOclient_init(
                CO->SDOclient[i],
                CO->SDO[0],
                (CO_SDOclientPar_t*) &OD_SDOClientParameter[i],
                CO->CANmodule[0],
                CO_RXCAN_SDO_CLI+i,
                CO->CANmodule[0],
                CO_TXCAN_SDO_CLI+i);

        if(err){return err;}

    }
#endif


#if CO_NO_TRACE > 0
    for(i=0; i<CO_NO_TRACE; i++) {
        CO_trace_init(
            CO->trace[i],
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
CO_ReturnError_t CO_init(
        void                   *CANdriverState,
        uint8_t                 nodeId,
        uint16_t                bitRate)
{
    CO_ReturnError_t err;

    err = CO_new();
    if (err) {
        return err;
    }

    err = CO_CANinit(CANdriverState, bitRate);
    if (err) {
        CO_delete(CANdriverState);
        return err;
    }

    err = CO_CANopenInit(nodeId);
    if (err) {
        CO_delete(CANdriverState);
        return err;
    }

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_delete(void *CANdriverState){
#ifndef CO_USE_GLOBALS
    int16_t i;
#endif

    CO_CANsetConfigurationMode(CANdriverState);
    CO_CANmodule_disable(CO->CANmodule[0]);

#ifndef CO_USE_GLOBALS
  #if CO_NO_TRACE > 0
      for(i=0; i<CO_NO_TRACE; i++) {
          free(CO->trace[i]);
          free(CO_traceTimeBuffers[i]);
          free(CO_traceValueBuffers[i]);
      }
  #endif
  #if CO_NO_SDO_CLIENT != 0
      for(i=0; i<CO_NO_SDO_CLIENT; i++) {
          free(CO->SDOclient[i]);
      }
  #endif
  #if CO_NO_LSS_SERVER == 1
    free(CO->LSSslave);
  #endif
  #if CO_NO_LSS_CLIENT == 1
    free(CO->LSSmaster);
  #endif
    free(CO_HBcons_monitoredNodes);
    free(CO->HBcons);
    for(i=0; i<CO_NO_RPDO; i++){
        free(CO->RPDO[i]);
    }
    for(i=0; i<CO_NO_TPDO; i++){
        free(CO->TPDO[i]);
    }
  #if CO_NO_SYNC == 1
    free(CO->SYNC);
  #endif
  #if CO_NO_TIME == 1
    free(CO->TIME);
  #endif
    free(CO->NMT);
    free(CO->emPr);
    free(CO->em);
    free(CO_SDO_ODExtensions);
    for(i=0; i<CO_NO_SDO_SERVER; i++){
        free(CO->SDO[i]);
    }
    free(CO_CANmodule_txArray0);
    free(CO_CANmodule_rxArray0);
    free(CO->CANmodule[0]);
    CO = NULL;
#endif
}


/******************************************************************************/
CO_NMT_reset_cmd_t CO_process(
        CO_t                   *co,
        uint16_t                timeDifference_ms,
        uint16_t               *timerNext_ms)
{
    uint8_t i;
    bool_t NMTisPreOrOperational = false;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
#ifdef CO_USE_LEDS
    static uint16_t ms50 = 0;
#endif /* CO_USE_LEDS */

    if(co->NMT->operatingState == CO_NMT_PRE_OPERATIONAL || co->NMT->operatingState == CO_NMT_OPERATIONAL)
        NMTisPreOrOperational = true;

#ifdef CO_USE_LEDS
    ms50 += timeDifference_ms;
    if(ms50 >= 50){
        ms50 -= 50;
        CO_NMT_blinkingProcess50ms(co->NMT);
    }
#endif /* CO_USE_LEDS */

    for(i=0; i<CO_NO_SDO_SERVER; i++){
        CO_SDO_process(
                co->SDO[i],
                NMTisPreOrOperational,
                timeDifference_ms,
                1000,
                timerNext_ms);
    }

    CO_EM_process(
            co->emPr,
            NMTisPreOrOperational,
            timeDifference_ms * 10,
            OD_inhibitTimeEMCY,
            timerNext_ms);


    reset = CO_NMT_process(
            co->NMT,
            timeDifference_ms,
            OD_producerHeartbeatTime,
            OD_NMTStartup,
            OD_errorRegister,
            OD_errorBehavior,
            timerNext_ms);


    CO_HBconsumer_process(
            co->HBcons,
            NMTisPreOrOperational,
            timeDifference_ms);

#if CO_NO_TIME == 1
    CO_TIME_process(
            co->TIME,
            timeDifference_ms);
#endif

    return reset;
}


/******************************************************************************/
#if CO_NO_SYNC == 1
bool_t CO_process_SYNC(
        CO_t                   *co,
        uint32_t                timeDifference_us)
{
    bool_t syncWas = false;

    switch(CO_SYNC_process(co->SYNC, timeDifference_us, OD_synchronousWindowLength)){
        case 1:     //immediately after the SYNC message
            syncWas = true;
            break;
        case 2:     //outside SYNC window
            CO_CANclearPendingSyncPDOs(co->CANmodule[0]);
            break;
    }

    return syncWas;
}
#endif

/******************************************************************************/
void CO_process_RPDO(
        CO_t                   *co,
        bool_t                  syncWas)
{
    int16_t i;

    for(i=0; i<CO_NO_RPDO; i++){
        CO_RPDO_process(co->RPDO[i], syncWas);
    }
}


/******************************************************************************/
void CO_process_TPDO(
        CO_t                   *co,
        bool_t                  syncWas,
        uint32_t                timeDifference_us)
{
    int16_t i;

    /* Verify PDO Change Of State and process PDOs */
    for(i=0; i<CO_NO_TPDO; i++){
        if(!co->TPDO[i]->sendRequest)
            co->TPDO[i]->sendRequest = CO_TPDOisCOS(co->TPDO[i]);
        CO_TPDO_process(co->TPDO[i], syncWas, timeDifference_us);
    }
}
