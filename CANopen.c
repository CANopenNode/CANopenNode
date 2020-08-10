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
//extern const CO_OD_entry_t CO_OD[CO_OD_NoOfElements];   /* Object Dictionary */
#if defined CO_USE_GLOBALS || MULTIPLE_OD == 0
CO_t COO;                    /* Pointers to CANopen objects */
CO_t *CO = NULL;                    /* Pointer to COO */
#endif

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

#if MULTIPLE_OD==0
#if    CO_NO_SYNC                                 >  1     \
    || (CO_NO_SDO_SERVER < 1 || CO_NO_SDO_SERVER > 128)    \
    || CO_NO_TIME                                 >  1     \
    || CO_NO_SDO_CLIENT                           > 128    \
    || CO_NO_GFC                                  >  1     \
    || CO_NO_SRDO                                 > 64     \
    || (CO_NO_RPDO < 1 || CO_NO_RPDO > 0x200)              \
    || (CO_NO_TPDO < 1 || CO_NO_TPDO > 0x200)              \
    || ODL_consumerHeartbeatTime_arrayLength      == 0     \
    || ODL_errorStatusBits_stringLength           < 10     \
    || CO_NO_LSS_SLAVE                            >  1     \
    || CO_NO_LSS_MASTER                           >  1
#error Features from CO_OD.h file are not corectly configured for this project!
#endif
#endif

#ifndef CO_NO_HB_PROD
  #define CO_NO_HB_PROD      1                                      /*  Producer Heartbeat Cont */
#endif

/* Indexes of CO_CANrx_t objects in CO_CANmodule_t and total number of them. **/
#ifdef CO_USE_GLOBALS

#ifndef CO_NO_GFC
#define CO_NO_GFC 0
#endif

#ifndef CO_NO_SRDO
#define CO_NO_SRDO 0
#endif

#ifndef CO_NO_HB_CONS
#ifdef ODL_consumerHeartbeatTime_arrayLength
#define CO_NO_HB_CONS ODL_consumerHeartbeatTime_arrayLength
#else
#define CO_NO_HB_CONS 0
#endif
#endif

#define CO_RXCAN_NMT(cnst)      0
#define CO_RXCAN_SYNC(cnst)    (CO_RXCAN_NMT(cnst)      + CO_NO_NMT)
#define CO_RXCAN_EMERG(cnst)   (CO_RXCAN_SYNC(cnst)     + CO_NO_SYNC)
#define CO_RXCAN_TIME(cnst)    (CO_RXCAN_EMERG(cnst)    + CO_NO_EM_CONS)
#define CO_RXCAN_GFC(cnst)     (CO_RXCAN_TIME(cnst)     + CO_NO_TIME)
#define CO_RXCAN_SRDO(cnst)    (CO_RXCAN_GFC(cnst)      + CO_NO_GFC)
#define CO_RXCAN_RPDO(cnst)    (CO_RXCAN_SRDO(cnst)     + CO_NO_SRDO*2)
#define CO_RXCAN_SDO_SRV(cnst) (CO_RXCAN_RPDO(cnst)     + CO_NO_RPDO)
#define CO_RXCAN_SDO_CLI(cnst) (CO_RXCAN_SDO_SRV(cnst)  + CO_NO_SDO_SERVER)
#define CO_RXCAN_CONS_HB(cnst) (CO_RXCAN_SDO_CLI(cnst)  + CO_NO_SDO_CLIENT)
#define CO_RXCAN_LSS_SLV(cnst) (CO_RXCAN_CONS_HB(cnst)  + CO_NO_HB_CONS)
#define CO_RXCAN_LSS_MST(cnst) (CO_RXCAN_LSS_SLV(cnst)  + CO_NO_LSS_SLAVE)
#define CO_RXCAN_NO_MSGS(cnst) ( \
		                  CO_NO_NMT       + \
                          CO_NO_SYNC        + \
                          CO_NO_EM_CONS     + \
                          CO_NO_TIME        + \
                          CO_NO_GFC         + \
                          CO_NO_SRDO*2      + \
                          CO_NO_RPDO        + \
                          CO_NO_SDO_SERVER  + \
                          CO_NO_SDO_CLIENT  + \
                          CO_NO_HB_CONS     + \
                          CO_NO_LSS_SLAVE   + \
                          CO_NO_LSS_MASTER)

/* Indexes of CO_CANtx_t objects in CO_CANmodule_t and total number of them. **/
#define CO_TXCAN_NMT(cnst)      0
#define CO_TXCAN_SYNC(cnst)    (CO_TXCAN_NMT(cnst)      + CO_NO_NMT_MST)
#define CO_TXCAN_EMERG(cnst)   (CO_TXCAN_SYNC(cnst)     + CO_NO_SYNC)
#define CO_TXCAN_TIME(cnst)    (CO_TXCAN_EMERG(cnst)    + CO_NO_EMERGENCY)
#define CO_TXCAN_GFC(cnst)     (CO_TXCAN_TIME(cnst)     + CO_NO_TIME)
#define CO_TXCAN_SRDO(cnst)    (CO_TXCAN_GFC(cnst)      + CO_NO_GFC)
#define CO_TXCAN_TPDO(cnst)    (CO_TXCAN_SRDO(cnst)     + CO_NO_SRDO*2)
#define CO_TXCAN_SDO_SRV(cnst) (CO_TXCAN_TPDO(cnst)     + CO_NO_TPDO)
#define CO_TXCAN_SDO_CLI(cnst) (CO_TXCAN_SDO_SRV(cnst)  + CO_NO_SDO_SERVER)
#define CO_TXCAN_HB(cnst)      (CO_TXCAN_SDO_CLI(cnst)  + CO_NO_SDO_CLIENT)
#define CO_TXCAN_LSS_SLV(cnst) (CO_TXCAN_HB(cnst)       + CO_NO_HB_PROD)
#define CO_TXCAN_LSS_MST(cnst) (CO_TXCAN_LSS_SLV  + CO_NO_LSS_SLAVE)
#define CO_TXCAN_NO_MSGS(cnst) (CO_NO_NMT_MST     + \
                          CO_NO_SYNC        + \
                          CO_NO_EMERGENCY   + \
                          CO_NO_TIME        + \
                          CO_NO_GFC         + \
                          CO_NO_SRDO*2      + \
                          CO_NO_TPDO        + \
                          CO_NO_SDO_SERVER  + \
                          CO_NO_SDO_CLIENT  + \
                          CO_NO_HB_PROD     + \
                          CO_NO_LSS_SLAVE   + \
                          CO_NO_LSS_MASTER)

#else
#define CO_RXCAN_NMT(cnst)      0
#define CO_RXCAN_SYNC(cnst)    (CO_RXCAN_NMT(cnst)      + 1)
#define CO_RXCAN_EMERG(cnst)   (CO_RXCAN_SYNC(cnst)     + (cnst).NO_SYNC)
#define CO_RXCAN_TIME(cnst)    (CO_RXCAN_EMERG(cnst)    + CO_NO_EM_CONS)
#define CO_RXCAN_GFC(cnst)     (CO_RXCAN_TIME(cnst)     + (cnst).NO_TIME)
#define CO_RXCAN_SRDO(cnst)    (CO_RXCAN_GFC(cnst)      + (cnst).NO_GFC)
#define CO_RXCAN_RPDO(cnst)    (CO_RXCAN_SRDO(cnst)     + (cnst).NO_SRDO*2)
#define CO_RXCAN_SDO_SRV(cnst) (CO_RXCAN_RPDO(cnst)     + (cnst).NO_RPDO)
#define CO_RXCAN_SDO_CLI(cnst) (CO_RXCAN_SDO_SRV(cnst)  + (cnst).NO_SDO_SERVER)
#define CO_RXCAN_CONS_HB(cnst) (CO_RXCAN_SDO_CLI(cnst)  + (cnst).NO_SDO_CLIENT)
#define CO_RXCAN_LSS_SLV(cnst) (CO_RXCAN_CONS_HB(cnst)  + (cnst).consumerHeartbeatTime_arrayLength)
#define CO_RXCAN_LSS_MST(cnst) (CO_RXCAN_LSS_SLV(cnst)  + (cnst).NO_LSS_SLAVE)
#define CO_RXCAN_NO_MSGS(cnst) ( \
		1        + \
		(cnst).NO_SYNC        + \
		CO_NO_EM_CONS     + \
		(cnst).NO_TIME        + \
		(cnst).NO_GFC         + \
		(cnst).NO_SRDO*2      + \
		(cnst).NO_RPDO        + \
		(cnst).NO_SDO_SERVER  + \
		(cnst).NO_SDO_CLIENT  + \
		(cnst).consumerHeartbeatTime_arrayLength     + \
		(cnst).NO_LSS_SERVER   + \
		(cnst).NO_LSS_CLIENT)

/* Indexes of CO_CANtx_t objects in CO_CANmodule_t and total number of them. **/
#define CO_TXCAN_NMT(cnst)      0
#define CO_TXCAN_SYNC(cnst)    (CO_TXCAN_NMT(cnst)      + (cnst).NO_NMT_MASTER)
#define CO_TXCAN_EMERG(cnst)   (CO_TXCAN_SYNC(cnst)     + (cnst).NO_SYNC)
#define CO_TXCAN_TIME(cnst)    (CO_TXCAN_EMERG(cnst)    + (cnst).NO_EMERGENCY)
#define CO_TXCAN_GFC(cnst)     (CO_TXCAN_TIME(cnst)     + (cnst).NO_TIME)
#define CO_TXCAN_SRDO(cnst)    (CO_TXCAN_GFC(cnst)      + (cnst).NO_GFC)
#define CO_TXCAN_TPDO(cnst)    (CO_TXCAN_SRDO(cnst)     + (cnst).NO_SRDO*2)
#define CO_TXCAN_SDO_SRV(cnst) (CO_TXCAN_TPDO(cnst)     + (cnst).NO_TPDO)
#define CO_TXCAN_SDO_CLI(cnst) (CO_TXCAN_SDO_SRV(cnst)  + (cnst).NO_SDO_SERVER)
#define CO_TXCAN_HB(cnst)      (CO_TXCAN_SDO_CLI(cnst)  + (cnst).NO_SDO_CLIENT)
#define CO_TXCAN_LSS_SLV(cnst) (CO_TXCAN_HB(cnst)       + CO_NO_HB_PROD)
#define CO_TXCAN_LSS_MST(cnst) (CO_TXCAN_LSS_SLV  + CO_NO_LSS_SLAVE)
#define CO_TXCAN_NO_MSGS(cnst) (\
		(cnst).NO_NMT_MASTER    + \
		(cnst).NO_SYNC        + \
		(cnst).NO_EMERGENCY   + \
		(cnst).NO_TIME        + \
		(cnst).NO_GFC         + \
		((cnst).NO_SRDO)*2      + \
		(cnst).NO_TPDO        + \
		(cnst).NO_SDO_SERVER  + \
		(cnst).NO_SDO_CLIENT  + \
		CO_NO_HB_PROD     + \
		(cnst).NO_SDO_SERVER   + \
		(cnst).NO_SDO_CLIENT)

#endif
/* Create objects from heap ***************************************************/
#ifndef CO_USE_GLOBALS
CO_ReturnError_t CO_newEx(CO_t *CO,uint32_t *heapMemoryUsed) {
    int16_t i;
    uint16_t errCnt = 0;
    uint32_t CO_memoryUsed = 0;

    /* If CANopen was initialized before, return. */
    if(CO != NULL && CO->SDO!=NULL) {
        return CO_ERROR_NO;
    }

    /* CANmodule */
    CO->CANmodule[0] = (CO_CANmodule_t *)calloc(1, sizeof(CO_CANmodule_t));
    if (CO->CANmodule[0] == NULL) errCnt++;
    CO->CANmodule_rxArray0 =
        (CO_CANrx_t *)calloc(CO_RXCAN_NO_MSGS(*(CO->consts)), sizeof(CO_CANrx_t));
    if (CO->CANmodule_rxArray0 == NULL) errCnt++;
    CO->CANmodule_txArray0 =
        (CO_CANtx_t *)calloc(CO_TXCAN_NO_MSGS(*(CO->consts)), sizeof(CO_CANtx_t));
    if (CO->CANmodule_txArray0 == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_CANmodule_t) +
                     sizeof(CO_CANrx_t) * CO_RXCAN_NO_MSGS(*(CO->consts)) +
                     sizeof(CO_CANtx_t) * CO_TXCAN_NO_MSGS(*(CO->consts));

    /* SDOserver */
    if (CO->consts->NO_SDO_SERVER > 0){
        CO->SDO = (CO_SDO_t *)calloc(CO->consts->NO_SDO_SERVER, sizeof(CO_SDO_t));
        if (CO->SDO==NULL) errCnt++;
        CO->SDO_ODExtensions = (CO_OD_extension_t *)calloc(CO->consts->OD_NoOfElements, sizeof(CO_OD_extension_t));
        if (CO->SDO_ODExtensions == NULL) errCnt++;
    }
    CO_memoryUsed += sizeof(CO_SDO_t) * CO->consts->NO_SDO_SERVER +
                     sizeof(CO_OD_extension_t) * CO->consts->OD_NoOfElements;

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

    if (CO->consts->NO_SYNC == 1) {
        CO->SYNC = (CO_SYNC_t *)calloc(1, sizeof(CO_SYNC_t));
  		if (CO->SYNC == NULL) errCnt++;
		CO_memoryUsed += sizeof(CO_SYNC_t);
    }

    if (CO->consts->NO_TIME == 1) {
    /* TIME */
    	CO->TIME = (CO_TIME_t *)calloc(1, sizeof(CO_TIME_t));
		if (CO->TIME == NULL) errCnt++;
		CO_memoryUsed += sizeof(CO_TIME_t);
	} else {
		CO->TIME = NULL;
	}

#if CO_NO_GFC == 1
    CO->GFC = (CO_GFC_t *)calloc(1, sizeof(CO_GFC_t));
    if (CO->GFC == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_GFC_t);
#endif

#if CO_NO_SRDO != 0
    /* SRDO */
    CO->SRDOGuard = (CO_SRDOGuard_t *)calloc(1, sizeof(CO_SRDOGuard_t));
    if (CO->SRDOGuard == NULL) errCnt++;
    for (i = 0; i < CO_NO_SRDO; i++) {
        CO->SRDO[i] = (CO_SRDO_t *)calloc(1, sizeof(CO_SRDO_t));
        if (CO->SRDO[i] == NULL) errCnt++;
    }
    CO_memoryUsed += sizeof(CO_SRDO_t) * CO_NO_SRDO + sizeof(CO_SRDOGuard_t);
#endif
    /* RPDO */
    if (CO->consts->NO_RPDO>0) {
  	  CO->RPDO                     = (CO_RPDO_t *)         calloc(CO->consts->NO_RPDO, sizeof(CO_RPDO_t));
      if (CO->RPDO == NULL) errCnt++;
    } else {
  	  CO->RPDO=NULL;
    }
    CO_memoryUsed += sizeof(CO_RPDO_t) * CO->consts->NO_RPDO;

    /* TPDO */
    if (CO->consts->NO_TPDO){
        CO->TPDO                     = (CO_TPDO_t *)         calloc(CO->consts->NO_TPDO, sizeof(CO_TPDO_t));
        if (CO->TPDO == NULL) errCnt++;
    } else {
      	CO->TPDO=NULL;
    }
    CO_memoryUsed += sizeof(CO_TPDO_t) * CO->consts->NO_TPDO;

    /* Heartbeat consumer */
    CO->HBcons = (CO_HBconsumer_t *)calloc(1, sizeof(CO_HBconsumer_t));
    if (CO->HBcons == NULL) errCnt++;
    CO->HBcons_monitoredNodes = (CO_HBconsNode_t *)   calloc(CO->consts->consumerHeartbeatTime_arrayLength, sizeof(CO_HBconsNode_t));
    if (CO->HBcons_monitoredNodes == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_HBconsumer_t) +
                     sizeof(CO_HBconsNode_t) * CO->consts->consumerHeartbeatTime_arrayLength;

#if CO_NO_SDO_CLIENT != 0 || defined CO_DOXYGEN|| MULTIPLE_OD == 1
    if (CO->consts->NO_SDO_CLIENT != 0) {
          CO->SDOclient                = (CO_SDOclient_t *)    calloc(CO->consts->NO_SDO_CLIENT, sizeof(CO_SDOclient_t));
          if (CO->SDOclient == NULL) errCnt++;
    } else {
      	CO->SDOclient=NULL;
    }
    CO_memoryUsed += sizeof(CO_SDOclient_t) * CO->consts->NO_SDO_CLIENT;
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    /* LEDs */
    CO->LEDs = (CO_LEDs_t *)calloc(1, sizeof(CO_LEDs_t));
    if (CO->LEDs == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_LEDs_t);
#endif

#if CO_NO_LSS_SLAVE == 1 || defined CO_DOXYGEN || MULTIPLE_OD == 1
    /* LSSslave */
    if (CO->consts->NO_LSS_CLIENT == 1) {
      CO->LSSslave                        = (CO_LSSslave_t *)     calloc(1, sizeof(CO_LSSslave_t));
      if (CO->LSSslave == NULL) errCnt++;
    }
    else
      CO->LSSslave = NULL;
    CO_memoryUsed += sizeof(CO_LSSslave_t);
#endif

#if CO_NO_LSS_MASTER == 1 || defined CO_DOXYGEN || MULTIPLE_OD == 1
    /* LSSmaster */
    if (CO->consts->NO_LSS_CLIENT == 1) {
      CO->LSSmaster                       = (CO_LSSmaster_t *)    calloc(1, sizeof(CO_LSSmaster_t));
      if (CO->LSSmaster == NULL) errCnt++;
    }
    else
      CO->LSSmaster=NULL;
    CO_memoryUsed += sizeof(CO_LSSmaster_t);
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /* Gateway-ascii */
    CO->gtwa = (CO_GTWA_t *)calloc(1, sizeof(CO_GTWA_t));
    if (CO->gtwa == NULL) errCnt++;
    CO_memoryUsed += sizeof(CO_GTWA_t);
#endif

#ifdef TRACE
      if (CO->consts->NO_TRACE > 0) {
      	CO->trace=calloc(CO->consts->NO_TRACE, sizeof(CO_trace_t));
      	CO->traceTimeBuffers=calloc(CO->consts->NO_TRACE, sizeof(uint32_t));
      	CO->traceValueBuffers=calloc(CO->consts->NO_TRACE, sizeof(int32_t));
      	CO->traceBufferSize=calloc(CO->consts->NO_TRACE, sizeof(uint32_t));
        CO_memoryUsed += CO->consts->NO_TRACE * sizeof(uint32_t) * 4;
      	if (CO->traceTimeBuffers && CO->traceValueBuffers && CO->traceBufferSize) {
        for(int i=0; i<CO->NO_TRACE; i++) {
            CO->trace[i]                    = (CO_trace_t *)        calloc(1, sizeof(CO_trace_t));
            if (!CO->trace[i]) errCnt++;
            CO->traceTimeBuffers[i]          = (uint32_t *)          calloc(CO->traceConfig[i].size, sizeof(uint32_t));
            if (!CO->traceTimeBuffers[i]) errCnt++;
            CO->traceValueBuffers[i]         = (int32_t *)           calloc(CO->traceConfig[i].size, sizeof(int32_t));
            if (!CO->traceValueBuffers[i]) errCnt++;
            CO_memoryUsed += CO->traceConfig[i].size[i] * sizeof(uint32_t) * 2 + sizeof(CO_trace_t);
            if(CO->traceTimeBuffers[i] != NULL && CO->traceValueBuffers[i] != NULL) {
                CO->traceBufferSize[i] = CO->traceConfig[i].size;
            } else {
                CO->traceBufferSize[i] = 0;
            }
        }
      	} else {
      		 errCnt++;
      	}

    }
#endif

    if(heapMemoryUsed != NULL) {
        *heapMemoryUsed = CO_memoryUsed;
    }

    return (errCnt == 0) ? CO_ERROR_NO : CO_ERROR_OUT_OF_MEMORY;
}


/******************************************************************************/
void CO_deleteEx(CO_t* CO, void *CANptr) {
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
    free(CO->HBcons_monitoredNodes);
    free(CO->HBcons);

#if CO_NO_GFC == 1
    /* GFC */
    free(CO->GFC);
#endif

#if CO_NO_SRDO != 0
    /* SRDO */
    for (i = 0; i < CO_NO_SRDO; i++) {
        free(CO->SRDO[i]);
    }
    free(CO->SRDOGuard);
#endif

    /* TPDO */
    free(CO->TPDO);

    /* RPDO */
    free(CO->RPDO);

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
    free(CO->SDO_ODExtensions);
    free(CO->SDO);

    /* CANmodule */
    free(CO->CANmodule_txArray0);
    free(CO->CANmodule_rxArray0);
    free(CO->CANmodule[0]);

    /* globals */
    CO = NULL;
}
#endif /* #ifndef CO_USE_GLOBALS */


/* Alternatively create objects as globals ************************************/
#ifdef CO_USE_GLOBALS
    static CO_CANmodule_t       COO_CANmodule;
    static CO_CANrx_t           COO_CANmodule_rxArray0[CO_RXCAN_NO_MSGS(&CO_Consts)];
    static CO_CANtx_t           COO_CANmodule_txArray0[CO_TXCAN_NO_MSGS(&CO_Consts)];
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
#if CO_NO_GFC == 1
    static CO_GFC_t             COO_GFC;
#endif
#if CO_NO_SRDO != 0
    static CO_SRDOGuard_t       COO_SRDOGuard;
    static CO_SRDO_t            COO_SRDO[CO_NO_SRDO];
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
#if CO_NO_TRACE > 0 || defined TRACE
  #ifndef CO_TRACE_BUFFER_SIZE_FIXED
    #define CO_TRACE_BUFFER_SIZE_FIXED 100
  #endif
    static CO_trace_t           COO_trace[CO_NO_TRACE];
    static uint32_t             COO_traceTimeBuffers[CO_NO_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
    static int32_t              COO_traceValueBuffers[CO_NO_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
#endif

CO_ReturnError_t CO_newEx(CO_t* CO, void *CANptr) {
    int16_t i;

    /* If CANopen was initialized before, return. */
    if (CO == NULL) {
        return CO_ERROR_NO;
    }

    /* globals */
    CO = &COO;

    /* CANmodule */
    CO->CANmodule[0] = &COO_CANmodule;
    CO->CANmodule_rxArray0 = &COO_CANmodule_rxArray0[0];
    CO->CANmodule_txArray0 = &COO_CANmodule_txArray0[0];

    /* SDOserver */
    CO->SDO = COO_SDO;
    CO->SDO_ODExtensions = &COO_SDO_ODExtensions[0];

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

#if CO_NO_GFC == 1
    /* GFC */
    CO->GFC = &COO_GFC;
#endif

#if CO_NO_SRDO != 0
    /* SRDO */
    CO->SRDOGuard = &COO_SRDOGuard;
    CO->SRDO = COO_SRDO;
#endif

    /* RPDO */
    CO->RPDO=COO_RPDO;

    /* TPDO */
    CO->TPDO = COO_TPDO;

    /* Heartbeat consumer */
#if CO_NO_HB_CONS>0
	CO->HBcons = &COO_HBcons;
    CO_HBcons_monitoredNodes = COO_HBcons_monitoredNodes;
#endif

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

#if defined TRACE && CO_NO_TRACE>0
    /* Trace */
    for (i = 0; i < CO_NO_TRACE; i++) {
        CO->trace[i] = &COO_trace[i];
        CO->traceTimeBuffers[i] = &COO_traceTimeBuffers[i][0];
        CO->traceValueBuffers[i] = &COO_traceValueBuffers[i][0];
        CO->traceBufferSize[i] = CO_TRACE_BUFFER_SIZE_FIXED;
    }
#endif

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_deleteEx(CO_t CO,  void *CANptr) {
    CO_CANsetConfigurationMode(CANptr);

    /* If CANopen isn't initialized, return. */
    if (CO == NULL) {
        return;
    }

    CO_CANmodule_disable(CO->CANmodule[0]);

    /* globals */
    CO = NULL;
}
#endif /* #ifdef CO_USE_GLOBALS */

#if MULTIPLE_OD == 0
void CO_delete( void *CANptr) {
  CO_deleteEx(&COO, CANptr);
}
#endif

/******************************************************************************/
CO_ReturnError_t CO_CANinitEx(CO_t *CO, void *CANptr, uint16_t bitRate)
{
	CO_ReturnError_t err;

	CO->CANmodule[0]->CANnormal = false;
	CO_CANsetConfigurationMode(CANptr);

	err = CO_CANmodule_init(
			CO->CANmodule[0],
			CANptr,
			CO->CANmodule_rxArray0,
			CO_RXCAN_NO_MSGS(*(CO->consts)),
			CO->CANmodule_txArray0,
			CO_TXCAN_NO_MSGS(*(CO->consts)),
			bitRate);

	return err;
}


/******************************************************************************/
#if CO_NO_LSS_SLAVE == 1 || MULTIPLE_OD == 1
CO_ReturnError_t CO_LSSinitEx(CO_t *CO,
        					uint8_t                 nodeId,
							uint16_t                bitRate)
{
    CO_LSS_address_t lssAddress;
    CO_ReturnError_t err;
    int x=CO_OD_find(CO->SDO,0x1018);
    if (x>=0xffff)
    	return CO_ERROR_ILLEGAL_ARGUMENT;
    uint32_t* dataPtr=CO_OD_getDataPointer(CO->SDO,x,2);
    if (dataPtr==NULL)
    	return CO_ERROR_ILLEGAL_ARGUMENT;
    lssAddress.identity.productCode = *dataPtr;
    dataPtr=CO_OD_getDataPointer(CO->SDO,x,3);
    if (dataPtr==NULL)
    	return CO_ERROR_ILLEGAL_ARGUMENT;
    lssAddress.identity.revisionNumber = *dataPtr;
    dataPtr=CO_OD_getDataPointer(CO->SDO,x,4);
    if (dataPtr==NULL)
    	return CO_ERROR_ILLEGAL_ARGUMENT;
    lssAddress.identity.serialNumber = *dataPtr;
    dataPtr=CO_OD_getDataPointer(CO->SDO,x,1);
    if (dataPtr==NULL)
    	return CO_ERROR_ILLEGAL_ARGUMENT;
    lssAddress.identity.vendorID = *dataPtr;
    err = CO_LSSslave_init(
            CO->LSSslave,
            &lssAddress,
            bitRate,
            nodeId,
            CO->CANmodule[0],
			CO_RXCAN_LSS_SLV(*(CO->consts)),
            CO_CAN_ID_LSS_SRV,
            CO->CANmodule[0],
			CO_TXCAN_LSS_SLV(*(CO->consts)),
            CO_CAN_ID_LSS_CLI);

    return err;
}
#endif /* CO_NO_LSS_SLAVE == 1 */


/******************************************************************************/
CO_ReturnError_t CO_CANopenInitEx(CO_t *CO, const CO_OD_entry_t *CO_OD, uint8_t nodeId) {
    int16_t i;
    CO_ReturnError_t err;
    uint16_t entry;

    /* Verify CANopen Node-ID*/
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    CO->nodeIdUnconfigured = false;
    		if (CO->consts->NO_LSS_SERVER == 1) {
		if (nodeId == CO_LSS_NODE_ID_ASSIGNMENT) {
			CO->nodeIdUnconfigured = true;
		}
	else if (nodeId < 1 || nodeId > 127) {
        return CO_ERROR_PARAMETERS;
    }
	}
#else
    CO->nodeIdUnconfigured = false;
#endif
	CO->SDO->OD=CO_OD;
	CO->SDO->ODSize=CO->consts->OD_NoOfElements;
	entry=CO_OD_find(CO->SDO,0x1015);
	if (entry!=0xffff)
		CO->inhibitTimeEMCY=*(uint16_t*)CO_OD_getDataPointer(CO->SDO,entry,0); /*1015, Data Type: UNSIGNED16 */
	else
		CO->inhibitTimeEMCY=10;
	entry=CO_OD_find(CO->SDO,0x1017);
	if (entry!=0xffff)
		CO->producerHeartbeatTime=*(uint16_t*)CO_OD_getDataPointer(CO->SDO,entry,0); /*1017, Data Type: UNSIGNED16 */
	else
		CO->producerHeartbeatTime=500; /*1017, Data Type: UNSIGNED16 */
	entry=CO_OD_find(CO->SDO,0x1F80);
	if (entry!=0xffff)
		CO->NMTStartup=*(uint32_t*)CO_OD_getDataPointer(CO->SDO,entry,0);/*1F80, Data Type: UNSIGNED32 */
	else
		CO->NMTStartup=0;
	entry=CO_OD_find(CO->SDO,0x1001);
	if (entry!=0xffff)
		CO->errorRegister=(uint8_t*)CO_OD_getDataPointer(CO->SDO,entry,0); /*1001, Data Type: UNSIGNED8 */
	else {
		CO->errorRegister=NULL;
		// TODO: This should never happen. 0x1001 errorRegister is not optional.
		// Maybe CO_t should have a errorregister_var field we could point at.
	}
	entry=CO_OD_find(CO->SDO,0x1029);
	if (entry!=0xffff)
		CO->errorBehavior=(uint8_t*)CO_OD_getDataPointer(CO->SDO,entry,0);/*1029, Data Type: UNSIGNED8, Array[6] */
	else
		CO->errorBehavior=NULL;
    /* Verify parameters from CO_OD */
#if CO_NO_SRDO != 0
    if (sizeof(OD_SRDOCommunicationParameter_t) != sizeof(CO_SRDOCommPar_t) ||
        sizeof(OD_SRDOMappingParameter_t)       != sizeof(CO_SRDOMapPar_t)) {
        return CO_ERROR_PARAMETERS;
    }
#endif
    if(   (CO->consts->sizeof_OD_TPDOCommunicationParameter != sizeof(CO_TPDOCommPar_t)  && CO->consts->sizeof_OD_TPDOCommunicationParameter!=0)
        || (CO->consts->sizeof_OD_TPDOMappingParameter != sizeof(CO_TPDOMapPar_t)        &&  CO->consts->sizeof_OD_TPDOMappingParameter != 0)
        || (CO->consts->sizeof_OD_RPDOCommunicationParameter != sizeof(CO_RPDOCommPar_t) && CO->consts->sizeof_OD_RPDOCommunicationParameter!=0)
        || (CO->consts->sizeof_OD_RPDOMappingParameter != sizeof(CO_RPDOMapPar_t)        && CO->consts->sizeof_OD_RPDOMappingParameter!=0)
 	   )
     {
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

    if (CO->consts->NO_LSS_SERVER == 1) {
		if (CO->nodeIdUnconfigured) {
			return CO_ERROR_NODE_ID_UNCONFIGURED_LSS;
		}
	}

    /* SDOserver */
    for (i = 0; i < CO->consts->NO_SDO_SERVER; i++) {
        uint32_t COB_IDClientToServer;
        uint32_t COB_IDServerToClient;
        if(i==0){
            /*Default SDO server must be located at first index*/
            COB_IDClientToServer = CO_CAN_ID_RSDO + nodeId;
            COB_IDServerToClient = CO_CAN_ID_TSDO + nodeId;
        }else{
        	uint16_t entry=CO_OD_find(CO->SDO,0x1200+i);
        	uint32_t *dp=CO_OD_getDataPointer(CO->SDO,entry,1);
            COB_IDClientToServer = *dp;
        	dp=CO_OD_getDataPointer(CO->SDO,entry,2);
            COB_IDServerToClient = *dp;
        }

        err = CO_SDO_init(&CO->SDO[i],
                          COB_IDClientToServer,
                          COB_IDServerToClient,
                          OD_H1200_SDO_SERVER_PARAM + i,
                          i == 0 ? NULL : &CO->SDO[0],
                          CO_OD,
						  CO->consts->OD_NoOfElements,
                          CO->SDO_ODExtensions,
                          nodeId,
                          1000,
                          CO->CANmodule[0],
                          CO_RXCAN_SDO_SRV((*(CO->consts)))+i,
                          CO->CANmodule[0],
                          CO_TXCAN_SDO_SRV((*(CO->consts)))+i);
//        CO_ReturnError_t CO_SDO_init(
//                CO_SDO_t               *SDO,
//                uint32_t                COB_IDClientToServer,
//                uint32_t                COB_IDServerToClient,
//                uint16_t                ObjDictIndex_SDOServerParameter,
//                CO_SDO_t               *parentSDO,
//                const CO_OD_entry_t     OD[],
//                uint16_t                ODSize,
//                CO_OD_extension_t      *ODExtensions,
//                uint8_t                 nodeId,
//                uint16_t                SDOtimeoutTime_ms,
//                CO_CANmodule_t         *CANdevRx,
//                uint16_t                CANdevRxIdx,
//                CO_CANmodule_t         *CANdevTx,
//                uint16_t                CANdevTxIdx)
//        err = CO_SDO_init(
//                &CO->SDO[i],
//                COB_IDClientToServer,
//                COB_IDServerToClient,
//                OD_H1200_SDO_SERVER_PARAM+i,
//                i==0 ? 0 : &CO->SDO[0],
//                CO_OD,
//                CO->consts->OD_NoOfElements,
//                CO->SDO_ODExtensions,
//                nodeId,
//                CO->CANmodule[0],
//                CO_RXCAN_SDO_SRV((*(CO->consts)))+i,
//                CO->CANmodule[0],
//                CO_TXCAN_SDO_SRV((*(CO->consts)))+i);
        if(err){return err;}
    }


    /* Emergency */
	err = CO_EM_init(
			CO->em,
			CO->emPr,
			CO->SDO,
			CO->errorStatusBits,
			CO->errorStatusBitsSize,
			CO->consts->errorRegister,
			CO->consts->preDefinedErrorField,
			CO->consts->preDefinedErrorFieldSize,
			CO->CANmodule[0],
			CO_RXCAN_EMERG((*(CO->consts))),
			CO->CANmodule[0],
			CO_TXCAN_EMERG((*(CO->consts))),
			(uint16_t)CO_CAN_ID_EMERGENCY + nodeId);

    if (err) return err;
//    CO_ReturnError_t CO_NMT_init(
//            CO_NMT_t               *NMT,
//            CO_EMpr_t              *emPr,
//            uint8_t                 nodeId,
//            uint16_t                firstHBTime_ms,
//            CO_CANmodule_t         *NMT_CANdevRx,
//            uint16_t                NMT_rxIdx,
//            uint16_t                CANidRxNMT,
//            CO_CANmodule_t         *NMT_CANdevTx,
//            uint16_t                NMT_txIdx,
//            uint16_t                CANidTxNMT,
//            CO_CANmodule_t         *HB_CANdevTx,
//            uint16_t                HB_txIdx,
//            uint16_t                CANidTxHB)

    /* NMT_Heartbeat */
    err = CO_NMT_init(CO->NMT,
                       CO->emPr,
                       nodeId,
                       500,
                       CO->CANmodule[0],
                       CO_RXCAN_NMT((*(CO->consts))),
                       CO_CAN_ID_NMT_SERVICE,
                       CO->CANmodule[0],
                       CO_TXCAN_NMT((*(CO->consts))),
                       CO_CAN_ID_NMT_SERVICE,
                       CO->CANmodule[0],
                       CO_TXCAN_HB((*(CO->consts))),
                       CO_CAN_ID_HEARTBEAT + nodeId);
     if(err){return err;}

    /* SYNC */
     entry=CO_OD_find(CO->SDO,0x1005);
     uint32_t syncCOBId=*(uint32_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
     entry=CO_OD_find(CO->SDO,0x1006);
     uint32_t syncPeriod=*(uint32_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
     uint8_t syncCntOverflow=0;
     entry=CO_OD_find(CO->SDO,0x1019);
     if (entry!=0xffff)
     	syncCntOverflow=*(uint8_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
     CO->NMT->operatingState = CO_NMT_PRE_OPERATIONAL;

     if (CO->consts->NO_SYNC == 1) {
     	uint32_t syncwin=10000;
         entry=CO_OD_find(CO->SDO,0x1007);
         if (entry!=0xffff)
         	syncwin=*(uint32_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
         CO->syncronizationWindowLength=syncwin;
 		err = CO_SYNC_init(
 				CO->SYNC,
 				CO->em,
 				CO->SDO,
 				&CO->NMT->operatingState,
 				syncCOBId,
 				syncPeriod,
 				syncCntOverflow,
 				CO->CANmodule[0],
 				CO_RXCAN_SYNC((*(CO->consts))),
 				CO->CANmodule[0],
 				CO_TXCAN_SYNC((*(CO->consts))));
     }
     if(err){return err;}

     uint32_t timeCOBid=0x100;
     entry=CO_OD_find(CO->SDO,0x1012);
     if (entry!=0xffff)
     	timeCOBid=*(uint8_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
     if (CO->consts->NO_TIME == 1)
 		err = CO_TIME_init(
 				CO->TIME,
 				CO->em,
 				CO->SDO,
 				&CO->NMT->operatingState,
 				timeCOBid,
 				0,
 				CO->CANmodule[0],
 				CO_RXCAN_TIME((*(CO->consts))),
 				CO->CANmodule[0],
 				CO_TXCAN_TIME((*(CO->consts))));

     if(err){return err;}

#if CO_NO_GFC == 1
    /* GFC */
    CO_GFC_init(CO->GFC,
                &OD_globalFailSafeCommandParameter,
                CO->CANmodule[0],
                CO_RXCAN_GFC,
                CO_CAN_ID_GFC,
                CO->CANmodule[0],
                CO_TXCAN_GFC,
                CO_CAN_ID_GFC);
#endif

#if CO_NO_SRDO != 0
    /* SRDO */
    err = CO_SRDOGuard_init(CO->SRDOGuard,
                      CO->SDO[0],
                      &CO->NMT->operatingState,
                      &OD_configurationValid,
                      OD_H13FE_SRDO_VALID,
                      OD_H13FF_SRDO_CHECKSUM);
    if (err) return err;

    for (i = 0; i < CO_NO_SRDO; i++) {
        CO_CANmodule_t *CANdev = CO->CANmodule[0];
        uint16_t CANdevRxIdx = CO_RXCAN_SRDO + 2*i;
        uint16_t CANdevTxIdx = CO_TXCAN_SRDO + 2*i;

        err = CO_SRDO_init(CO->SRDO[i],
                           CO->SRDOGuard,
                           CO->em,
                           CO->SDO[0],
                           nodeId,
                           ((i == 0) ? CO_CAN_ID_SRDO_1 : 0),
                           (CO_SRDOCommPar_t*)&OD_SRDOCommunicationParameter[i],
                           (CO_SRDOMapPar_t *)&OD_SRDOMappingParameter[i],
                           &OD_safetyConfigurationChecksum[i],
                           OD_H1301_SRDO_1_PARAM + i,
                           OD_H1381_SRDO_1_MAPPING + i,
                           CANdev,
                           CANdevRxIdx,
                           CANdevRxIdx + 1,
                           CANdev,
                           CANdevTxIdx,
                           CANdevTxIdx + 1);
        if (err) return err;
    }
#endif

    /* RPDO */
    entry=CO_OD_find(CO->SDO,0x1400);
    CO_RPDOCommPar_t* rxCommParam=(CO_RPDOCommPar_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
    entry=CO_OD_find(CO->SDO,0x1600);
    CO_RPDOMapPar_t* rxCommMap=(CO_RPDOMapPar_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
    for(i=0; i<CO->consts->NO_RPDO; i++){
        CO_CANmodule_t *CANdevRx = CO->CANmodule[0];
        uint16_t CANdevRxIdx = CO_RXCAN_RPDO((*(CO->consts))) + i;

        err = CO_RPDO_init(
                &CO->RPDO[i],
                CO->em,
                CO->SDO,
#if (CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE
               CO->SYNC,
#endif
			   &CO->NMT->operatingState,
                nodeId,
                ((i<4) ? (CO_CAN_ID_RPDO_1+i*0x100) : 0),
                0,
                &rxCommParam[i],
                &rxCommMap[i],
                OD_H1400_RXPDO_1_PARAM+i,
                OD_H1600_RXPDO_1_MAPPING+i,
                CANdevRx,
                CANdevRxIdx);

        if(err){return err;}
    }

    /* TPDO */
    entry=CO_OD_find(CO->SDO,0x1800);
     CO_TPDOCommPar_t* txCommParam=(CO_TPDOCommPar_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
     entry=CO_OD_find(CO->SDO,0x1A00);
     CO_TPDOMapPar_t* txCommMap=(CO_TPDOMapPar_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
     for(i=0; i<CO->consts->NO_TPDO; i++){
         err = CO_TPDO_init(
                 &CO->TPDO[i],
                 CO->em,
                 CO->SDO,
#if (CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE
                 CO->SYNC,
#endif
				 &CO->NMT->operatingState,
                 nodeId,
                 ((i<4) ? (CO_CAN_ID_TPDO_1+i*0x100) : 0),
                 0,
                 &txCommParam[i],
                 &txCommMap[i],
                 OD_H1800_TXPDO_1_PARAM+i,
                 OD_H1A00_TXPDO_1_MAPPING+i,
                 CO->CANmodule[0],
                 CO_TXCAN_TPDO((*(CO->consts)))+i);

         if(err){return err;}
     }

    /* Heartbeat consumer */
     if (CO->consts->consumerHeartbeatTime_arrayLength>0) {
 		entry=CO_OD_find(CO->SDO,0x1016);
 		uint32_t* consumerArrPtr=(uint32_t*)CO_OD_getDataPointer(CO->SDO,entry,1);
 		err = CO_HBconsumer_init(
 				CO->HBcons,
 				CO->em,
 				CO->SDO,
 				&consumerArrPtr[0],
 				CO->HBcons_monitoredNodes,
				CO->consts->consumerHeartbeatTime_arrayLength,
 				CO->CANmodule[0],
 				CO_RXCAN_CONS_HB((*(CO->consts))));

 		if(err){return err;}
 	}
#if MULTIPLE_OD == 1 || CO_NO_SDO_CLIENT
     if (CO->consts->NO_SDO_CLIENT != 0)
  		for(i=0; i<CO->consts->NO_SDO_CLIENT; i++){
  		    entry=CO_OD_find(CO->SDO,0x1280);
  		    CO_SDOclientPar_t* SDOclientParam=(CO_SDOclientPar_t*)CO_OD_getDataPointer(CO->SDO,entry,0);
  			err = CO_SDOclient_init(
  					&CO->SDOclient[i],
  					CO->SDO,
  					&SDOclientParam[i],
  					CO->CANmodule[0],
  					CO_RXCAN_SDO_CLI((*(CO->consts)))+i,
  					CO->CANmodule[0],
  					CO_TXCAN_SDO_CLI((*(CO->consts)))+i);

  			if(err){return err;}

  		}
#endif
#if CO_NO_LSS_MASTER == 1 || defined CO_DOXYGEN
    if (CO->consts->NO_LSS_CLIENT == 1)
        err = CO_LSSmaster_init(
             CO->LSSmaster,
             CO_LSSmaster_DEFAULT_TIMEOUT,
             CO->CANmodule[0],
             CO_RXCAN_LSS((*(CO->consts))),
             CO_CAN_ID_LSS_CLI,
             CO->CANmodule[0],
             CO_TXCAN_LSS((*(CO->consts))),
             CO_CAN_ID_LSS_SRV);
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

#ifdef TRACE
    for(i=0; i<CO->NO_TRACE; i++) {
        CO_trace_init(
            CO->trace[i],
            CO->SDO,
            CO->traceConfig[i].axisNo,
            CO->traceTimeBuffers[i],
            CO->traceValueBuffers[i],
            CO->traceBufferSize[i],
            &CO->traceConfig[i].map,
            &CO->traceConfig[i].format,
            &CO->traceConfig[i].trigger,
            &CO->traceConfig[i].threshold,
            &CO->traceData[i].value,
            &CO->traceData[i].min,
            &CO->traceData[i].max,
            &CO->traceData[i].triggerTime,
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

    CO_CANmodule_process(co->CANmodule[0]);

#if CO_NO_LSS_SLAVE == 1
    bool_t resetLSS = CO_LSSslave_process(co->LSSslave);
#endif

//#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
//    bool_t unc = co->nodeIdUnconfigured;
//    uint16_t CANerrorStatus = CO->CANmodule[0]->CANerrorStatus; // Never access CanModule directly - call a function!
//    CO_LEDs_process(co->LEDs,
//                    timeDifference_us,
//                    unc ? CO_NMT_INITIALIZING : co->NMT->operatingState,
//    #if CO_NO_LSS_SLAVE == 1
//                    CO_LSSslave_getState(co->LSSslave)
//                        == CO_LSS_STATE_CONFIGURATION,
//    #else
//                    false,
//    #endif
//                    (CANerrorStatus & CO_CAN_ERRTX_BUS_OFF) != 0,
//                    (CANerrorStatus & CO_CAN_ERR_WARN_PASSIVE) != 0,
//                    0, /* RPDO event timer timeout */
//                    unc ? false : CO_isError(co->em, CO_EM_SYNC_TIME_OUT),
//                    unc ? false : (CO_isError(co->em, CO_EM_HEARTBEAT_CONSUMER)
//                        || CO_isError(co->em, CO_EM_HB_CONSUMER_REMOTE_RESET)),
//                    OD_errorRegister != 0,
//                    CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS,
//                    timerNext_us);
//#endif /* (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE */

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
    for(i=0; i<co->consts->NO_SDO_SERVER; i++){
          CO_SDO_process(&co->SDO[i],
                       NMTisPreOrOperational,
                       timeDifference_us,
                       timerNext_us);
    }

    /* Emergency */
    CO_EM_process(co->emPr,
                  NMTisPreOrOperational,
                  timeDifference_us,
                  co->inhibitTimeEMCY,
                  timerNext_us);

    /* NMT_Heartbeat */
    reset = CO_NMT_process(co->NMT,
                           timeDifference_us,
                           co->producerHeartbeatTime,
                           co->NMTStartup,
                           *(co->errorRegister),
                           co->errorBehavior,
                           timerNext_us);

    /* TIME */
    if (co->consts->NO_TIME == 1) {
		CO_TIME_process(co->TIME,
						timeDifference_us);
    }

    /* Heartbeat consumer */
    if (co->consts->consumerHeartbeatTime_arrayLength>0)
    CO_HBconsumer_process(
            co->HBcons,
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
bool_t CO_process_SYNC(CO_t *co,
                       uint32_t timeDifference_us,
                       uint32_t *timerNext_us)
{
    bool_t syncWas = false;

    if (co->consts->NO_LSS_SERVER == 1) {
		if (co->nodeIdUnconfigured) {
			return syncWas;
		}
    }
    const CO_SYNC_status_t sync_process = CO_SYNC_process(co->SYNC,
                                   timeDifference_us,
								   co->syncronizationWindowLength,
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


/******************************************************************************/
void CO_process_RPDO(CO_t *co,
                     bool_t syncWas)
{
    int16_t i;

    if (co->consts->NO_LSS_SERVER == 1) {
		if (co->nodeIdUnconfigured) {
			return ;
		}
    }

    for (i = 0; i < co->consts->NO_RPDO; i++) {
        CO_RPDO_process(&(co->RPDO[i]), syncWas);
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
    for (i = 0; i < co->consts->NO_TPDO; i++) {
        if (!co->TPDO[i].sendRequest)
            co->TPDO[i].sendRequest = CO_TPDOisCOS(&co->TPDO[i]);
        CO_TPDO_process(&(co->TPDO[i]), syncWas, timeDifference_us, timerNext_us);
    }
}

/******************************************************************************/
#if CO_NO_SRDO != 0
void CO_process_SRDO(CO_t *co,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
    int16_t i;
    uint8_t firstOperational;

#if CO_NO_LSS_SLAVE == 1
    if (co->nodeIdUnconfigured) {
        return;
    }
#endif

    firstOperational = CO_SRDOGuard_process(co->SRDOGuard);

    /* Verify PDO Change Of State and process PDOs */
    for (i = 0; i < CO_NO_SRDO; i++) {
        CO_SRDO_process(co->SRDO[i], firstOperational, timeDifference_us, timerNext_us);
    }
}
#endif

#ifdef INIT_HELPERS
/******************************************************************************/
CO_ReturnError_t CO_initEx(CO_t *CO, const CO_OD_entry_t *CO_OD,
        void                   *CANdriverState,
        uint8_t                 nodeId,
        uint16_t                bitRate)
{
    CO_ReturnError_t err;

    err = CO_newEx(CO,NULL);
    if (err) {
        return err;
    }

    err = CO_CANinitEx(CO, CANdriverState, bitRate);
    if (err) {
        CO_deleteEx(CO, CANdriverState);
        return err;
    }

    err = CO_CANopenInitEx(CO, &CO_OD[0], nodeId);
    if (err) {
        CO_deleteEx(CO, CANdriverState);
        return err;
    }

    return CO_ERROR_NO;
}

#if MULTIPLE_OD == 0
CO_ReturnError_t CO_init(void *CANdriverState, uint8_t nodeId,   uint16_t bitRate)
{
	COO.consts=&CO_Consts;
#ifdef OD_errorStatusBits
	COO.errorStatusBits=OD_errorStatusBits;
	COO.errorStatusBitsSize=ODL_errorStatusBits_stringLength;
#else
	static errorStatusBits[10];
	COO.errorStatusBits=errorStatusBits;
	COO.errorStatusBitsSize=sizeof(errorStatusBits);
#endif
	return CO_initEx(&COO, &CO_OD[0],CANdriverState,nodeId,bitRate);
}
#endif
#endif
