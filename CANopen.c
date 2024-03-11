/**
 * \file CANopen.c
 *
 * \brief CANopen main module implementation.
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "CANopen.h"


/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
/**
 * Get values from CO_config_t or from single default OD.h.
 * At the moment only one  object dictionary is supported,
 * therefore CO_MULTIPLE_OD deosn't have to be defined.
 */
#ifdef CO_MULTIPLE_OD
#define CO_GET_CO(obj) co->obj
#define CO_GET_CNT(obj) co->config->CNT_##obj
#define OD_GET(entry, index) co->config->ENTRY_##entry
#else
#include "OD.h"
#define CO_GET_CO(obj) CO_##obj
#define CO_GET_CNT(obj) OD_CNT_##obj
#define OD_GET(entry, index) OD_ENTRY_##entry

/** NMT macro check */
#if OD_CNT_NMT != 1
#error OD_CNT_NMT from OD.h not correct!
#endif
#define CO_RX_CNT_NMT_SLV OD_CNT_NMT
#if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
#define CO_TX_CNT_NMT_MST 1
#else
#define CO_TX_CNT_NMT_MST 0
#endif

/** HB producer macro check */
#if OD_CNT_HB_PROD != 1
#error OD_CNT_HB_PROD from OD.h not correct!
#endif
#define CO_TX_CNT_HB_PROD OD_CNT_HB_PROD
#if !defined OD_CNT_HB_CONS
#define OD_CNT_HB_CONS 0
#elif OD_CNT_HB_CONS < 0 || OD_CNT_HB_CONS > 1
#error OD_CNT_HB_CONS from OD.h not correct!
#endif
#if ((CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE) && OD_CNT_HB_CONS == 1
#if OD_CNT_ARR_1016 < 1 || OD_CNT_ARR_1016 > 127
#error OD_CNT_ARR_1016 is not defined in Object Dictionary or value is wrong!
#endif
#define CO_RX_CNT_HB_CONS OD_CNT_ARR_1016
#else
#define CO_RX_CNT_HB_CONS 0
#endif

/** EMCY macro check */
#if OD_CNT_EM != 1
#error OD_CNT_EM from OD.h not correct!
#endif
#ifndef OD_ENTRY_H1003
#define OD_ENTRY_H1003 NULL
#endif
#ifndef OD_CNT_ARR_1003
#define OD_CNT_ARR_1003 8
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
#if OD_CNT_EM_PROD == 1
#define CO_TX_CNT_EM_PROD OD_CNT_EM_PROD
#else
#error wrong OD_CNT_EM_PROD
#endif
#ifndef OD_ENTRY_H1015
#define OD_ENTRY_H1015 NULL
#endif
#else
#define CO_TX_CNT_EM_PROD 0
#endif
#if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
#define CO_RX_CNT_EM_CONS 1
#else
#define CO_RX_CNT_EM_CONS 0
#endif

/** SDO server macro check */
#if !defined OD_CNT_SDO_SRV
#define OD_CNT_SDO_SRV 1
#define OD_ENTRY_H1200 NULL
#elif OD_CNT_SDO_SRV < 1 || OD_CNT_SDO_SRV > 128
#error OD_CNT_SDO_SRV from OD.h not correct!
#endif
#define CO_RX_CNT_SDO_SRV OD_CNT_SDO_SRV
#define CO_TX_CNT_SDO_SRV OD_CNT_SDO_SRV

/** SDO client macro check */
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
#if !defined OD_CNT_SDO_CLI
#define OD_CNT_SDO_CLI 0
#define OD_ENTRY_H1280 NULL
#elif OD_CNT_SDO_CLI < 0 || OD_CNT_SDO_CLI > 128
#error OD_CNT_SDO_CLI from OD.h not correct!
#endif
#define CO_RX_CNT_SDO_CLI OD_CNT_SDO_CLI
#define CO_TX_CNT_SDO_CLI OD_CNT_SDO_CLI
#else
#define CO_RX_CNT_SDO_CLI 0
#define CO_TX_CNT_SDO_CLI 0
#endif

/** TIME macro check */
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
#if !defined OD_CNT_TIME
#define OD_CNT_TIME 0
#define OD_ENTRY_H1012 NULL
#elif OD_CNT_TIME < 0 || OD_CNT_TIME > 1
#error OD_CNT_TIME from OD.h not correct!
#endif
#define CO_RX_CNT_TIME OD_CNT_TIME
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
#define CO_TX_CNT_TIME OD_CNT_TIME
#else
#define CO_TX_CNT_TIME 0
#endif
#else
#define CO_RX_CNT_TIME 0
#define CO_TX_CNT_TIME 0
#endif

/** SYNC macro check */
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
#if !defined OD_CNT_SYNC
#define OD_CNT_SYNC 0
#define OD_ENTRY_H1005 NULL
#define OD_ENTRY_H1006 NULL
#elif OD_CNT_SYNC < 0 || OD_CNT_SYNC > 1
#error OD_CNT_SYNC from OD.h not correct!
#endif
#define CO_RX_CNT_SYNC OD_CNT_SYNC
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
#define CO_TX_CNT_SYNC OD_CNT_SYNC
#else
#define CO_TX_CNT_SYNC 0
#endif
#ifndef OD_ENTRY_H1007
#define OD_ENTRY_H1007 NULL
#endif
#ifndef OD_ENTRY_H1019
#define OD_ENTRY_H1019 NULL
#endif
#else
#define CO_RX_CNT_SYNC 0
#define CO_TX_CNT_SYNC 0
#endif

/** RPDO macro check */
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
#if !defined OD_CNT_RPDO
#define OD_CNT_RPDO 0
#define OD_ENTRY_H1400 NULL
#define OD_ENTRY_H1600 NULL
#elif OD_CNT_RPDO < 0 || OD_CNT_RPDO > 0x200
#error OD_CNT_RPDO from OD.h not correct!
#endif
#define CO_RX_CNT_RPDO OD_CNT_RPDO
#else
#define CO_RX_CNT_RPDO 0
#endif

/** TPDO macro check */
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
#if !defined OD_CNT_TPDO
#define OD_CNT_TPDO 0
#define OD_ENTRY_H1800 NULL
#define OD_ENTRY_H1A00 NULL
#elif OD_CNT_TPDO < 0 || OD_CNT_TPDO > 0x200
#error OD_CNT_TPDO from OD.h not correct!
#endif
#define CO_TX_CNT_TPDO OD_CNT_TPDO
#else
#define CO_TX_CNT_TPDO 0
#endif

/** LED macro check */
#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
#define OD_CNT_LEDS 1
#endif

/** GFC macro check */
#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
#if !defined OD_CNT_GFC
#define OD_CNT_GFC 0
#define OD_ENTRY_H1300 NULL
#elif OD_CNT_GFC < 0 || OD_CNT_GFC > 1
#error OD_CNT_GFC from OD.h not correct!
#endif
#define CO_RX_CNT_GFC OD_CNT_GFC
#define CO_TX_CNT_GFC OD_CNT_GFC
#else
#define CO_RX_CNT_GFC 0
#define CO_TX_CNT_GFC 0
#endif

/** SRDO macro check */
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
#if !defined OD_CNT_SRDO
#define OD_CNT_SRDO 0
#define OD_ENTRY_H1301 NULL
#define OD_ENTRY_H1381 NULL
#define OD_ENTRY_H13FE NULL
#define OD_ENTRY_H13FF NULL
#elif OD_CNT_SRDO < 0 || OD_CNT_SRDO > 64
#error OD_CNT_SRDO from OD.h not correct!
#endif
#define CO_RX_CNT_SRDO OD_CNT_SRDO
#define CO_TX_CNT_SRDO OD_CNT_SRDO
#else
#define CO_RX_CNT_SRDO 0
#define CO_TX_CNT_SRDO 0
#endif

/** LSS slave macro check */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
#define OD_CNT_LSS_SLV 1
#else
#define OD_CNT_LSS_SLV 0
#endif
#define CO_RX_CNT_LSS_SLV OD_CNT_LSS_SLV
#define CO_TX_CNT_LSS_SLV OD_CNT_LSS_SLV

/** LSS master macro check */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
#define OD_CNT_LSS_MST 1
#else
#define OD_CNT_LSS_MST 0
#endif
#define CO_RX_CNT_LSS_MST OD_CNT_LSS_MST
#define CO_TX_CNT_LSS_MST OD_CNT_LSS_MST

/** GTW macro check */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
#define OD_CNT_GTWA 1
#endif

/** TRACE macro check */
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
#if !defined OD_CNT_TRACE
#define OD_CNT_TRACE 0
#elif OD_CNT_TRACE < 0
#error OD_CNT_TRACE from OD.h not correct!
#endif
#endif

/**
 * CO_CANrx_t objects indexes (objects with highest priority of the CAN  are listed first).
 */
#define CO_RX_IDX_NMT_SLV   0
#define CO_RX_IDX_SYNC      (CO_RX_IDX_NMT_SLV  + CO_RX_CNT_NMT_SLV)
#define CO_RX_IDX_EM_CONS   (CO_RX_IDX_SYNC     + CO_RX_CNT_SYNC)
#define CO_RX_IDX_TIME      (CO_RX_IDX_EM_CONS  + CO_RX_CNT_EM_CONS)
#define CO_RX_IDX_GFC       (CO_RX_IDX_TIME     + CO_RX_CNT_TIME)
#define CO_RX_IDX_SRDO      (CO_RX_IDX_GFC      + CO_RX_CNT_GFC)
#define CO_RX_IDX_RPDO      (CO_RX_IDX_SRDO     + CO_RX_CNT_SRDO * 2)
#define CO_RX_IDX_SDO_SRV   (CO_RX_IDX_RPDO     + CO_RX_CNT_RPDO)
#define CO_RX_IDX_SDO_CLI   (CO_RX_IDX_SDO_SRV  + CO_RX_CNT_SDO_SRV)
#define CO_RX_IDX_HB_CONS   (CO_RX_IDX_SDO_CLI  + CO_RX_CNT_SDO_CLI)
#define CO_RX_IDX_LSS_SLV   (CO_RX_IDX_HB_CONS  + CO_RX_CNT_HB_CONS)
#define CO_RX_IDX_LSS_MST   (CO_RX_IDX_LSS_SLV  + CO_RX_CNT_LSS_SLV)
#define CO_CNT_ALL_RX_MSGS  (CO_RX_IDX_LSS_MST  + CO_RX_CNT_LSS_MST)

/**
 * CO_CANtx_t objects indexes (objects with highest priority of the CAN  are listed first).
 */
#define CO_TX_IDX_NMT_MST   0
#define CO_TX_IDX_SYNC      (CO_TX_IDX_NMT_MST  + CO_TX_CNT_NMT_MST)
#define CO_TX_IDX_EM_PROD   (CO_TX_IDX_SYNC     + CO_TX_CNT_SYNC)
#define CO_TX_IDX_TIME      (CO_TX_IDX_EM_PROD  + CO_TX_CNT_EM_PROD)
#define CO_TX_IDX_GFC       (CO_TX_IDX_TIME     + CO_TX_CNT_TIME)
#define CO_TX_IDX_SRDO      (CO_TX_IDX_GFC      + CO_TX_CNT_GFC)
#define CO_TX_IDX_TPDO      (CO_TX_IDX_SRDO     + CO_TX_CNT_SRDO * 2)
#define CO_TX_IDX_SDO_SRV   (CO_TX_IDX_TPDO     + CO_TX_CNT_TPDO)
#define CO_TX_IDX_SDO_CLI   (CO_TX_IDX_SDO_SRV  + CO_TX_CNT_SDO_SRV)
#define CO_TX_IDX_HB_PROD   (CO_TX_IDX_SDO_CLI  + CO_TX_CNT_SDO_CLI)
#define CO_TX_IDX_LSS_SLV   (CO_TX_IDX_HB_PROD  + CO_TX_CNT_HB_PROD)
#define CO_TX_IDX_LSS_MST   (CO_TX_IDX_LSS_SLV  + CO_TX_CNT_LSS_SLV)
#define CO_CNT_ALL_TX_MSGS  (CO_TX_IDX_LSS_MST  + CO_TX_CNT_LSS_MST)
#endif /* #ifdef #else CO_MULTIPLE_OD */

/**
 *  Objects from heap. Not used at the moment.
 */
#ifndef CO_USE_GLOBALS
#include <stdlib.h>

/** default allocation strategy */
#if (!defined(CO_alloc) || !defined(CO_free))
#if defined(CO_alloc)
#warning CO_alloc is defined but CO_free is not. using default values instead
#undef CO_alloc
#endif
#if defined(CO_free)
#warning CO_free is defined but CO_alloc is not. using default values instead
#undef CO_free
#endif

/*
 * Allocate memory for number of elements, each of specific size
 * Allocated memory must be reset to all zeros
 */
#define CO_alloc(num, size)     calloc((num), (size))
#define CO_free(ptr)            free((ptr))
#endif

/** define macros for allocation */
#define CO_alloc_break_on_fail(var, num, size) 		\
	if (((var) = CO_alloc((num), (size))) != NULL) 	\
	{ 												\
		mem += (size) * (num); 						\
	} 												\
	else 											\
	{ 												\
		break;										\
	}

#ifdef CO_MULTIPLE_OD
#define ON_MULTI_OD(sentence) sentence
#else
#define ON_MULTI_OD(sentence)
#endif


/***********************************************************************************************************************
 * Public Functions
 **********************************************************************************************************************/
/*
 * Create new CANopen object. If CO_USE_GLOBALS is defined, then function uses global static
 * variables for all the CANopenNode objects. Otherwise it allocates all objects from heap.
 */
CO_t *CO_new(CO_config_t *config, uint32_t *heapMemoryUsed)
{
    CO_t *co = NULL;

    /* return values */
    CO_t *coFinal = NULL;
    uint32_t mem = 0;

    /*
     *  For each object:
     * - allocate memory, verify allocation and calculate size of heap used
     * - if CO_MULTIPLE_OD is defined:
     *   - use config structure
     *   - calculate number of CANrx and CYNtx messages: RX_CNT_xx and TX_CNT_xx
     *   - calculate indexes: RX_IDX_xx and TX_IDX_xx
     *   - calculate total count of CAN message buffers: CNT_ALL_RX_MSGS and
     *     CNT_ALL_TX_MSGS.
     */

    do
    {
#ifdef CO_MULTIPLE_OD
        /* verify arguments */
        if (config == NULL || config->CNT_NMT > 1 || config->CNT_HB_CONS > 1 || \
        	config->CNT_EM > 1 || config->CNT_SDO_SRV > 128 || \
			config->CNT_SDO_CLI > 128 || config->CNT_SYNC > 1 || \
			config->CNT_RPDO > 512 || config->CNT_TPDO > 512 || \
			config->CNT_TIME > 1 || config->CNT_LEDS > 1 || \
			config->CNT_GFC > 1 || config->CNT_SRDO > 64 || \
			config->CNT_LSS_SLV > 1 || config->CNT_LSS_MST > 1 || \
			config->CNT_GTWA > 1)
        {
            break;
        }
#else
        (void)config;
#endif

        /* CANopen object */
        CO_alloc_break_on_fail(co, 1, sizeof(*co));

#ifdef CO_MULTIPLE_OD
        co->config = config;
#endif

        /* NMT_Heartbeat */
        ON_MULTI_OD(uint8_t RX_CNT_NMT_SLV = 0);
        ON_MULTI_OD(uint8_t TX_CNT_NMT_MST = 0);
        ON_MULTI_OD(uint8_t TX_CNT_HB_PROD = 0);

        /* NMT */
        if (CO_GET_CNT(NMT) == 1)
        {
            CO_alloc_break_on_fail(co->NMT, CO_GET_CNT(NMT), sizeof(*co->NMT));
            ON_MULTI_OD(RX_CNT_NMT_SLV = 1);
 #if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
            ON_MULTI_OD(TX_CNT_NMT_MST = 1);
 #endif
            ON_MULTI_OD(TX_CNT_HB_PROD = 1);
        }

        /* HB consumer */
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
        ON_MULTI_OD(uint8_t RX_CNT_HB_CONS = 0);
        if (CO_GET_CNT(HB_CONS) == 1)
        {
            uint8_t countOfMonitoredNodes = CO_GET_CNT(ARR_1016);
            CO_alloc_break_on_fail(co->HBcons, CO_GET_CNT(HB_CONS), sizeof(*co->HBcons));
            CO_alloc_break_on_fail(co->HBconsMonitoredNodes, countOfMonitoredNodes, sizeof(*co->HBconsMonitoredNodes));
            ON_MULTI_OD(RX_CNT_HB_CONS = countOfMonitoredNodes);
        }
#endif

        /* Emergency */
        ON_MULTI_OD(uint8_t RX_CNT_EM_CONS = 0);
        ON_MULTI_OD(uint8_t TX_CNT_EM_PROD = 0);
        if (CO_GET_CNT(EM) == 1)
        {
            CO_alloc_break_on_fail(co->em, CO_GET_CNT(EM), sizeof(*co->em));
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
            ON_MULTI_OD(RX_CNT_EM_CONS = 1);
 #endif
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
            ON_MULTI_OD(TX_CNT_EM_PROD = 1);
 #endif
 #if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
            uint8_t fifoSize = CO_GET_CNT(ARR_1003) + 1;
            if (fifoSize >= 2)
            {
                CO_alloc_break_on_fail(co->em_fifo, fifoSize, sizeof(*co->em_fifo));
            }
 #endif
        }

        /* SDO server */
        ON_MULTI_OD(uint8_t RX_CNT_SDO_SRV = 0);
        ON_MULTI_OD(uint8_t TX_CNT_SDO_SRV = 0);
        if (CO_GET_CNT(SDO_SRV) > 0)
        {
            CO_alloc_break_on_fail(co->SDOserver, CO_GET_CNT(SDO_SRV), sizeof(*co->SDOserver));
            ON_MULTI_OD(RX_CNT_SDO_SRV = config->CNT_SDO_SRV);
            ON_MULTI_OD(TX_CNT_SDO_SRV = config->CNT_SDO_SRV);
        }

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
        /* SDO client */
        ON_MULTI_OD(uint8_t RX_CNT_SDO_CLI = 0);
        ON_MULTI_OD(uint8_t TX_CNT_SDO_CLI = 0);
        if (CO_GET_CNT(SDO_CLI) > 0)
        {
            CO_alloc_break_on_fail(co->SDOclient, CO_GET_CNT(SDO_CLI), sizeof(*co->SDOclient));
            ON_MULTI_OD(RX_CNT_SDO_CLI = config->CNT_SDO_CLI);
            ON_MULTI_OD(TX_CNT_SDO_CLI = config->CNT_SDO_CLI);
        }
#endif

#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
        /* Time */
        ON_MULTI_OD(uint8_t RX_CNT_TIME = 0);
        ON_MULTI_OD(uint8_t TX_CNT_TIME = 0);
        if (CO_GET_CNT(TIME) == 1)
        {
            CO_alloc_break_on_fail(co->TIME, CO_GET_CNT(TIME), sizeof(*co->TIME));
            ON_MULTI_OD(RX_CNT_TIME = 1);
 #if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
            ON_MULTI_OD(TX_CNT_TIME = 1);
 #endif
        }
#endif

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
        /* SYNC */
        ON_MULTI_OD(uint8_t RX_CNT_SYNC = 0);
        ON_MULTI_OD(uint8_t TX_CNT_SYNC = 0);
        if (CO_GET_CNT(SYNC) == 1)
        {
            CO_alloc_break_on_fail(co->SYNC, CO_GET_CNT(SYNC), sizeof(*co->SYNC));
            ON_MULTI_OD(RX_CNT_SYNC = 1);
 #if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
            ON_MULTI_OD(TX_CNT_SYNC = 1);
 #endif
        }
#endif

#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
        /* RPDO */
        ON_MULTI_OD(uint16_t RX_CNT_RPDO = 0);
        if (CO_GET_CNT(RPDO) > 0)
        {
            CO_alloc_break_on_fail(co->RPDO, CO_GET_CNT(RPDO), sizeof(*co->RPDO));
            ON_MULTI_OD(RX_CNT_RPDO = config->CNT_RPDO);
        }
#endif

#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
        /* TPDO */
        ON_MULTI_OD(uint16_t TX_CNT_TPDO = 0);
        if (CO_GET_CNT(TPDO) > 0)
        {
            CO_alloc_break_on_fail(co->TPDO, CO_GET_CNT(TPDO), sizeof(*co->TPDO));
            ON_MULTI_OD(TX_CNT_TPDO = config->CNT_TPDO);
        }
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
        /* LEDS */
        if (CO_GET_CNT(LEDS) == 1)
        {
            CO_alloc_break_on_fail(co->LEDs, CO_GET_CNT(LEDS), sizeof(*co->LEDs));
        }
#endif

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
        /* GFC */
        ON_MULTI_OD(uint8_t RX_CNT_GFC = 0);
        ON_MULTI_OD(uint8_t TX_CNT_GFC = 0);
        if (CO_GET_CNT(GFC) == 1)
        {
            CO_alloc_break_on_fail(co->GFC, CO_GET_CNT(GFC), sizeof(*co->GFC));
            ON_MULTI_OD(RX_CNT_GFC = 1);
            ON_MULTI_OD(TX_CNT_GFC = 1);
        }
#endif

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
        /* SRDO */
        ON_MULTI_OD(uint8_t RX_CNT_SRDO = 0);
        ON_MULTI_OD(uint8_t TX_CNT_SRDO = 0);
        if (CO_GET_CNT(SRDO) > 0)
        {
            CO_alloc_break_on_fail(co->SRDOGuard, 1, sizeof(*co->SRDOGuard));
            CO_alloc_break_on_fail(co->SRDO, CO_GET_CNT(SRDO), sizeof(*co->SRDO));
            ON_MULTI_OD(RX_CNT_SRDO = config->CNT_SRDO * 2);
            ON_MULTI_OD(TX_CNT_SRDO = config->CNT_SRDO * 2);
        }
#endif

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
        /* LSS slave */
        ON_MULTI_OD(uint8_t RX_CNT_LSS_SLV = 0);
        ON_MULTI_OD(uint8_t TX_CNT_LSS_SLV = 0);
        if (CO_GET_CNT(LSS_SLV) == 1)
        {
            CO_alloc_break_on_fail(co->LSSslave, CO_GET_CNT(LSS_SLV), sizeof(*co->LSSslave));
            ON_MULTI_OD(RX_CNT_LSS_SLV = 1);
            ON_MULTI_OD(TX_CNT_LSS_SLV = 1);
        }
#endif

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
        /* LSS master */
        ON_MULTI_OD(uint8_t RX_CNT_LSS_MST = 0);
        ON_MULTI_OD(uint8_t TX_CNT_LSS_MST = 0);
        if (CO_GET_CNT(LSS_MST) == 1)
        {
            CO_alloc_break_on_fail(co->LSSmaster, CO_GET_CNT(LSS_MST), sizeof(*co->LSSmaster));
            ON_MULTI_OD(RX_CNT_LSS_MST = 1);
            ON_MULTI_OD(TX_CNT_LSS_MST = 1);
        }
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
        /* GTW */
        if (CO_GET_CNT(GTWA) == 1)
        {
            CO_alloc_break_on_fail(co->gtwa, CO_GET_CNT(GTWA), sizeof(*co->gtwa));
        }
#endif

#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
        /* TRACE */
        if (CO_GET_CNT(TRACE) > 0)
        {
            CO_alloc_break_on_fail(co->trace, CO_GET_CNT(TRACE), sizeof(*co->trace));
        }
#endif

#ifdef CO_MULTIPLE_OD
        /*
         * Indexes of CO_CANrx_t and CO_CANtx_t objects in CO_CANmodule_t and
         * total number of them. Indexes are sorted in a way, that objects with
         * highest priority of the CAN identifier are listed first.
         */
        int16_t idxRx = 0;
        co->RX_IDX_NMT_SLV = idxRx; idxRx += RX_CNT_NMT_SLV;
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
        co->RX_IDX_SYNC = idxRx; idxRx += RX_CNT_SYNC;
#endif
        co->RX_IDX_EM_CONS = idxRx; idxRx += RX_CNT_EM_CONS;
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
        co->RX_IDX_TIME = idxRx; idxRx += RX_CNT_TIME;
#endif
#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
        co->RX_IDX_GFC = idxRx; idxRx += RX_CNT_GFC;
#endif
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
        co->RX_IDX_SRDO = idxRx; idxRx += RX_CNT_SRDO * 2;
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
        co->RX_IDX_RPDO = idxRx; idxRx += RX_CNT_RPDO;
#endif
        co->RX_IDX_SDO_SRV = idxRx; idxRx += RX_CNT_SDO_SRV;
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
        co->RX_IDX_SDO_CLI = idxRx; idxRx += RX_CNT_SDO_CLI;
#endif
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
        co->RX_IDX_HB_CONS = idxRx; idxRx += RX_CNT_HB_CONS;
#endif
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
        co->RX_IDX_LSS_SLV = idxRx; idxRx += RX_CNT_LSS_SLV;
#endif
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
        co->RX_IDX_LSS_MST = idxRx; idxRx += RX_CNT_LSS_MST;
#endif
        co->CNT_ALL_RX_MSGS = idxRx;
        int16_t idxTx = 0;
        co->TX_IDX_NMT_MST = idxTx; idxTx += TX_CNT_NMT_MST;
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
        co->TX_IDX_SYNC = idxTx; idxTx += TX_CNT_SYNC;
#endif
        co->TX_IDX_EM_PROD = idxTx; idxTx += TX_CNT_EM_PROD;
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
        co->TX_IDX_TIME = idxTx; idxTx += TX_CNT_TIME;
#endif
#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
        co->TX_IDX_GFC = idxTx; idxTx += TX_CNT_GFC;
#endif
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
        co->TX_IDX_SRDO = idxTx; idxTx += TX_CNT_SRDO * 2;
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
        co->TX_IDX_TPDO = idxTx; idxTx += TX_CNT_TPDO;
#endif
        co->TX_IDX_SDO_SRV = idxTx; idxTx += TX_CNT_SDO_SRV;
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
        co->TX_IDX_SDO_CLI = idxTx; idxTx += TX_CNT_SDO_CLI;
#endif
        co->TX_IDX_HB_PROD = idxTx; idxTx += TX_CNT_HB_PROD;
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
        co->TX_IDX_LSS_SLV = idxTx; idxTx += TX_CNT_LSS_SLV;
#endif
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
        co->TX_IDX_LSS_MST = idxTx; idxTx += TX_CNT_LSS_MST;
#endif
        co->CNT_ALL_TX_MSGS = idxTx;
#endif /* #ifdef CO_MULTIPLE_OD */

        /* CANmodule */
        CO_alloc_break_on_fail(co->CANmodule, 1, sizeof(*co->CANmodule));

        /* CAN RX blocks */
        CO_alloc_break_on_fail(co->CANrx, CO_GET_CO(CNT_ALL_RX_MSGS), sizeof(*co->CANrx));

        /* CAN TX blocks */
        CO_alloc_break_on_fail(co->CANtx, CO_GET_CO(CNT_ALL_TX_MSGS), sizeof(*co->CANtx));

        /* finish successfully, set other parameters */
        co->nodeIdUnconfigured = 1;
        coFinal = co;
    } while (0);

    /* delete CANopen common object */
    if (coFinal == NULL)
    {
        CO_delete(co);
    }
    /* set heap memory pointer (not used now) */
    if (heapMemoryUsed != NULL)
    {
        *heapMemoryUsed = mem;
    }

    return coFinal;
}

/*
 * Delete CANopen object and free memory. Must be called at program exit.
 */
void CO_delete(CO_t *co)
{
	/* CANopen common object doesn't exist */
    if (co == NULL)
    {
        return;
    }

    /* disable module */
    CO_CANmodule_disable(co->CANmodule);

    /* CANmodule */
    CO_free(co->CANtx);
    CO_free(co->CANrx);
    CO_free(co->CANmodule);

#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
    CO_free(co->trace);
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    CO_free(co->gtwa);
#endif

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
    CO_free(co->LSSmaster);
#endif

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    CO_free(co->LSSslave);
#endif

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
    CO_free(co->SRDO);
    CO_free(co->SRDOGuard);
#endif

#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
    CO_free(co->GFC);
#endif

#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    CO_free(co->LEDs);
#endif

#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
    CO_free(co->TPDO);
#endif

#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
    CO_free(co->RPDO);
#endif

#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
    CO_free(co->SYNC);
#endif

#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
    CO_free(co->TIME);
#endif

#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
    free(co->SDOclient);
#endif

    /* SDOserver */
    CO_free(co->SDOserver);

    /* Emergency */
    CO_free(co->em);
#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
    CO_free(co->em_fifo);
#endif

#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
    CO_free(co->HBconsMonitoredNodes);
    CO_free(co->HBcons);
#endif

    /* NMT_Heartbeat */
    CO_free(co->NMT);

    /* CANopen object */
    CO_free(co);
}
#endif /* #ifndef CO_USE_GLOBALS */

/**
 * Global CANopen configuration. This is the default option where the
 * object dictionary is generated by an external designer and its
 * address is used directly.
 */
#ifdef CO_USE_GLOBALS
#ifdef CO_MULTIPLE_OD
#error CO_MULTIPLE_OD can not be used with CO_USE_GLOBALS
#endif
    static CO_t COO;
    static CO_CANmodule_t COO_CANmodule;
    static CO_CANrx_t COO_CANmodule_rxArray[CO_CNT_ALL_RX_MSGS];
    static CO_CANtx_t COO_CANmodule_txArray[CO_CNT_ALL_TX_MSGS];
    static CO_NMT_t COO_NMT;
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
    static CO_HBconsumer_t COO_HBcons;
    static CO_HBconsNode_t COO_HBconsMonitoredNodes[OD_CNT_ARR_1016];
#endif
    static CO_EM_t COO_EM;
#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
    static CO_EM_fifo_t COO_EM_FIFO[CO_GET_CNT(ARR_1003) + 1];
#endif
    static CO_SDOserver_t COO_SDOserver[OD_CNT_SDO_SRV];
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
    static CO_SDOclient_t COO_SDOclient[OD_CNT_SDO_CLI];
#endif
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
    static CO_TIME_t COO_TIME;
#endif
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
    static CO_SYNC_t COO_SYNC;
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
    static CO_RPDO_t COO_RPDO[OD_CNT_RPDO];
#endif
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
    static CO_TPDO_t COO_TPDO[OD_CNT_TPDO];
#endif
#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    static CO_LEDs_t COO_LEDs;
#endif
#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
    static CO_GFC_t COO_GFC;
#endif
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
    static CO_SRDOGuard_t COO_SRDOGuard;
    static CO_SRDO_t COO_SRDO[OD_CNT_SRDO];
#endif
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    static CO_LSSslave_t COO_LSSslave;
#endif
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
    static CO_LSSmaster_t COO_LSSmaster;
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    static CO_GTWA_t COO_gtwa;
#endif
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
  #ifndef CO_TRACE_BUFFER_SIZE_FIXED
    #define CO_TRACE_BUFFER_SIZE_FIXED 100
  #endif
    static CO_trace_t COO_trace[OD_CNT_TRACE];
    static uint32_t COO_traceTimeBuffers[OD_CNT_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
    static int32_t COO_traceValueBuffers[OD_CNT_TRACE][CO_TRACE_BUFFER_SIZE_FIXED];
#endif

/*
 * Create new CANopen object. Get private CANopen object fields addresses.
 */
CO_t *CO_new(CO_config_t *config, uint32_t *heapMemoryUsed)
{
	/* variables */
    (void)config;
    (void)heapMemoryUsed;

    /* get static CANopen object address */
    CO_t *co = &COO;

    /* CAN module */
    co->CANmodule = &COO_CANmodule;
    co->CANrx = &COO_CANmodule_rxArray[0];
    co->CANtx = &COO_CANmodule_txArray[0];

    /* NMT */
    co->NMT = &COO_NMT;
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
    co->HBcons = &COO_HBcons;
    co->HBconsMonitoredNodes = &COO_HBconsMonitoredNodes[0];
#endif

    /* EMCY */
    co->em = &COO_EM;
#if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
    co->em_fifo = &COO_EM_FIFO[0];
#endif

    /* SDO server */
    co->SDOserver = &COO_SDOserver[0];
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
    co->SDOclient = &COO_SDOclient[0];
#endif

    /* TIME */
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
    co->TIME = &COO_TIME;
#endif

    /* SYNC */
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
    co->SYNC = &COO_SYNC;
#endif

    /* RPDO */
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
    co->RPDO = &COO_RPDO[0];
#endif

    /* TPDO */
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
    co->TPDO = &COO_TPDO[0];
#endif

    /* LED */
#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    co->LEDs = &COO_LEDs;
#endif

    /* GFC */
#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
    co->GFC = &COO_GFC;
#endif

    /* SRDO */
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
    co->SRDOGuard = &COO_SRDOGuard;
    co->SRDO = &COO_SRDO[0];
#endif

    /* LSS slave */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    co->LSSslave = &COO_LSSslave;
#endif

    /* LSS master */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
    co->LSSmaster = &COO_LSSmaster;
#endif

    /* GTW */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    co->gtwa = &COO_gtwa;
#endif

    /* TRACE */
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
    co->trace = &COO_trace[0];
    co->traceTimeBuffers = &COO_traceTimeBuffers[0][0];
    co->traceValueBuffers = &COO_traceValueBuffers[0][0];
    co->traceBufferSize = CO_TRACE_BUFFER_SIZE_FIXED;
#endif

    /* return public object */
    return co;
}

/**
 *  Delete CANopen object and free memory. Do nothing since no memory
 *  has been allocated and the CANopen object is static.
 */
void CO_delete(CO_t *co)
{
	/* check CANopen object */
    if (co == NULL)
    {
        return;
    }

    /* disable module (does nothing at the moment) */
    CO_CANmodule_disable(co->CANmodule);
}
#endif /* #ifdef CO_USE_GLOBALS */

/*
 * Test if LSS slave is enabled.
 */
bool_t CO_isLSSslaveEnabled(CO_t *co)
{
    (void) co;
    bool_t en = 0;
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    if (CO_GET_CNT(LSS_SLV) == 1)
    {
    	en = 1;
    }
#endif
    return en;
}

/*
 * Initialize CAN driver.Function must be called in the communication reset section.
 */
CO_ReturnError_t CO_CANinit(CO_t *co, CO_hw_cfg *hw_cfg)
{
	/* error code */
	CO_ReturnError_t ret = CO_ERROR_ILLEGAL_ARGUMENT;

	/* check input pointers */
    if ((co != NULL) && (hw_cfg != NULL))
    {
    	/* set configuration mode (unused for the moment) */
		co->CANmodule->CANnormal = 0;
		CO_CANsetConfigurationMode(hw_cfg);

		/* initialize can module */
		ret = CO_CANmodule_init(co->CANmodule,
								hw_cfg,
								co->CANrx,
								CO_GET_CO(CNT_ALL_RX_MSGS),
								co->CANtx,
								CO_GET_CO(CNT_ALL_TX_MSGS));
    }

    /* return error code */
    return ret;
}

#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
/*
 * Initialize CANopen LSS slave. Function must be called before CO_CANopenInit.
 */
CO_ReturnError_t CO_LSSinit(CO_t *co,
                            CO_LSS_address_t *lssAddress,
                            uint8_t *pendingNodeID,
                            uint16_t *pendingBitRate)
{
	/* error code */
    CO_ReturnError_t ret = CO_ERROR_ILLEGAL_ARGUMENT;

    /* check input */
    if ( (co != NULL) && (CO_GET_CNT(LSS_SLV) == 1) )
    {
		/* LSSslave */
		ret = CO_LSSslave_init(co->LSSslave,
							   lssAddress,
							   pendingBitRate,
							   pendingNodeID,
							   co->CANmodule,
							   CO_GET_CO(RX_IDX_LSS_SLV),
							   CO_CAN_ID_LSS_MST,
							   co->CANmodule,
							   CO_GET_CO(TX_IDX_LSS_SLV),
							   CO_CAN_ID_LSS_SLV);
    }

    /* return error code */
    return ret;
}
#endif /* (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE */

/**
 * Initialize CANopenNode except PDO objects. Function must be called in
 * the communication reset section.
 */
CO_ReturnError_t CO_CANopenInit(CO_t *co,
                                CO_NMT_t *NMT,
                                CO_EM_t *em,
                                OD_t *od,
                                OD_entry_t *OD_statusBits,
                                CO_NMT_control_t NMTcontrol,
                                uint16_t firstHBTime_ms,
                                uint16_t SDOserverTimeoutTime_ms,
                                uint16_t SDOclientTimeoutTime_ms,
                                bool_t SDOclientBlockTransfer,
                                uint8_t nodeId,
                                uint32_t *errInfo)
{
	/* variables */
    (void)SDOclientTimeoutTime_ms;
    (void)SDOclientBlockTransfer;
    CO_ReturnError_t err;

    /* check inputs */
    if ((co == NULL) || ((CO_GET_CNT(NMT) == 0 && NMT == NULL)) || ((CO_GET_CNT(EM) == 0 && em == NULL)))
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /*
     * NMT and em inputs are passed to the function during initialization,
     * take the default ones (predefined by us).
     */
    if (CO_GET_CNT(NMT) == 0)
    {
        co->NMT = NMT;
    }
    if (em == NULL)
    {
        em = co->em;
    }

    /* verify configuration */
    co->nodeIdUnconfigured = 0;

    /* initialize LSS slave */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    if (CO_GET_CNT(LSS_SLV) == 1 && nodeId == CO_LSS_NODE_ID_ASSIGNMENT)
    {
        co->nodeIdUnconfigured = 1;
    }
    else
#endif
    if (nodeId < 1 || nodeId > 127)
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* initialize LEDs */
#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    if (CO_GET_CNT(LEDS) == 1)
    {
        err = CO_LEDs_init(co->LEDs);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* CANopen Node ID is unconfigured, stop initialization */
    if (co->nodeIdUnconfigured)
    {
        return CO_ERROR_NODE_ID_UNCONFIGURED_LSS;
    }

    /* initialize EMCY */
    if (CO_GET_CNT(EM) == 1)
    {
        err = CO_EM_init(co->em,
                         co->CANmodule,
                         OD_GET(H1001, OD_H1001_ERR_REG),
 #if (CO_CONFIG_EM) & (CO_CONFIG_EM_PRODUCER | CO_CONFIG_EM_HISTORY)
                         co->em_fifo,
                         (CO_GET_CNT(ARR_1003) + 1),
 #endif
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PRODUCER
                         OD_GET(H1014, OD_H1014_COBID_EMERGENCY),
                         CO_GET_CO(TX_IDX_EM_PROD),
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_PROD_INHIBIT
                         OD_GET(H1015, OD_H1015_INHIBIT_TIME_EMCY),
 #endif
 #endif
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_HISTORY
                         OD_GET(H1003, OD_H1003_PREDEF_ERR_FIELD),
 #endif
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_STATUS_BITS
                         OD_statusBits,
 #endif
 #if (CO_CONFIG_EM) & CO_CONFIG_EM_CONSUMER
                         co->CANmodule,
                         CO_GET_CO(RX_IDX_EM_CONS),
 #endif
                         nodeId,
                         errInfo);
        if (err)
        {
        	return err;
        }
    }

    /* initialize NMT */
    if (CO_GET_CNT(NMT) == 1)
    {
        err = CO_NMT_init(co->NMT,
                          OD_GET(H1017, OD_H1017_PRODUCER_HB_TIME),
                          em,
                          nodeId,
                          NMTcontrol,
                          firstHBTime_ms,
                          co->CANmodule,
                          CO_GET_CO(RX_IDX_NMT_SLV),
                          CO_CAN_ID_NMT_SERVICE,
 #if (CO_CONFIG_NMT) & CO_CONFIG_NMT_MASTER
                          co->CANmodule,
                          CO_GET_CO(TX_IDX_NMT_MST),
                          CO_CAN_ID_NMT_SERVICE,
 #endif
                          co->CANmodule,
                          CO_GET_CO(TX_IDX_HB_PROD),
                          CO_CAN_ID_HEARTBEAT + nodeId,
                          errInfo);
        if (err)
        {
        	return err;
        }
    }

    /* initialize HB consumers */
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
    if (CO_GET_CNT(HB_CONS) == 1)
    {
        err = CO_HBconsumer_init(co->HBcons,
                                 em,
                                 co->HBconsMonitoredNodes,
                                 CO_GET_CNT(ARR_1016),
                                 OD_GET(H1016, OD_H1016_CONSUMER_HB_TIME),
                                 co->CANmodule,
                                 CO_GET_CO(RX_IDX_HB_CONS),
                                 errInfo);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* initialize SDO server */
    if (CO_GET_CNT(SDO_SRV) > 0)
    {
    	/* get SDO server info from OD (normally only one server) */
        OD_entry_t *SDOsrvPar = OD_GET(H1200, OD_H1200_SDO_SERVER_1_PARAM);

        for (int16_t i = 0; i < CO_GET_CNT(SDO_SRV); i++)
        {
            err = CO_SDOserver_init(&co->SDOserver[i],
                                    od,
                                    SDOsrvPar++,
                                    nodeId,
                                    SDOserverTimeoutTime_ms,
                                    co->CANmodule,
                                    CO_GET_CO(RX_IDX_SDO_SRV) + i,
                                    co->CANmodule,
                                    CO_GET_CO(TX_IDX_SDO_SRV) + i,
                                    errInfo);
            if (err)
            {
            	return err;
            }
        }
    }

    /* initialize SDO client */
#if (CO_CONFIG_SDO_CLI) & CO_CONFIG_SDO_CLI_ENABLE
    if (CO_GET_CNT(SDO_CLI) > 0)
    {
    	/* get SDO client info from OD (normally only one server) */
        OD_entry_t *SDOcliPar = OD_GET(H1280, OD_H1280_SDO_CLIENT_1_PARAM);

        for (int16_t i = 0; i < CO_GET_CNT(SDO_CLI); i++)
        {
            err = CO_SDOclient_init(&co->SDOclient[i],
                                    od,
                                    SDOcliPar++,
                                    nodeId,
                                    co->CANmodule,
                                    CO_GET_CO(RX_IDX_SDO_CLI) + i,
                                    co->CANmodule,
                                    CO_GET_CO(TX_IDX_SDO_CLI) + i,
                                    errInfo);
            if (err)
            {
            	return err;
            }
        }
    }
#endif

    /* initialize TIME */
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
    if (CO_GET_CNT(TIME) == 1)
    {
        err = CO_TIME_init(co->TIME,
                           OD_GET(H1012, OD_H1012_COBID_TIME),
                           co->CANmodule,
                           CO_GET_CO(RX_IDX_TIME),
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_PRODUCER
                           co->CANmodule,
                           CO_GET_CO(TX_IDX_TIME),
#endif
                           errInfo);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* initialize SYNC */
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
    if (CO_GET_CNT(SYNC) == 1)
    {
        err = CO_SYNC_init(co->SYNC,
                           em,
                           OD_GET(H1005, OD_H1005_COBID_SYNC),
                           OD_GET(H1006, OD_H1006_COMM_CYCL_PERIOD),
                           OD_GET(H1007, OD_H1007_SYNC_WINDOW_LEN),
                           OD_GET(H1019, OD_H1019_SYNC_CNT_OVERFLOW),
                           co->CANmodule,
                           CO_GET_CO(RX_IDX_SYNC),
#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_PRODUCER
                           co->CANmodule,
                           CO_GET_CO(TX_IDX_SYNC),
#endif
                           errInfo);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* initialize GFC */
#if (CO_CONFIG_GFC) & CO_CONFIG_GFC_ENABLE
    if (CO_GET_CNT(GFC) == 1)
    {
        err = CO_GFC_init(co->GFC,
                          &OD_globalFailSafeCommandParameter,
                          co->CANmodule,
                          CO_GET_CO(RX_IDX_GFC),
                          CO_CAN_ID_GFC,
                          co->CANmodule,
                          CO_GET_CO(TX_IDX_GFC),
                          CO_CAN_ID_GFC);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* initialize SRDO */
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
/*
 * NOTE: SRDOs can be defined only at compile time. In this implementation
 * 		 it is not possible to dynamically add new SRDOs. To change this
 * 		 behavior, remove the following macro.
 */
#if (CO_GET_CNT(SRDO) > 0)
    {
    	/* common object dictionary error code */
    	ODR_t ret;

    	/*
    	 * SRDO configuration valid flag. This number is like a password used for
    	 * the SRDO module. The value specified in the object dictionary at
    	 * (index = 0x13FEE, subIndex = 1) needs to match the one hard-coded in
    	 * the stack and defined with the macro CO_SRDO_CRC_VALID_CHECH.
    	 */
    	uint8_t flag = 0;

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CRC_CHECK
    	/* CRC check enabled, read flag from object dictionary */
    	OD_entry_t *SRDOvalid = (OD_entry_t *)OD_GET(H13FE, OD_H13FE_SRDO_VALID);
    	ret = OD_get_value(SRDOvalid, 1, (void *)&flag, sizeof(flag), 0);

    	/* exit on error */
    	if (ret != 0)
    	{
    		return CO_ERROR_ILLEGAL_ARGUMENT;
    	}
#else
    	/* CRC check disabled, assign directly proper value */
    	flag = CO_SRDO_CRC_VALID_CHECH;
#endif

    	/* initialize SRDO guard */
        err = CO_SRDOGuard_init(co->SRDOGuard,
        						od,
                                &co->NMT->operatingState,
								flag,
                                OD_H13FE_SRDO_VALID,
                                OD_H13FF_SRDO_CHECKSUM);

        /* exit immediately if guard invalid */
        if (err)
        {
        	return err;
        }

        /*
         * In the object dictionary SRDO communication, mapping and CRC records
         * are represented as standard object dictionary records, they will be
         * transformed in specific SRDO structures in a second step.
         */
        OD_entry_t *SRDOcomm = (OD_entry_t *)OD_GET(H1301, OD_H1301_SRDO_1_PARAM);
        OD_entry_t *SRDOmap = (OD_entry_t *)OD_GET(H1381, OD_H1381_SRDO_1_MAPPING);
        OD_entry_t *SRDOcrc = (OD_entry_t *)OD_GET(H13FF, OD_H13FF_SRDO_CHECKSUM);

        /* loop over SRDOs (no RX - TX distinction) */
        for (int16_t i = 0; i < CO_GET_CNT(SRDO); i++)
        {
        	/*
        	 * Get RX and TX devices for every SRDO. This is redundant since
        	 * only one of the two is used, but for the moment is like this
        	 * for compatibility reasons with the old versions.
        	 */
            uint16_t CANdevRxIdx = CO_GET_CO(RX_IDX_SRDO) + 2 * i;
            uint16_t CANdevTxIdx = CO_GET_CO(TX_IDX_SRDO) + 2 * i;

            /*
             * Get CRC for current SRDO from object dictionary. The (i+1)-th subEntry
             * in SRDOcrc corresponds to the i-th SRDO checksum (skip subEntry zero).
              */
            uint16_t checksum = 0;
            ret = OD_get_value(SRDOcrc, i+1, (void *)&checksum, sizeof(checksum), 0);

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CRC_CHECK
            /* CRC check enabled */
            if (ret != 0)
            {
            	return CO_ERROR_ILLEGAL_ARGUMENT;
            }
#else
            /* CRC check disabled */
            (void)ret;
#endif

            /* initialize specific SRDO */
            err = CO_SRDO_init(&co->SRDO[i],
                               co->SRDOGuard,
                               em,
							   od,
                               nodeId,
                               ((i == 0) ? CO_CAN_ID_SRDO_1 : 0),
                               SRDOcomm++,
                               SRDOmap++,
							   checksum,
                               OD_H1301_SRDO_1_PARAM + i,
                               OD_H1381_SRDO_1_MAPPING + i,
                               co->CANmodule,
                               CANdevRxIdx,
                               CANdevRxIdx + 1,
                               co->CANmodule,
                               CANdevTxIdx,
                               CANdevTxIdx + 1);

            /* at first broken SRDO exit */
            if (err)
            {
            	return err;
            }
        }
    }
#endif
#endif

    /* initialize LSS master */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_MASTER
    if (CO_GET_CNT(LSS_MST) == 1)
    {
        err = CO_LSSmaster_init(co->LSSmaster,
                                CO_LSSmaster_DEFAULT_TIMEOUT,
                                co->CANmodule,
                                CO_GET_CO(RX_IDX_LSS_MST),
                                CO_CAN_ID_LSS_SLV,
                                co->CANmodule,
                                CO_GET_CO(TX_IDX_LSS_MST),
                                CO_CAN_ID_LSS_MST);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* initialize GTW */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    /*
     * Verify gateway module. TODO: not implemented, keep for future but
     * don't define the activation macro.
     */
    if (CO_GET_CNT(GTWA) == 1)
    {
        err = CO_GTWA_init(co->gtwa,
 #if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
                           &co->SDOclient[0],
                           SDOclientTimeoutTime_ms,
                           SDOclientBlockTransfer,
 #endif
 #if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT
                           co->NMT,
 #endif
 #if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
                           co->LSSmaster,
 #endif
 #if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
                           co->LEDs,
 #endif
                           0);
        if (err)
        {
        	return err;
        }
    }
#endif

    /* initialize TRACE */
#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
    /*
     * Verify trace module. TODO: not implemented, keep for future but
     * don't define the activation macro.
     */
    if (CO_GET_CNT(TRACE) > 0)
    {
        for (uint16_t i = 0; i < CO_GET_CNT(TRACE); i++)
        {
            err = CO_trace_init(co->trace[i],
                                co->SDO[0],
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
            if (err)
            {
            	return err;
            }
        }
    }
#endif

    return CO_ERROR_NO;
}

/*
 * Initialize CANopenNode PDO objects. Function must be called in the
 * end of communication reset section after all CANopen and application
 * initialization, otherwise some OD variables won't be mapped into PDO
 * correctly.
 */
CO_ReturnError_t CO_CANopenInitPDO(CO_t *co,
                                   CO_EM_t *em,
                                   OD_t *od,
                                   uint8_t nodeId,
                                   uint32_t *errInfo)
{
	/* check overall CANopen structure */
    if (co == NULL)
    {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* check NodeId */
    if ((nodeId < 1) || (nodeId > 127) || (co->nodeIdUnconfigured))
    {
        return (co->nodeIdUnconfigured) ? CO_ERROR_NODE_ID_UNCONFIGURED_LSS : CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* RPDO */
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
    if (CO_GET_CNT(RPDO) > 0)
    {
    	/* get first communication and mapping parameter pointer */
        OD_entry_t *RPDOcomm = OD_GET(H1400, OD_H1400_RXPDO_1_PARAM);
        OD_entry_t *RPDOmap = OD_GET(H1600, OD_H1600_RXPDO_1_MAPPING);

        /* initialize all RPDOs */
        for (int16_t i = 0; i < CO_GET_CNT(RPDO); i++)
        {
        	/* variables */
            CO_ReturnError_t err;
            uint16_t preDefinedCanId = 0;

            /* check default CAN-ID */
            if (i < CO_RPDO_DEFAULT_CANID_COUNT)
            {
#if CO_RPDO_DEFAULT_CANID_COUNT <= 4
            	/* use 0x100 distant default COB-ID */
                preDefinedCanId = (CO_CAN_ID_RPDO_1 + i * 0x100) + nodeId;
#else
                /* use 0x100 distant default COB-ID (but fill the gaps) */
                uint16_t pdoOffset = i % 4;
                uint16_t nodeIdOffset = i / 4;
                preDefinedCanId = (CO_CAN_ID_RPDO_1 + pdoOffset * 0x100) + nodeId + nodeIdOffset;
#endif
            }

            /* initialize single RPDO */
            err = CO_RPDO_init(&co->RPDO[i],
                               od,
                               em,
 #if (CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE
                               co->SYNC,
 #endif
                               preDefinedCanId,
                               RPDOcomm++,
                               RPDOmap++,
                               co->CANmodule,
                               CO_GET_CO(RX_IDX_RPDO) + i,
                               errInfo);
            if (err)
            {
            	return err;
            }
        }
    }
#endif

    /* TPDO */
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
    if (CO_GET_CNT(TPDO) > 0)
    {
    	/* get first communication and mapping parameter pointer */
        OD_entry_t *TPDOcomm = OD_GET(H1800, OD_H1800_TXPDO_1_PARAM);
        OD_entry_t *TPDOmap = OD_GET(H1A00, OD_H1A00_TXPDO_1_MAPPING);

        /* initialize all TPDOs */
        for (int16_t i = 0; i < CO_GET_CNT(TPDO); i++)
        {
            CO_ReturnError_t err;
            uint16_t preDefinedCanId = 0;

            /* check default CAN-ID */
            if (i < CO_TPDO_DEFAULT_CANID_COUNT)
            {
#if CO_TPDO_DEFAULT_CANID_COUNT <= 4
            	/* use 0x100 distant default COB-ID */
                preDefinedCanId = (CO_CAN_ID_TPDO_1 + i * 0x100) + nodeId;
#else
                /* use 0x100 distant default COB-ID (but fill the gaps) */
                uint16_t pdoOffset = i % 4;
                uint16_t nodeIdOffset = i / 4;
                preDefinedCanId = (CO_CAN_ID_TPDO_1 + pdoOffset * 0x100) + nodeId + nodeIdOffset;
#endif
            }

            /* initialize single TPDO */
            err = CO_TPDO_init(&co->TPDO[i],
                               od,
                               em,
 #if (CO_CONFIG_PDO) & CO_CONFIG_PDO_SYNC_ENABLE
                               co->SYNC,
 #endif
                               preDefinedCanId,
                               TPDOcomm++,
                               TPDOmap++,
                               co->CANmodule,
                               CO_GET_CO(TX_IDX_TPDO) + i,
                               errInfo);
            if (err)
            {
            	return err;
            }
        }
    }
#endif

    return CO_ERROR_NO;
}

/*
 * Process CANopen objects. Function must be called cyclically.
 * It processes all asynchronous CANopen objects.
 */
CO_NMT_reset_cmd_t CO_process(CO_t *co,
                              bool_t enableGateway,
                              uint32_t timeDifference_us,
                              uint32_t *timerNext_us)
{
	/* variables */
    (void) enableGateway;
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_NMT_internalState_t NMTstate = CO_NMT_getInternalState(co->NMT);
    bool_t NMTisPreOrOperational = (NMTstate == CO_NMT_PRE_OPERATIONAL || NMTstate == CO_NMT_OPERATIONAL);

    /* CAN module */
    CO_CANmodule_process(co->CANmodule);

    /* process LSS slaves (first step) */
#if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    if (CO_GET_CNT(LSS_SLV) == 1)
    {
        if (CO_LSSslave_process(co->LSSslave))
        {
            reset = CO_RESET_COMM;
        }
    }
#endif

    /* process LEDS (first step) */
#if (CO_CONFIG_LEDS) & CO_CONFIG_LEDS_ENABLE
    bool_t unc = co->nodeIdUnconfigured;
    uint16_t CANerrorStatus = co->CANmodule->CANerrorStatus;
    bool_t LSSslave_configuration = 0;

    /* process LSS slaves (second step) */
 #if (CO_CONFIG_LSS) & CO_CONFIG_LSS_SLAVE
    if (CO_GET_CNT(LSS_SLV) == 1 && CO_LSSslave_getState(co->LSSslave) == CO_LSS_STATE_CONFIGURATION)
    {
        LSSslave_configuration = 1;
    }
 #endif

 /** default macro, can be defined externally */
 #ifndef CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS
 #define CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS (0)
 #endif

    /* process LEDS (second step) */
    if (CO_GET_CNT(LEDS) == 1)
    {
        CO_LEDs_process(co->LEDs,
            timeDifference_us,
            unc ? CO_NMT_INITIALIZING : NMTstate,
            LSSslave_configuration,
            (CANerrorStatus & CO_CAN_ERRTX_BUS_OFF) != 0,
            (CANerrorStatus & CO_CAN_ERR_WARN_PASSIVE) != 0,
            0, /* RPDO event timer timeout */
            unc ? 0 : CO_isError(co->em, CO_EM_SYNC_TIME_OUT),
            unc ? 0 : (CO_isError(co->em, CO_EM_HEARTBEAT_CONSUMER)
                        || CO_isError(co->em, CO_EM_HB_CONSUMER_REMOTE_RESET)),
            CO_getErrorRegister(co->em) != 0,
            CO_STATUS_FIRMWARE_DOWNLOAD_IN_PROGRESS,
            timerNext_us);
    }
#endif

    /* CANopen Node ID is un configured (LSS slave), stop processing here */
    if (co->nodeIdUnconfigured)
    {
        return reset;
    }

    /* process Emergency */
    if (CO_GET_CNT(EM) == 1)
    {
        CO_EM_process(co->em,
                      NMTisPreOrOperational,
                      timeDifference_us,
                      timerNext_us);
    }

    /* process NMT_Heartbeat */
    if (CO_GET_CNT(NMT) == 1)
    {
        reset = CO_NMT_process(co->NMT,
                               &NMTstate,
                               timeDifference_us,
                               timerNext_us);
    }

    /* get NMT state */
    NMTisPreOrOperational = (NMTstate == CO_NMT_PRE_OPERATIONAL || NMTstate == CO_NMT_OPERATIONAL);

    /* process SDOserver */
    for (uint8_t i = 0; i < CO_GET_CNT(SDO_SRV); i++)
    {
        CO_SDOserver_process(&co->SDOserver[i],
                             NMTisPreOrOperational,
                             timeDifference_us,
                             timerNext_us);
    }

    /* process HB consumer */
#if (CO_CONFIG_HB_CONS) & CO_CONFIG_HB_CONS_ENABLE
    if (CO_GET_CNT(HB_CONS) == 1)
    {
        CO_HBconsumer_process(co->HBcons,
                              NMTisPreOrOperational,
                              timeDifference_us,
                              timerNext_us);
    }
#endif

    /* process TIME */
#if (CO_CONFIG_TIME) & CO_CONFIG_TIME_ENABLE
    if (CO_GET_CNT(TIME) == 1)
    {
        CO_TIME_process(co->TIME, NMTisPreOrOperational, timeDifference_us);
    }
#endif

    /* process GTW. TODO: don't enable macro */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII
    if (CO_GET_CNT(GTWA) == 1)
    {
        CO_GTWA_process(co->gtwa,
                        enableGateway,
                        timeDifference_us,
                        timerNext_us);
    }
#endif

    /*
     * Return value tells the main task if an error has occurred
     * and a module reset is needed.
     */
    return reset;
}


#if (CO_CONFIG_SYNC) & CO_CONFIG_SYNC_ENABLE
/*
 * Process CANopen SYNC objects. Function must be called cyclically.
 * For time critical applications it may be called from real time thread
 * with constant interval (1ms typically). It processes SYNC CANopen objects.
 */
bool_t CO_process_SYNC(CO_t *co,
                       uint32_t timeDifference_us,
                       uint32_t *timerNext_us)
{
	/* test sync */
    bool_t syncWas = 0;

    if (!co->nodeIdUnconfigured && CO_GET_CNT(SYNC) == 1)
    {
    	/* get state */
        CO_NMT_internalState_t NMTstate = CO_NMT_getInternalState(co->NMT);
        bool_t NMTisPreOrOperational = (NMTstate == CO_NMT_PRE_OPERATIONAL || NMTstate == CO_NMT_OPERATIONAL);

        /* process synch */
        CO_SYNC_status_t sync_process = CO_SYNC_process(co->SYNC,
                                                        NMTisPreOrOperational,
                                                        timeDifference_us,
                                                        timerNext_us);
        switch (sync_process)
        {
            case CO_SYNC_NONE:
            {
                break;
            }
            case CO_SYNC_RX_TX:
            {
                syncWas = 1;
                break;
            }
            case CO_SYNC_PASSED_WINDOW:
            {
                CO_CANclearPendingSyncPDOs(co->CANmodule);
                break;
            }
        }
    }

    return syncWas;
}
#endif

#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_ENABLE
/**
 * Process CANopen RPDO objects. Function must be called cyclically.
 * For time critical applications it may be called from real time
 * thread with constant interval (1ms typically). It processes receive
 * PDO CANopen objects.
 */
void CO_process_RPDO(CO_t *co,
                     bool_t syncWas,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
	/* variables */
    (void)timeDifference_us;
    (void)timerNext_us;

    /* check if configured */
    if (co->nodeIdUnconfigured)
    {
        return;
    }

    /* get NMT state */
    bool_t NMTisOperational = CO_NMT_getInternalState(co->NMT) == CO_NMT_OPERATIONAL;

    for (int16_t i = 0; i < CO_GET_CNT(RPDO); i++)
    {
    	/* process every valid RPDO */
        CO_RPDO_process(&co->RPDO[i],
#if (CO_CONFIG_PDO) & CO_CONFIG_RPDO_TIMERS_ENABLE
                        timeDifference_us,
                        timerNext_us,
#endif
                        NMTisOperational,
                        syncWas);
    }
}
#endif

#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_ENABLE
/*
 * Process CANopen TPDO objects. Function must be called cyclically. For time
 * critical applications it may be called from real time thread with constant
 * interval (1ms typically). It processes transmit PDO CANopen objects.
 */
void CO_process_TPDO(CO_t *co,
                     bool_t syncWas,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
	/* variables */
    (void) timeDifference_us;
    (void) timerNext_us;

    /* check if configured */
    if (co->nodeIdUnconfigured)
    {
        return;
    }

    /* get NMT state */
    bool_t NMTisOperational = CO_NMT_getInternalState(co->NMT) == CO_NMT_OPERATIONAL;

    for (int16_t i = 0; i < CO_GET_CNT(TPDO); i++)
    {
    	/* process every valid TPDO */
        CO_TPDO_process(&co->TPDO[i],
#if (CO_CONFIG_PDO) & CO_CONFIG_TPDO_TIMERS_ENABLE
                        timeDifference_us,
                        timerNext_us,
#endif
                        NMTisOperational,
                        syncWas);
    }
}
#endif

#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
/*
 * Process CANopen SRDO objects. Function must be called cyclically. For time
 * critical applications it may be called from real time thread with constant
 * interval (1ms typically). It processes SRDO CANopen objects.
 */
void CO_process_SRDO(CO_t *co,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
	/* check if configured */
    if (co->nodeIdUnconfigured)
    {
        return;
    }
/*
 * SRDOs can be defined only at compile time. In this implementation
 * it is not possible to dynamically add new SRDOs.
 * To change this behavior, remove this macro.
 */
#if (CO_GET_CNT(SRDO) > 0)

    /*
     * Check if this is the first activation. At the moment the stack
     * doesn't support dynamic modification of SRDO, therefore this
     * check is meaningful only the first time (during initialization).
     */
    uint8_t firstOperational = CO_SRDOGuard_process(co->SRDOGuard);

    for (int16_t i = 0; i < CO_GET_CNT(SRDO); i++)
    {
    	/* process every valid SRDO */
        CO_SRDO_process(&co->SRDO[i],
                        firstOperational,
                        timeDifference_us,
                        timerNext_us);
    }
#endif
}
#endif
