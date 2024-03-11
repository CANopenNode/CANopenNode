/**
 * \file CO_SRDO.c
 *
 * \brief SRDO implementation.
 *
 */

/***********************************************************************************************************************
 * Includes
 **********************************************************************************************************************/
#include "CO_SRDO.h"
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE
#include "crc16-ccitt.h"
/* verify configuration */
#if !((CO_CONFIG_CRC16) & CO_CONFIG_CRC16_ENABLE)
#error CO_CONFIG_CRC16_ENABLE must be enabled.
#endif


/***********************************************************************************************************************
 * Definitions
 **********************************************************************************************************************/
#define CO_SRDO_INVALID          (0U)
#define CO_SRDO_TX               (1U)
#define CO_SRDO_RX               (2U)


/***********************************************************************************************************************
 * Private function prototypes
 **********************************************************************************************************************/
/**
 * Copy source buffer to target buffer taking into account cpu-endianness.
 */
static void memcopy(uint8_t dest[], const uint8_t src[], uint16_t size);

/**
 * Configure object dictionary entry extension, used only for SRDOs.
 */
static void CoSrdoOdExtConfig(OD_t *OD,
					   	      uint16_t index,
							  ODR_t (*pRead)(OD_stream_t *stream,
									  	     void *buf, OD_size_t count,
											 OD_size_t *countRead),
							  ODR_t (*pWrite)(OD_stream_t *stream,
									  	  	  const void *buf,
											  OD_size_t count,
											  OD_size_t *countWritten),
							  void *object,
							  uint8_t *flags,
							  uint8_t flagsSize);

/**
 * SRDO communication record extension read callback.
 */
static ODR_t CoSrdoCommReadCb(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead);

/**
 * SRDO communication record extension write callback.
 */
static ODR_t CoSrdoCommWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten);

/**
 * SRDO mapping record extension read callback.
 */
static ODR_t CoSrdoMapReadCb(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead);

/**
 * SRDO mapping record extension write callback.
 */
static ODR_t CoSrdoMapWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten);

/**
 * SRDO Guard CRC extension write callback.
 */
static ODR_t CoSrdoGuardCrcWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten);

/**
 * SRDO Guard valid extension write callback.
 */
static ODR_t CoSrdoGuardValidWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten);

/**
 * Initialize SRDO specific communication record from object dictionary entry.
 */
static ODR_t CoSrdoOdCommInit(CO_SRDO_t *SRDO, OD_entry_t *entry);

/**
 * Initialize SRDO specific mapping object array from object dictionary entry.
 */
static ODR_t CoSrdoOdMapInit(CO_SRDO_t *SRDO, OD_entry_t *entry);

/**
 * Calculate SRDO CRC.
 */
static uint16_t CO_SRDOcalcCrc(const CO_SRDO_t *SRDO);

/**
 * Receive CAN frames for SRDO.
 */
static void CO_SRDO_receive_normal(void *object, void *msg);

/**
 * Receive CAN frames inverted for SRDO.
 */
static void CO_SRDO_receive_inverted(void *object, void *msg);

/**
 * Configure mapping part for SRDO.
 */
static ODR_t CO_SRDOconfigMap(CO_SRDO_t* SRDO, uint8_t noOfMappedObjects);

/**
 * Find SRDO object given mapping.
 */
static ODR_t CO_SRDOfindMap(OD_t *OD,
							uint32_t map,
							uint8_t R_T,
							uint8_t **ppData,
							uint8_t *pLength,
							uint8_t *pSendIfCOSFlags,
							uint8_t *pIsMultibyteVar);

/**
 * Configure communication part for SRDO.
 */
static void CO_SRDOconfigCom(CO_SRDO_t* SRDO, uint32_t COB_IDnormal, uint32_t COB_IDinverted);


/***********************************************************************************************************************
 * Private functions
 **********************************************************************************************************************/
/**
 * Copy source buffer to target buffer taking into account cpu-endianness.
 */
static void memcopy(uint8_t dest[], const uint8_t src[], uint16_t size)
{
	/* check input */
	if ((dest != NULL) && (src != NULL))
	{
		/* swap and copy bytes */
		for (uint16_t i = 0; i < size; i++)
		{
#ifdef CO_LITTLE_ENDIAN
		dest[i] = src[i];
#else
		dest[i] = src[size-i-1];
#endif
		}
	}
}

/**
 * Configure object dictionary entry extension, used only for SRDOs.
 */
static void CoSrdoOdExtConfig(OD_t *OD,
							  uint16_t index,
							  ODR_t (*pRead)(OD_stream_t *stream,
									  	  	 void *buf,
											 OD_size_t count,
											 OD_size_t *countRead),
							  ODR_t (*pWrite)(OD_stream_t *stream,
									  	  	  const void *buf,
											  OD_size_t count,
											  OD_size_t *countWritten),
							  void *object,
							  uint8_t *flags,
							  uint8_t flagsSize)
{
    /* get OD entry */
    OD_entry_t *entry = OD_find(OD, index);

    /* check if entry exists */
    if (entry != NULL)
    {
    	/* get entry extensions */
    	OD_extension_t *ext = entry->extension;

    	/* check extensions */
        if (ext != NULL)
        {
        	/* number of sub-entries */
        	uint8_t maxSubIndex = entry->subEntriesCount;

			/* set extension fields */
			ext->read = pRead;
			ext->write = pWrite;
			ext->object = object;

			/* set flags */
			if ((flags != NULL) && (flagsSize != 0U) && (flagsSize == maxSubIndex))
			{
				for (uint16_t i = 0U; i <= maxSubIndex; i++)
				{
					ext->flagsPDO[i] = 0U;
				}
			}
        }
    }
}

/**
 * Initialize SRDO specific communication record from object dictionary entry.
 */
static ODR_t CoSrdoOdCommInit(CO_SRDO_t *SRDO, OD_entry_t *entry)
{
	/* return value */
	ODR_t ret = ODR_OK;

	/* check entry passed*/
    if (entry != NULL)
    {
        /* maxSubIndex */
        ret |= OD_get_value(entry,
        					0,
							(void *)&SRDO->SRDOCommPar.maxSubIndex,
							sizeof(SRDO->SRDOCommPar.maxSubIndex),
							0);

        /* informationDirection */
        ret |= OD_get_value(entry,
        					1,
							(void *)&SRDO->SRDOCommPar.informationDirection,
							sizeof(SRDO->SRDOCommPar.informationDirection),
							0);

        /* refreshTimeOrSCT */
        ret |= OD_get_value(entry,
        					2,
							(void *)&SRDO->SRDOCommPar.refreshTimeOrSCT,
							sizeof(SRDO->SRDOCommPar.refreshTimeOrSCT),
							0);

        /* SRVT */
        ret |= OD_get_value(entry,
        					3,
							(void *)&SRDO->SRDOCommPar.SRVT,
							sizeof(SRDO->SRDOCommPar.SRVT),
							0);

        /* transmissionType */
        ret |= OD_get_value(entry,
        					4,
							(void *)&SRDO->SRDOCommPar.transmissionType,
							sizeof(SRDO->SRDOCommPar.transmissionType),
							0);

        /* COB_ID_1 */
        ret |= OD_get_value(entry,
        					5,
							(void *)&SRDO->SRDOCommPar.COB_ID_1,
							sizeof(SRDO->SRDOCommPar.COB_ID_1),
							0);

        /* COB_ID_2 */
        ret |= OD_get_value(entry,
        					6,
							(void *)&SRDO->SRDOCommPar.COB_ID_2,
							sizeof(SRDO->SRDOCommPar.COB_ID_2),
							0);

        /*
         * CAN channel TX only: RX SRDO also has the channel information
         * but is unused, since all associated RxFifos are read every
         * 1ms cycle anyway, but keep it for future.
         */
        if (SRDO->SRDOCommPar.informationDirection == CO_SRDO_TX)
        {
            /*
             * The channel field is in the communication record because
             * during the initialization the TX buffers are not yet
             * initialized. The assignment from the communication record
             * to the TX buffers is done in CO_SRDOconfigCom().
             * NOTE: this is fundamental otherwise the channel information
             * 		 would be lost and no (Tx)SRDOs will be sent.
             */
            ret |= OD_get_value(entry,
            					7,
    							(void *)&SRDO->SRDOCommPar.channel,
    							sizeof(SRDO->SRDOCommPar.channel),
    							0);
        }
    }
    else
    {
    	/* subIndex zero not found or broken */
    	ret = ODR_NO_MAP;
    }

    return ret;
}

/**
 * Initialize SRDO specific mapping record from object dictionary entry.
 * This function is responsible for setting up the mappedObjects array
 * and the mapping record configuration is done later in a second step.
 */
static ODR_t CoSrdoOdMapInit(CO_SRDO_t *SRDO, OD_entry_t *entry)
{
	/* return value */
	ODR_t ret = ODR_OK;

	/* check entry passed */
    if (entry != NULL)
    {
        /*
         * Set numberOfMappedObjects individually. SubIndex zero is mandatory,
         * therefore if it is not found stop immediately and return error.
         */
        ret = OD_get_value(entry,
        				   0,
						   (void *)&SRDO->SRDOMapPar.numberOfMappedObjects,
						   sizeof(SRDO->SRDOMapPar.numberOfMappedObjects),
						   0);

        /* check if number of mapped objects is fine */
        if (ret == ODR_OK)
        {
        	/* mapped object counter */
        	uint8_t j = 0;

			/*
			 * Set all mappedObjects in array two by two, since SRDO mapping requires normal and
			 * inverted maps to be in consecutive positions (subIndexes). The stop condition
			 * can also be (j < SRDO->SRDOMapPar.numberOfMappedObjects), however in this way it
			 * is clear that an out of bound array is not possible (for static code analyzer).
			 */
			for (uint8_t i = 1; i < CO_SRDO_MAX_MAPPED_ENTRIES && j < SRDO->SRDOMapPar.numberOfMappedObjects-1; i+=2)
			{
				/* temporary buffer */
				uint32_t data[2] = {0, 0};

				/* error code */
				ODR_t odRet = 0;

				/*
				 * Get couple of associated entries (normal, inverted). Note that in the
				 * EDS file they need to be in consecutive position.
				 */
				for (uint8_t k = 0; k < 2; k++)
				{
					odRet |= OD_get_value(entry, i+k, (void *)&data[k], sizeof(data[k]), 0);
				}

				/*
				 * If index does not exist for both subIndeces, continue. This logic allows to
				 * have non contiguous sub-indeces in the PDO map, meaning that both Arrays and
				 * Records are supported.
				 */
				if (odRet == ODR_SUB_NOT_EXIST)
				{
					continue;
				}
		        /*
		         * Check if both SRDO maps (normal, inverted) are valid. TODO: add some meaningful
		         * check to understand if the two objects are compatible (i.e. data type compatible,
		         * pointed objects types compatible, ...). The advantage of reading subIndexes two
		         * by two is that a comparison between normal an inverted maps is easy.
		         */
				else if (odRet != ODR_OK)
		        {
		            /* stop immediately */
		            ret = ODR_NO_MAP;
		            break;
		        }
				else
				{
					/* set both SRDO map object entries */
					memcpy(&SRDO->SRDOMapPar.mappedObjects[j], &data[0], sizeof(data));

					/* increase mapped object counter */
					j+=2;
				}
			}
        }
        else
        {
        	/* map length error */
        	ret = ODR_MAP_LEN;
        }
    }
    else
    {
    	/* subIndex zero not found means broken map */
    	ret = ODR_NO_MAP;
    }

    return ret;
}

/**
 * SRDO communication record extension read callback.
 */
static ODR_t CoSrdoCommReadCb(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
	/* error code */
	ODR_t ret = ODR_OK;

	/* get SRDO object */
	CO_SRDO_t *SRDO = (CO_SRDO_t*)stream->object;

	/* COB_ID1 and COB_ID2 */
    if ((stream->subIndex == 5) || (stream->subIndex == 6))
    {
    	/* get data */
        uint32_t value;
        memcpy(&value, stream->dataOrig, sizeof(value));

        /* get default COD_ID index */
        uint16_t index = stream->subIndex - 5;

        /* if default COB ID is used, write default value here */
        if (((value & 0x7FF) == SRDO->defaultCOB_ID[index]) && (SRDO->nodeId <= 64))
        {
        	value += SRDO->nodeId;
        }

        /* if SRDO is not valid, set bit 31 */
        memcpy(stream->dataOrig, &value, sizeof(value));
    }

    return ret;
}

/**
 * SRDO communication record extension write callback.
 */
static ODR_t CoSrdoCommWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
	/* get SRDO object */
	CO_SRDO_t *SRDO = (CO_SRDO_t*)stream->object;

    /* check NMT */
    if (*SRDO->SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
    {
    	/* data cannot be transferred or stored to the application */
    	return ODR_DATA_DEV_STATE;
    }

    /* get data pointer */
    uint8 *data = (uint8 *)stream->dataOrig;

    /* check data availability */
    if (data == NULL)
    {
    	return ODR_NO_DATA;
    }

    /* information direction */
    if (stream->subIndex == 1)
    {
        uint8_t value = data[0];
        if (value > 2)
        {
        	return ODR_INVALID_VALUE;
        }
    }
    /* refresh time or SCT */
    else if (stream->subIndex == 2)
    {
        uint16_t value;
        memcpy(&value, data, sizeof(value));
        if (value == 0)
        {
        	return ODR_INVALID_VALUE;
        }
    }
    /* SRVT */
    else if (stream->subIndex == 3)
    {
        uint8_t value = data[0];
        if (value == 0)
        {
        	return ODR_INVALID_VALUE;
        }
    }
    /* transmission type */
    else if (stream->subIndex == 4)
    {
    	uint8_t value = data[0];
        if (value != 254)
        {
        	/* invalid value for parameter (download only) */
        	return ODR_INVALID_VALUE;
        }
    }
    /* COB_ID1 and COB_ID2 */
    else if ((stream->subIndex == 5) || (stream->subIndex == 6))
    {
        uint32_t value;
        memcpy(&value, data, sizeof(value));

        /*
         * Get position [0, 1] for normal and inverted message. TODO: make it
         * more robust and independent from subIndex order.
         */
        uint16_t index = stream->subIndex-5;

        /* check value range */
        if ((value < 0x101) || (value > 0x180) || ((value & 1) == index))
        {
        	/* invalid value for parameter (download only) */
        	return ODR_INVALID_VALUE;
        }

        /* if default COB-ID is being written, write default COB_ID without nodeId */
        if ((SRDO->nodeId <= 64) && (value == (SRDO->defaultCOB_ID[index] + SRDO->nodeId)))
        {
            value = SRDO->defaultCOB_ID[index];
            memcpy(data, &value, sizeof(value));
        }
    }
    /* channel */
    else if (stream->subIndex == 7)
    {
    	uint8_t value = data[0];
        if ((value > 3) && (value != 0xAA) && (value != 0xFF))
        {
          	/* invalid value for channel */
           	return ODR_INVALID_VALUE;
        }
    }

    /* mark invalid (needs further check) */
    SRDO->SRDOGuard->configurationValid = ODR_INVALID_VALUE;

    return ODR_OK;
}

/**
 * SRDO mapping record extension read callback.
 */
static ODR_t CoSrdoMapReadCb(OD_stream_t *stream, void *buf, OD_size_t count, OD_size_t *countRead)
{
	/* nothing to do */
    return ODR_OK;
}

/**
 * SRDO mapping record extension write callback.
 */
static ODR_t CoSrdoMapWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
	/* get SRDO object */
	CO_SRDO_t *SRDO = (CO_SRDO_t*)stream->object;

	/* check NMT */
    if (*(SRDO->SRDOGuard->operatingState) == CO_NMT_OPERATIONAL)
    {
    	/* data cannot be transferred or stored to the application */
    	return ODR_DATA_DEV_STATE;
    }

    /* check if SRDO is still valid */
    if (SRDO->SRDOCommPar.informationDirection != 0)
    {
    	/* SRDO must be deleted */
    	return ODR_UNSUPP_ACCESS;
    }

    /* get data pointer */
    uint8 *data = (uint8 *)stream->dataOrig;

    /* check data availability */
    if (data == NULL)
    {
     	return ODR_NO_DATA;
    }

    /* numberOfMappedObjects */
    if (stream->subIndex == 0)
    {
        uint8_t value = data[0];
        /*
         * Only even numbers are allowed, since there are [n] pairs of
         * (normal, inverted) objects in the SRDO maps.
         */
        if ((value > 16) || (value & 1))
		{
        	return ODR_MAP_LEN;
        }
    }
    else
    {
    	/*
    	 * Only the number of mapped objects (subIndex zero) can be modified
    	 * here, the other maps are created by the CoSrdoOdMapInit function.
    	 */
        if (SRDO->SRDOMapPar.numberOfMappedObjects != 0)
        {
        	return ODR_UNSUPP_ACCESS;
        }
    }

    /* mark invalid (needs further processing) */
    SRDO->SRDOGuard->configurationValid = ODR_INVALID_VALUE;

    return ODR_OK;
}

/**
 * SRDO Guard CRC read callback.
 */
static ODR_t CoSrdoGuardCrcWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
	/* error code */
	ODR_t ret = ODR_OK;

	/* get SRDO guard object */
    CO_SRDOGuard_t *SRDOGuard = (CO_SRDOGuard_t*)stream->object;

    /*
     * If this function is called means a read or write is in progress
     * and this cannot happen in operational state.
     */
    if (*SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
    {
    	ret = ODR_DATA_DEV_STATE;
    }

    /* mark invalid (needs further processing) */
    SRDOGuard->configurationValid = ODR_INVALID_VALUE;

    return ret;
}

/**
 * SRDO Guard valid write callback.
 */
static ODR_t CoSrdoGuardValidWriteCb(OD_stream_t *stream, const void *buf, OD_size_t count, OD_size_t *countWritten)
{
    /* get SRDO guard object */
    CO_SRDOGuard_t *SRDOGuard = (CO_SRDOGuard_t *) stream->object;

    /*
     * If this function is called means a read or write is in progress
     * and this cannot happen in operational state.
     */
    if (*SRDOGuard->operatingState == CO_NMT_OPERATIONAL)
    {
    	return ODR_DATA_DEV_STATE;
    }

    /* get data pointer */
    uint8 *data = (uint8 *)stream->dataOrig;

    /* check data availability */
    if (data == NULL)
    {
       	return ODR_NO_DATA;
    }

    /* check CRC */
    SRDOGuard->checkCRC = (uint8_t)(data[0] == CO_SRDO_CRC_VALID_CHECH);

    return ODR_OK;
}

/**
 * Receive normal CAN frames for SRDO.
 */
static void CO_SRDO_receive_normal(void *object, void *msg)
{
	/* variables */
    CO_SRDO_t *SRDO = (CO_SRDO_t*)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    /*
     * Check SRDO data: this operation copies the received CAN message in
     * CANrxNew[0], but is allowed only if the last inverted SRDO message
     * in CANrxNew[1] has been processed and erased from buffer.
     */
    if ((SRDO->valid == CO_SRDO_RX) && (DLC >= SRDO->dataLength) && !CO_FLAG_READ(SRDO->CANrxNew[1]))
    {
        /* copy data into normal data buffer */
        memcpy(SRDO->CANrxData[0], data, sizeof(SRDO->CANrxData[0]));

        /* mark buffer as to process */
        CO_FLAG_SET(SRDO->CANrxNew[0]);

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
        /*
         * Optional signal to RTOS, which can resume task, which handles SRDO.
         * NOTE: useless now but keep it for the moment.
         */
        if (SRDO->pFunctSignalPre != NULL)
        {
            SRDO->pFunctSignalPre(SRDO->functSignalObjectPre);
        }
#endif
    }
}

/**
 * Receive inverted CAN frames for SRDO.
 */
static void CO_SRDO_receive_inverted(void *object, void *msg)
{
	/* variables */
    CO_SRDO_t *SRDO = (CO_SRDO_t*)object;
    uint8_t DLC = CO_CANrxMsg_readDLC(msg);
    uint8_t *data = CO_CANrxMsg_readData(msg);

    /*
     * Check SRDO data: this operation copies the received CAN message in
     * CANrxNew[1], but is allowed only if the last normal SRDO message
     * in CANrxNew[0] has been processed and erased from buffer.
     */
    if ((SRDO->valid == CO_SRDO_RX) && (DLC >= SRDO->dataLength) && CO_FLAG_READ(SRDO->CANrxNew[0]))
    {
    	/* copy data into inverted data buffer */
        memcpy(SRDO->CANrxData[1], data, sizeof(SRDO->CANrxData[1]));

        /* mark buffer as to process */
        CO_FLAG_SET(SRDO->CANrxNew[1]);

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
        /*
         * Optional signal to RTOS, which can resume task, which handles SRDO.
         * NOTE: useless now but keep it for the moment.
         */
        if (SRDO->pFunctSignalPre != NULL)
        {
            SRDO->pFunctSignalPre(SRDO->functSignalObjectPre);
        }
#endif
    }
}

/**
 * Configure communication part for SRDO.
 */
static void CO_SRDOconfigCom(CO_SRDO_t* SRDO, uint32_t COB_IDnormal, uint32_t COB_IDinverted)
{
	/* variables */
    uint16_t IDs[2][2];
    uint16_t* ID;
    uint16_t successCount = 0;
    uint32_t COB_ID[2];
    COB_ID[0] = COB_IDnormal;
    COB_ID[1] = COB_IDinverted;
    SRDO->valid = CO_SRDO_INVALID;

    /* check if SRDO is used */
    if (((SRDO->SRDOCommPar.informationDirection == CO_SRDO_TX) || (SRDO->SRDOCommPar.informationDirection == CO_SRDO_RX))
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CRC_CHECK
    	/* CRC check enabled */
    	&& (SRDO->SRDOGuard->configurationValid == CO_SRDO_CRC_VALID_CHECH)
#else
		/* CRC check disabled */
    	&& (SRDO->SRDOGuard->configurationValid != ODR_INVALID_VALUE)
#endif
		&& (SRDO->dataLength > 0))
    {
    	/* get COB-ID */
        ID = &IDs[SRDO->SRDOCommPar.informationDirection-1][0];

        /* check if default COB-ID used */
        for (int16_t i = 0; i < 2; i++)
        {
        	/* check if SRDO is valid */
            if (!(COB_ID[i] & 0xBFFFF800L))
            {
            	/* get ID from COB-ID */
                ID[i] = (uint16_t)COB_ID[i] & 0x7FF;

                /* check if default COB-ID is used (and update with NodeId) */
                if (ID[i] == SRDO->defaultCOB_ID[i] && SRDO->nodeId <= 64)
                {
                    ID[i] += 2*SRDO->nodeId;
                }

                /* final range check on ID */
                if ((0x101 <= ID[i]) && (ID[i] <= 0x180) && ((ID[i] & 1) != i))
                {
                    successCount++;
                }
            }
        }
    }
    /* all IDs are correct */
    if (successCount == 2)
    {
    	/* get information direction */
        SRDO->valid = SRDO->SRDOCommPar.informationDirection;

        /* TX SRDO */
        if (SRDO->valid == CO_SRDO_TX)
        {
        	/* initialize timer */
            SRDO->timer = 500 * SRDO->nodeId;
        }
        /* RX SRDO */
        else if (SRDO->valid == CO_SRDO_RX)
        {
        	/* initialize timer */
            SRDO->timer = SRDO->SRDOCommPar.refreshTimeOrSCT * 1000U;
        }
    }
    else
    {
    	/* clear and mark SRDO as invalid */
        memset(IDs, 0, sizeof(IDs));
        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
        SRDO->valid = CO_SRDO_INVALID;
    }

    /* loop over normal and inverted messages */
    for (uint16_t i = 0; i < 2; i++)
    {
        /* initialize TX buffer */
        SRDO->CANtxBuff[i] = CO_CANtxBufferInit(
            SRDO->CANdevTx,         //CAN device.
            SRDO->CANdevTxIdx[i],   //Index of specific buffer inside CAN module.
            IDs[0][i],              //CAN identifier.
            0,                      //Retry (rtr).
            SRDO->dataLength,       //Number of data bytes.
            0);                     //Synchronous message flag bit.

        /* null pointer check */
        if (SRDO->CANtxBuff[i] == NULL)
        {
            SRDO->valid = CO_SRDO_INVALID;
        }
        else
        {
        	/*
        	 * Assign channel to TX buffer. In this case the channel information is
        	 * not present in the SRDO structure, as for TPDOS. The reason is that
        	 * SRDOCommPar is dedicated to SRDOs only and all function dealing with
        	 * it are SRDO specific (they are almost two separate components).
        	 * NOTE: the channel information is still duplicated, since it is present
        	 * in both SRDOCommPar and CANtxBuff.
        	 */
        	SRDO->CANtxBuff[i]->channel = SRDO->SRDOCommPar.channel;
        }

        /* initialize SRDO RX buffer (no need for channel) */
        CO_ReturnError_t err = CO_CANrxBufferInit(
        		SRDO->CANdevRx,         								  	   //CAN device.
				SRDO->CANdevRxIdx[i],   								  	   //Index of specific buffer inside CAN module.
				IDs[1][i],              								  	   //CAN identifier.
				0x7FF,                  								  	   //Mask.
				0,                      								  	   //Retry (rtr).
				(void*)SRDO,            								  	   //Object passed to receive function.
				(i == 1) ? CO_SRDO_receive_inverted : CO_SRDO_receive_normal); //Function processing received message.

        /* wrong SRDO initialization */
        if (err != CO_ERROR_NO)
        {
            SRDO->valid = CO_SRDO_INVALID;
            CO_FLAG_CLEAR(SRDO->CANrxNew[i]);
        }
    }
}

/**
 * Find SRDO object given mapping.
 */
static ODR_t CO_SRDOfindMap(OD_t *OD,
			                uint32_t map,
						    uint8_t R_T,
						    uint8_t **ppData,
						    uint8_t *pLength,
						    uint8_t *pSendIfCOSFlags,
						    uint8_t *pIsMultibyteVar)
{
    /* variables */
    OD_entry_t *entry;
    OD_IO_t subEntry;
    ODR_t oderr;

    /* decode index, subIndex and data length from map */
    uint16_t index = (uint16_t)(map>>16);
    uint8_t subIndex = (uint8_t)(map>>8);
    uint8_t dataLen = (uint8_t)map;

    /* data length must be 8 bytes aligned. TODO: check if really needed */
    if (dataLen & 0x07)
    {
    	/* object cannot be mapped to the PDO */
    	return ODR_NO_MAP;
    }

    /* get dataLen in bytes */
    dataLen >>= 3;

    /* update overall length of SRDO objects */
    *pLength += dataLen;

    /* total PDO length can not be more than 8 bytes */
    if (*pLength > 8)
    {
    	/* the number and length of the objects to be mapped would exceed PDO length */
    	return ODR_MAP_LEN;
    }

    /*
     * Check if there is a reference to dummy entries. Dummy mapping is needed to
     * fill gaps in receive-SRDO mapping.
     */
    if (index <= 7 && subIndex == 0)
    {
    	/* variables */
        static uint32_t dummyTX = 0;
        static uint32_t dummyRX;
        uint8_t dummySize = 4;

        if (index < 2)
        {
        	dummySize = 0;
        }
        else if (index == 2 || index == 5)
        {
        	dummySize = 1;
        }
        else if (index == 3 || index == 6)
        {
        	dummySize = 2;
        }

        /* check if variable big enough for map */
        if (dummySize < dataLen)
        {
        	/* object cannot be mapped to the PDO */
        	return ODR_NO_MAP;
        }

        /* data and ODE pointer */
        if (R_T == 0)
        {
        	*ppData = (uint8_t*)&dummyRX;
        }
        else
        {
        	*ppData = (uint8_t*)&dummyTX;
        }

        return ODR_OK;
    }

    /* get object dictionary entry and sub entry */
    entry = OD_find(OD, index);
    oderr = OD_getSub(entry, subIndex, &subEntry, 0);

    /*
     * Check if object is valid. Here both Arrays and Records are
     * supported and they have the following behavior:
     *   - Array: contiguous indexes, if subIndex > subEntriesCount
     *   		  an error in OD_getSub() is returned.
     *   - Record: indexes don't need to be contiguous. To search for
     *   		   a specific subEntry, each element of the record is
     *   		   compared to the given subIndex, and only if there is
     *   		   a match the search stops. This structure is less
     *   		   efficient in terms of CPU consumption but, on average,
     *   		   requires less memory, since dummy or empty subEntries
     *   		   can be avoided.
     */
    if ((entry == NULL) || (oderr != ODR_OK))
    {
    	/* object does not exist in the object dictionary */
    	return ODR_IDX_NOT_EXIST;
    }

    /* check if object is SRDO mappable */
    if (!OD_mappable(&subEntry.stream))
   	{
    	return ODR_NO_MAP;
    }

    /* check if size of variable big enough for map */
    if (subEntry.stream.dataLength < dataLen)
    {
    	/* object cannot be mapped to the PDO */
    	return ODR_NO_MAP;
    }

    /* mark multi-byte variable */
    *pIsMultibyteVar = (subEntry.stream.attribute & ODA_MB) ? 1 : 0;

    /* get pointer to OD data */
    *ppData = (uint8_t*)subEntry.stream.dataOrig;

#ifdef CO_BIG_ENDIAN
    /* skip unused MSB bytes */
    if (*pIsMultibyteVar)
    {
        *ppData += objectLen - dataLen;
    }
#endif

    /* setup change of state flags */
    if (subEntry.stream.attribute & ODA_TPDO)
    {
        for (int16_t i = *pLength-dataLen; i < *pLength; i++)
        {
            *pSendIfCOSFlags |= 1 << i;
        }
    }

    return 0;
}

/**
 * Configure mapping part for SRDO.
 */
static ODR_t CO_SRDOconfigMap(CO_SRDO_t* SRDO, uint8_t noOfMappedObjects)
{
	/* variables */
    uint8_t lengths[2] = {0};
    ODR_t ret = 0;

    /* get pointer to first map */
    const uint32_t* pMap = &SRDO->SRDOMapPar.mappedObjects[0];

    /* configure all mapped objects */
    for (uint8_t i = 0; i < noOfMappedObjects; i++)
    {
    	/* variables */
        uint8_t* pData;
        uint8_t dummy = 0;
        uint8_t MBvar;

        /*
         * Length in bytes of total data mapped. Distinguish between normal and inverted
         * objects (according to array position). Note that normal objects are in even
         * positions, while inverted ones are in odd positions.
         */
        uint8_t* length = &lengths[i%2];

        /* get map pointer, either to normal or of inverted data */
        uint8_t** mapPointer = SRDO->mapPointer[i%2];

        /* save previous length */
        uint8_t prevLength = *length;

        /* get new map pointer */
        uint32_t map = *(pMap++);

        /* try to get pointed object and update pData */
        ret = CO_SRDOfindMap(SRDO->OD,
        					 map,
							 SRDO->SRDOCommPar.informationDirection == CO_SRDO_TX,
							 &pData,
							 length,
							 &dummy,
							 &MBvar);

        /* check if object found */
        if (ret != ODR_OK)
        {
            *length = 0;
            CO_errorReport(SRDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, map);
            break;
        }

        /*
         * Update mapPointer with new object information. Note that only
         * [prevLength, length] indexes are updated.
         */
#ifdef CO_BIG_ENDIAN
        if (MBvar)
        {
            for (int16_t j = *length-1; j >= prevLength; j--)
            {
            	mapPointer[j] = pData++;
            }
        }
        else
        {
            for (int16_t j = prevLength; j < *length; j++)
            {
            	mapPointer[j] = pData++;
            }
        }
#else
        for (int16_t j = prevLength; j < *length; j++)
        {
            mapPointer[j] = pData++;
        }
#endif
    }

    /* normal and inverted SRDOs need to have the same length */
    if (lengths[0] == lengths[1])
    {
    	SRDO->dataLength = lengths[0];
    }
    else
    {
        SRDO->dataLength = 0;
        CO_errorReport(SRDO->em, CO_EM_PDO_WRONG_MAPPING, CO_EMC_PROTOCOL_ERROR, 0);
        ret = ODR_MAP_LEN;
    }

    return ret;
}

/**
 * Calculate SRDO CRC.
 */
static uint16_t CO_SRDOcalcCrc(const CO_SRDO_t *SRDO)
{
	/* variables */
    uint16_t result = 0x0000;
    uint8_t buffer[4];

    /* communication and mapping record */
    const CO_SRDOCommPar_t *com = &SRDO->SRDOCommPar;
    const CO_SRDOMapPar_t *map = &SRDO->SRDOMapPar;

    /* information direction component */
    result = crc16_ccitt(&com->informationDirection, 1, result);

    /* refresh time or sct component */
    memcopy(&buffer[0], (uint8 *)&com->refreshTimeOrSCT, sizeof(com->refreshTimeOrSCT));
    result = crc16_ccitt(&buffer[0], 2, result);

    /* srvt component */
    result = crc16_ccitt(&com->SRVT, 1, result);

    /* normal cob id component */
    memcopy(&buffer[0], (uint8 *)&com->COB_ID_1, sizeof(com->COB_ID_1));
    result = crc16_ccitt(&buffer[0], 4, result);

    /* inverted cob id component */
    memcopy(&buffer[0], (uint8 *)&com->COB_ID_2, sizeof(com->COB_ID_2));
    result = crc16_ccitt(&buffer[0], 4, result);

    /* number of mapped objects component */
    result = crc16_ccitt(&map->numberOfMappedObjects, 1, result);

    /* single mapped objects component */
    for (uint8_t i = 0; i < map->numberOfMappedObjects; i++)
    {
    	/* skip number of mapped objects (sub entry zero) */
        uint8_t subindex = i + 1;

        /* process object map */
        result = crc16_ccitt(&subindex, 1, result);
        memcopy(&buffer[0], (uint8 *)&map->mappedObjects[i], sizeof(map->mappedObjects[0]));

        /* update common result */
        result = crc16_ccitt(&buffer[0], 4, result);
    }

    return result;
}


/***********************************************************************************************************************
 * Public functions
 **********************************************************************************************************************/
/**
 * Initialize SRDOGuard object.Function must be called in the
 * communication reset section.
 */
CO_ReturnError_t CO_SRDOGuard_init(CO_SRDOGuard_t *SRDOGuard,
								   OD_t *OD,
								   CO_NMT_internalState_t *operatingState,
								   uint8_t  configurationValid,
								   uint16_t idx_SRDOvalid,
								   uint16_t idx_SRDOcrc)
{
	/* error code */
	CO_ReturnError_t ret = CO_ERROR_NO;

    /* verify arguments */
    if( (SRDOGuard==NULL) || (OD==NULL) || (operatingState==NULL) )
    {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }
    else
    {
    	/* initialize SRDOGuard */
		SRDOGuard->operatingState = operatingState;
		SRDOGuard->operatingStatePrev = CO_NMT_INITIALIZING;
		SRDOGuard->configurationValid = configurationValid;
		SRDOGuard->checkCRC = (configurationValid == CO_SRDO_CRC_VALID_CHECH);

		/* configure extensions (write only) for valid entry 0x13FE */
		CoSrdoOdExtConfig(OD, idx_SRDOvalid, NULL, CoSrdoGuardValidWriteCb, (void*)SRDOGuard, 0, 0);

		/* configure extensions (write only) for CRC entry 0x13FF */
		CoSrdoOdExtConfig(OD, idx_SRDOcrc, NULL, CoSrdoGuardCrcWriteCb, (void*)SRDOGuard, 0, 0);
    }

    /* return error code */
    return ret;
}

/**
 * Process operation and valid state changes.
 */
uint8_t CO_SRDOGuard_process(CO_SRDOGuard_t *SRDOGuard)
{
	/* return value */
    uint8_t result = 0;

    /* get current NMT state */
    CO_NMT_internalState_t operatingState = *(SRDOGuard->operatingState);

    /* check if there is a NMT state change */
    if (operatingState != SRDOGuard->operatingStatePrev)
    {
    	/* update NMT state */
        SRDOGuard->operatingStatePrev = operatingState;

        /* set command if just entered operational */
        if (operatingState == CO_NMT_OPERATIONAL)
        {
            result |= 1 << 0;
        }
    }

    /* force CRC verification if requested */
    if (SRDOGuard->checkCRC)
    {
    	/*
    	 * Force mapping and communication record (re)configuration.
    	 * This action is performed in CO_SRDO_process only when
    	 * requested by result.
    	 */
        result |= 1 << 1;

        /*
         * Don't check immediately the CRC because an error would arise
         * and the reconfiguration would be stopped immediately. It will
         * be re enabled later in the the CO_SRDO_process function.
         */
        SRDOGuard->checkCRC = 0;
    }

    return result;
}

/**
 * Initialize SRDO callback function. Function initializes optional callback function,
 * which should immediately start processing of CO_SRDO_process() function.
 * Callback is called after SRDO message is received from the CAN bus.
 */
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
void CO_SRDO_initCallbackPre(CO_SRDO_t *SRDO,
							 void *object,
							 void (*pFunctSignalPre)(void *object))
{
    if (SRDO != NULL)
    {
        SRDO->functSignalObjectPre = object;
        SRDO->pFunctSignalPre = pFunctSignalPre;
    }
}
#endif

/**
 * Initialize SRDO callback function. Function initializes optional callback function,
 * that is called when SRDO enters a safe state. This happens when a timeout is reached
 * or the data is inconsistent. The safe state itself is not further defined.
 * One measure, for example, would be to go back to the pre-operational state Callback
 * is called from CO_SRDO_process().
 */
void CO_SRDO_initCallbackEnterSafeState(CO_SRDO_t *SRDO,
										void *object,
										void (*pFunctSignalSafe)(void *object))
{
    if (SRDO != NULL)
    {
        SRDO->functSignalObjectSafe = object;
        SRDO->pFunctSignalSafe = pFunctSignalSafe;
    }
}

/**
 * Initialize SRDO object, this function must be called in the communication reset section.
 */
CO_ReturnError_t CO_SRDO_init(CO_SRDO_t *SRDO,
							  CO_SRDOGuard_t *SRDOGuard,
							  CO_EM_t *em,
							  OD_t *OD,
							  uint8_t nodeId,
							  uint16_t defaultCOB_ID,
							  OD_entry_t *SRDOCommPar,
							  OD_entry_t *SRDOMapPar,
							  uint16_t checksum,
							  uint16_t idx_SRDOCommPar,
							  uint16_t idx_SRDOMapPar,
							  CO_CANmodule_t *CANdevRx,
							  uint16_t CANdevRxIdxNormal,
							  uint16_t CANdevRxIdxInverted,
							  CO_CANmodule_t *CANdevTx,
							  uint16_t CANdevTxIdxNormal,
							  uint16_t CANdevTxIdxInverted)
{
	/* error codes */
	CO_ReturnError_t ret = CO_ERROR_NO;
	ODR_t err = ODR_OK;

    /* verify arguments */
    if ((SRDO == NULL) 		  || (SRDOGuard == NULL)  || \
    	(em == NULL) 		  || (OD == NULL) 		  || \
    	(SRDOCommPar == NULL) || (SRDOMapPar == NULL) || \
		(CANdevRx == NULL) 	  || (CANdevTx == NULL))
    {
    	ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }
    else
    {
    	/* populate SRDO */
		SRDO->SRDOGuard = SRDOGuard;
		SRDO->em = em;
		SRDO->OD = OD;
		SRDO->checksum = checksum;
		SRDO->CANdevRx = CANdevRx;
		SRDO->CANdevRxIdx[0] = CANdevRxIdxNormal;
		SRDO->CANdevRxIdx[1] = CANdevRxIdxInverted;
		SRDO->CANdevTx = CANdevTx;
		SRDO->CANdevTxIdx[0] = CANdevTxIdxNormal;
		SRDO->CANdevTxIdx[1] = CANdevTxIdxInverted;
		SRDO->nodeId = nodeId;
		SRDO->defaultCOB_ID[0] = defaultCOB_ID;
		SRDO->defaultCOB_ID[1] = defaultCOB_ID + 1;
		SRDO->valid = CO_SRDO_INVALID;
		SRDO->toogle = 0; //NOTE: important to force zero initialization
		SRDO->timer = 0;  //NOTE: important to force zero initialization
		SRDO->pFunctSignalSafe = NULL;
		SRDO->functSignalObjectSafe = NULL;
#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_CALLBACK_PRE
		SRDO->pFunctSignalPre = NULL;
		SRDO->functSignalObjectPre = NULL;
#endif

		/* initialize SRDO communication record */
		err |= CoSrdoOdCommInit(SRDO, SRDOCommPar);

		/* initialize SRDO mapping record  */
		err |= CoSrdoOdMapInit(SRDO, SRDOMapPar);

		/* expose only first error */
		if ((ret == CO_ERROR_NO) && (err != ODR_OK))
		{
			ret = CO_ERROR_OD_PARAMETERS;
		}

		/* configure extensions for communication related objects */
		CoSrdoOdExtConfig(OD, idx_SRDOCommPar, CoSrdoCommReadCb, CoSrdoCommWriteCb, (void *)SRDO, 0, 0);

		/* configure extensions for mapping related objects */
		CoSrdoOdExtConfig(OD, idx_SRDOMapPar, CoSrdoMapReadCb, CoSrdoMapWriteCb, (void *)SRDO, 0, 0);

		/*
		 * TODO: add some general, but meaningful callback to enter safe state when
		 * a timeout is reached or the data is inconsistent. This can be don using
		 * the CO_SRDO_initCallbackEnterSafeState() function, or by directly assigning
		 * the address of an existent function to SRDO->pFunctSignalSafe.
		 */
    }

    return ret;
}

/**
 * Process transmitting/receiving SRDO messages. This function verifies the checksum
 * on demand and configures the SRDO operation state change to operational.
 */
void CO_SRDO_process(CO_SRDO_t *SRDO,
					 uint8_t commands,
					 uint32_t timeDifference_us,
					 uint32_t *timerNext_us)
{
	/* may be unused */
    (void)timerNext_us;

    /*
     * CRC check request: since runtime modification of SRDOs is not allowed,
     * this section (up to the "runtime behavior comment) can be moved in the
     * CO_SRDO_init() function in order to perform this check only once during
     * initialization. TODO: keep anyway if in future runtime is needed.
     */
    if (commands & (1 << 1))
    {
    	/* calculate CRC from SRDO */
        uint16_t crcValue = CO_SRDOcalcCrc(SRDO);

        /* check if CRC matches */
        if (SRDO->checksum != crcValue)
        {
        	SRDO->SRDOGuard->configurationValid = 0;
        }
    }

    /*
     * Initial mapping and communication record configuration. This logic is
     * performed only when the node enters NMT OPERATIONAL from a different
     * NMT status, normally only once during initialization.
     */
    if ((commands & (1 << 0))
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CRC_CHECK
    	/* CRC check enabled */
    	&& (SRDO->SRDOGuard->configurationValid == CO_SRDO_CRC_VALID_CHECH)
#else
    	/* CRC check disabled */
    	&& (SRDO->SRDOGuard->configurationValid != ODR_INVALID_VALUE)
#endif
		)
    {
    	/* configure SRDO mapping */
    	ODR_t err = CO_SRDOconfigMap(SRDO, SRDO->SRDOMapPar.numberOfMappedObjects);

    	/* check SRDO mapping */
        if (err == ODR_OK)
        {
        	/* set communication record, valid marker in internal structure */
            CO_SRDOconfigCom(SRDO, SRDO->SRDOCommPar.COB_ID_1, SRDO->SRDOCommPar.COB_ID_2);
        }
        else
        {
        	/*
        	 * Mark SRDO as invalid, this avoids the SRDO to be processed
        	 * in the following logic.
        	 */
            SRDO->valid = CO_SRDO_INVALID;
        }
    }

    /* runtime behavior */
    if ((SRDO->valid) && (*SRDO->SRDOGuard->operatingState == CO_NMT_OPERATIONAL))
    {
    	/* get timer (clamp to zero to avoid overflow) */
        if (SRDO->timer > timeDifference_us)
        {
        	SRDO->timer = SRDO->timer - timeDifference_us;
        }
        else
        {
        	SRDO->timer = 0;
        }

        /* TX SRDO */
        if (SRDO->valid == CO_SRDO_TX)
        {
        	/*
        	 * Time elapsed: the normal and inverted SRDO messages are sent in two different
        	 * cycles. This forces the SRDO refresh time to be greater than twice the cycle time.
        	 * To overcome this limitation remove the toggle logic and send (buffer) the two
        	 * messages in the same cycle, before the normal and than the inverted.
        	 * Note that the latter approach has the problem that the two CAN messages are buffered
        	 * in consecutive positions. This means that they will be sent one after the other and
        	 * the time between them may be too short for the receiver.
        	 */
            if (SRDO->timer == 0)
            {
            	/* inverted SRDO */
                if (SRDO->toogle)
                {
                	/* add message to CANtxBuff[1], ready queue of inverted SRDO */
                    CO_CANsend(SRDO->CANdevTx, SRDO->CANtxBuff[1]);

                    /* update timer */
                    SRDO->timer = SRDO->SRDOCommPar.refreshTimeOrSCT * 1000U - CO_CONFIG_SRDO_MINIMUM_DELAY;
                }
                /* normal SRDO */
                else
                {
                    /* object TX extension specific behavior */
#if (CO_CONFIG_SRDO) & CO_CONFIG_TSRDO_CALLS_EXTENSION
                    /*
                     * For each mapped OD, check mapping to see if an OD extension is
                     * available, and call it if it is.
                     */
                    const uint32_t* pMap = &SRDO->SRDOMapPar.mappedObjects[0];

                    /*
                     * Loop over objects and check for both TX and RX extensions.
                     * This is needed due to the different logic between consecutive
                     * stack releases, keep both.
                     */
                    for (int16_t i = 0; i < SRDO->SRDOMapPar.numberOfMappedObjects; i++)
                    {
                    	/* decode index, subIndex and dataLen from map */
                        uint32_t map = *(pMap++);
                        uint16_t index = (uint16_t)(map>>16);
                        uint8_t subIndex = (uint8_t)(map>>8);

                        /* get entry and subEntry from OD */
                        OD_entry_t *entry = OD_find(SRDO->OD, index);
                        OD_IO_t subEntry;
                        ODR_t err = OD_getSub(entry, subIndex, &subEntry, 0);

                        /* check if entry and subEntry exist */
                        if ((entry != NULL) && (err == ODR_OK))
                        {
                         	/* get extension */
                           	OD_extension_t *ext = entry->extension;

                           	/* check if object has extension */
                           	if (ext != NULL)
                           	{
								/* read callback */
								if (ext->read != NULL)
								{
									OD_size_t tmp;
									ext->read(&subEntry.stream, ext->object, subEntry.stream.dataLength, &tmp);
								}
								/* write callback */
								if (ext->write != NULL)
								{
									OD_size_t tmp;
									ext->write(&subEntry.stream, (const void *)ext->object, subEntry.stream.dataLength, &tmp);
								}
                           	}
                        }
                    }
#endif
                    /* set data to CANtxBuff */
                    uint8_t* pSRDOdataByte_normal = &SRDO->CANtxBuff[0]->data[0];
                    uint8_t** ppODdataByte_normal = &SRDO->mapPointer[0][0];
                    uint8_t* pSRDOdataByte_inverted = &SRDO->CANtxBuff[1]->data[0];
                    uint8_t** ppODdataByte_inverted = &SRDO->mapPointer[1][0];
                    bool_t data_ok = 1;

                    /*
                     * NOTE: there are two possible setups for the SRDO pointed objects:
                     *   1) Specify two different objects for normal and inverted SRDO data
                     *      in the EDS file. In this case the two objects need to be updated
                     *      both and consistently (inverted) by the application.
                     *   2) Specify twice the same object for normal and inverted SRDO data
                     *      in the EDS file. In this case only the normal object needs to be
                     *      updated by the application since it is the only one existing in the
                     *      object dictionary. This, maybe, reduces the safety but increases
                     *      the performances, since only half of the inter-task-communication
                     *      traffic from application to stack is required.
                     */
#if (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_CHECK_TX
                    /* optional TX SRDO consistency check */
					for (uint16_t i = 0; i < SRDO->dataLength; i++)
					{
						/* invert normal data */
						uint8_t invert = ~(*ppODdataByte_normal[i]);

						/* check if normal and inverted inverted match */
						if ((ppODdataByte_normal[i] != ppODdataByte_inverted[i]) && \
							(*ppODdataByte_inverted[i] != invert))
						{
							data_ok = 0;
							break;
						}
					}
#endif
                    /* check if data is correct before transmitting */
                    if (data_ok)
                    {
                        /* copy data from Object dictionary to CANtxBuff */
                        for (uint16_t i = 0; i < SRDO->dataLength; i++)
                        {
                        	/* copy normal data to SRDO */
                            pSRDOdataByte_normal[i] = *(ppODdataByte_normal[i]);
                            /* copy inverted data to SRDO */
                            pSRDOdataByte_inverted[i] = (ppODdataByte_normal[i] == ppODdataByte_inverted[i]) ? \
                            		~(*ppODdataByte_normal[i]) : *ppODdataByte_inverted[i];
                        }

                        /* add message to CANtxBuff[0], ready queue of normal SRDO */
                        CO_CANsend(SRDO->CANdevTx, SRDO->CANtxBuff[0]);

                        /*
                         * Timer offset: it is configured even if by default is zero.
                         * It makes no sense to have a different timer offset because
                         * the can messages are sent all together at the end of the
                         * TaskCoAuto cycle, meaning we have no control on the delay
                         * between normal and inverted SRDO message.
                         */
                        SRDO->timer = CO_CONFIG_SRDO_MINIMUM_DELAY;
                    }
                    else
                    {
                    	/*
                    	 * By setting toggle to one, it will be later inverted, meaning
                    	 * that no inverted CAN message will be transmitted and the
                    	 * next cycle the complete computation will be repeated.
                    	 */
                        SRDO->toogle = 1;

                        /* save state */
                        if (SRDO->pFunctSignalSafe != NULL)
                        {
                        	/* signal safe state (user defined) */
                            SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                        }
                    }
                }

                /*
                 * Invert toggle, this means that:
                 *   - toggle = 0 && data_ok --> send normal && toggle = 1
                 *   - toggle = 0 && !data_ok --> retry next cycle && toggle = 0
                 *   - toggle = 1 --> send inverted message && toggle = 0
                 */
                SRDO->toogle = !SRDO->toogle;
            }

#if (CO_CONFIG_SRDO) & CO_CONFIG_FLAG_TIMERNEXT
            if (timerNext_us != NULL)
            {
                if (*timerNext_us > SRDO->timer)
                {
                	/* schedule for next message timer */
                    *timerNext_us = SRDO->timer;
                }
            }
#endif
        }
        /* RX SRDO */
        else if (SRDO->valid == CO_SRDO_RX)
        {
        	/*
        	 * The SRDO receiver expects normal and inverted message to arrive with a
        	 * minimum (non zero) delay between them. In order to be sure that both
        	 * messages can be received in time, the same toggle logic used for the
        	 * transmit SRDO is used also here. This means that only SRDO with a
        	 * refresh time greater than twice the cycle time can be received and
        	 * processed properly.
        	 */
            if (SRDO->CANrxNew[SRDO->toogle] != NULL)
            {
            	/*
            	 * Both normal and inverted messages have been received, process
            	 * them, update object dictionary and reset flags.
            	 */
                if (SRDO->toogle)
                {
                	/* variables used to process CANRxBuff */
                    uint8_t* pSRDOdataByte_normal = &SRDO->CANrxData[0][0];
                    uint8_t** ppODdataByte_normal = &SRDO->mapPointer[0][0];
                    uint8_t* pSRDOdataByte_inverted = &SRDO->CANrxData[1][0];
                    uint8_t** ppODdataByte_inverted = &SRDO->mapPointer[1][0];
                    bool_t data_ok = 1;

                    /* check data consistency (mandatory for RX) */
                    for (uint16_t i = 0; i < SRDO->dataLength; i++)
                    {
                    	/* invert normal data */
                        uint8_t invert = ~pSRDOdataByte_normal[i];

                        /* check if normal and inverted match */
                        if (pSRDOdataByte_inverted[i] != invert)
                        {
                            data_ok = 0;
                            break;
                        }
                    }

                    /* check if data is fine */
                    if (data_ok)
                    {
                        /* copy data from CANRxBuff to Object dictionary */
                        for (uint16_t i = 0; i < SRDO->dataLength; i++)
                        {
                        	/* copy normal data to object dictionary */
                            *ppODdataByte_normal[i] = pSRDOdataByte_normal[i];
                            /* copy inverted data to object dictionary (only if different objects) */
                            if (ppODdataByte_normal[i] != ppODdataByte_inverted[i])
                            {
                            	*ppODdataByte_inverted[i] = pSRDOdataByte_inverted[i];
                            }
                        }

                        /* clear flags */
                        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);

                        /* object TX extension specific behavior */
#if (CO_CONFIG_SRDO) & CO_CONFIG_RSRDO_CALLS_EXTENSION
                        /*
                         * For each mapped OD, check mapping to see if an OD extension is
                         * available, and call it if it is.
                         */
                        const uint32_t* pMap = &SRDO->SRDOMapPar.mappedObjects[0];

                        /*
                         * Loop over objects and check for both TX and RX extensions.
                         * This is needed due to the different logic between consecutive
                         * stack releases, keep both.
                         */
                        for (int16_t i = SRDO->SRDOMapPar.numberOfMappedObjects; i > 0; i--)
                        {
                        	/* decode index, subIndex and dataLen from map */
                        	uint32_t map = *(pMap++);
                            uint16_t index = (uint16_t)(map>>16);
                            uint8_t subIndex = (uint8_t)(map>>8);

                            /* get entry and subEntry */
                            OD_entry_t *entry = OD_find(SRDO->OD, index);
                            OD_IO_t subEntry;
                            ODR_t suberr = OD_getSub(entry, subIndex, &subEntry, 0);

                            /* check entry and subEntry */
                            if ((entry != NULL) && (suberr == ODR_OK))
                            {
                               	/* get extension */
                               	OD_extension_t *ext = entry->extension;

                               	/* check if object has extension */
                               	if (ext != NULL)
                               	{
                               		/* read callback */
									if (ext->read != NULL)
									{
										OD_size_t tmp;
										ext->read(&subEntry.stream, ext->object, subEntry.stream.dataLength, &tmp);
									}
									/* write callback */
									if (ext->write != NULL)
									{
										OD_size_t tmp;
										ext->write(&subEntry.stream, (const void *)ext->object, subEntry.stream.dataLength, &tmp);
									}
                               	}
                            }
                        }
#endif
                    }
                    else
                    {
                    	/* clear flags */
                        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);

                        /* safe state */
                        if (SRDO->pFunctSignalSafe != NULL)
                        {
                            SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                        }
                    }

                    /* update timer */
                    SRDO->timer = SRDO->SRDOCommPar.refreshTimeOrSCT * 1000U;
                }
                else
                {
                	/*
                	 * First (normal) SRDO received, update timer with SRVT value (maximum delay
                	 * between normal and inverted message) and wait for inverted message.
                	 */
                    SRDO->timer = SRDO->SRDOCommPar.SRVT * 1000U;
                }

                /* invert toggle */
                SRDO->toogle = !SRDO->toogle;
            }

            /*
             * If message not received in time, reset toggle and timer and
             * call safe state function which is application dependent and
             * needs to be implemented (or at least defined) by the user.
             * At the moment the default behavior is not to update the
             * OD entries mapped to the SRDO and retry next cycle.
             */
            if (SRDO->timer == 0)
            {
            	/* reset timer and toggle (retry next cycle) */
                SRDO->toogle = 0;
                SRDO->timer = SRDO->SRDOCommPar.SRVT * 1000U;

                /* clear flags */
                CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
                CO_FLAG_CLEAR(SRDO->CANrxNew[1]);

                /* safe state */
                if (SRDO->pFunctSignalSafe != NULL)
                {
                    SRDO->pFunctSignalSafe(SRDO->functSignalObjectSafe);
                }
            }
        }
    }
    else
    {
    	/*
    	 * The received SRDO is neither a transmit nor a receive SRDO,
    	 * meaning that it is invalid and cannot be processed.
    	 */
        SRDO->valid = CO_SRDO_INVALID;

        /* clear flags */
        CO_FLAG_CLEAR(SRDO->CANrxNew[0]);
        CO_FLAG_CLEAR(SRDO->CANrxNew[1]);
    }
}

#endif /* (CO_CONFIG_SRDO) & CO_CONFIG_SRDO_ENABLE */
