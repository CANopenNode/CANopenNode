/*
 * CANopen data storage object for storing data into block device (eeprom)
 *
 * @file        CO_storageEeprom.c
 * @author      Janez Paternoster
 * @copyright   2021 Janez Paternoster
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

#include "storage/CO_storageEeprom.h"
#include "storage/CO_eeprom.h"
#include "301/crc16-ccitt.h"

#if (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE

/*
 * Function for writing data on "Store parameters" command - OD object 1010
 *
 * For more information see file CO_storage.h, CO_storage_entry_t.
 */
static ODR_t storeEeprom(CO_storage_entry_t *entry, CO_CANmodule_t *CANmodule) {
    bool_t writeOk;

    /* save data to the eeprom */
    CO_LOCK_OD(CANmodule);
    writeOk = CO_eeprom_writeBlock(entry->storageModule, entry->addr,
                                   entry->eepromAddr, entry->len);
    entry->crc = crc16_ccitt(entry->addr, entry->len, 0);
    CO_UNLOCK_OD(CANmodule);

    /* Verify, if data in eeprom are equal */
    uint16_t crc_read = CO_eeprom_getCrcBlock(entry->storageModule,
                                              entry->eepromAddr, entry->len);
    if (entry->crc != crc_read || !writeOk) {
        return ODR_HW;
    }

    /* Write signature (see CO_storageEeprom_init() for info) */
    uint16_t signatureOfEntry = (uint16_t)entry->len;
    uint32_t signature = (((uint32_t)entry->crc) << 16) | signatureOfEntry;
    writeOk = CO_eeprom_writeBlock(entry->storageModule,
                                   (uint8_t *)&signature,
                                   entry->eepromAddrSignature,
                                   sizeof(signature));

    /* verify signature and write */
    uint32_t signatureRead;
    CO_eeprom_readBlock(entry->storageModule,
                        (uint8_t *)&signatureRead,
                        entry->eepromAddrSignature,
                        sizeof(signatureRead));
    if(signature != signatureRead || !writeOk) {
        return ODR_HW;
    }

    return ODR_OK;
}


/*
 * Function for restoring data on "Restore default parameters" command - OD 1011
 *
 * For more information see file CO_storage.h, CO_storage_entry_t.
 */
static ODR_t restoreEeprom(CO_storage_entry_t *entry,
                           CO_CANmodule_t *CANmodule)
{
    (void) CANmodule;
    bool_t writeOk;

    /* Write empty signature */
    uint32_t signature = 0xFFFFFFFF;
    writeOk = CO_eeprom_writeBlock(entry->storageModule,
                                   (uint8_t *)&signature,
                                   entry->eepromAddrSignature,
                                   sizeof(signature));

    /* verify signature and protection */
    uint32_t signatureRead;
    CO_eeprom_readBlock(entry->storageModule,
                        (uint8_t *)&signatureRead,
                        entry->eepromAddrSignature,
                        sizeof(signatureRead));
    if(signature != signatureRead || !writeOk) {
        return ODR_HW;
    }

    return ODR_OK;
}


/******************************************************************************/
CO_ReturnError_t CO_storageEeprom_init(CO_storage_t *storage,
                                       CO_CANmodule_t *CANmodule,
                                       void *storageModule,
                                       OD_entry_t *OD_1010_StoreParameters,
                                       OD_entry_t *OD_1011_RestoreDefaultParam,
                                       CO_storage_entry_t *entries,
                                       uint8_t entriesCount,
                                       uint32_t *storageInitError)
{
    CO_ReturnError_t ret;
    bool_t eepromOvf = false;

    /* verify arguments */
    if (storage == NULL || entries == NULL || entriesCount == 0
        || storageInitError == NULL
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    storage->enabled = false;

    /* Initialize storage hardware */
    if (!CO_eeprom_init(storageModule)) {
        *storageInitError = 0xFFFFFFFF;
        return CO_ERROR_DATA_CORRUPT;
    }

    /* initialize storage and OD extensions */
    ret = CO_storage_init(storage,
                          CANmodule,
                          OD_1010_StoreParameters,
                          OD_1011_RestoreDefaultParam,
                          storeEeprom,
                          restoreEeprom,
                          entries,
                          entriesCount);
    if (ret != CO_ERROR_NO) {
        return ret;
    }

    /* Read entry signatures from the eeprom */
    uint32_t signatures[entriesCount];
    size_t signaturesAddress = CO_eeprom_getAddr(storageModule,
                                                 false,
                                                 sizeof(signatures),
                                                 &eepromOvf);
    CO_eeprom_readBlock(storageModule,
                        (uint8_t *)signatures,
                        signaturesAddress,
                        sizeof(signatures));

    /* initialize entries */
    *storageInitError = 0;
    for (uint8_t i = 0; i < entriesCount; i++) {
        CO_storage_entry_t *entry = &entries[i];
        bool_t isAuto = (entry->attr & CO_storage_auto) != 0;

        /* verify arguments */
        if (entry->addr == NULL || entry->len == 0 || entry->subIndexOD < 2) {
            *storageInitError = i;
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }

        /* calculate addresses inside eeprom */
        entry->eepromAddrSignature = signaturesAddress + sizeof(uint32_t) * i;
        entry->eepromAddr = CO_eeprom_getAddr(storageModule,
                                              isAuto,
                                              entry->len,
                                              &eepromOvf);
        entry->offset = 0;

        /* verify if eeprom is too small */
        if (eepromOvf) {
            *storageInitError = i;
            return CO_ERROR_OUT_OF_MEMORY;
        }

        /* 32bit signature (which was stored in eeprom) is combined from
         * 16bit signature of the entry and 16bit CRC checksum of the data
         * block. 16bit signature of the entry is entry->len. */
        uint32_t signature = signatures[i];
        uint16_t signatureInEeprom = (uint16_t)signature;
        entry->crc = (uint16_t)(signature >> 16);
        uint16_t signatureOfEntry = (uint16_t)entry->len;

        /* Verify two signatures */
        bool_t dataCorrupt = false;
        if (signatureInEeprom != signatureOfEntry) {
            dataCorrupt = true;
        }
        else {
            /* Read data into storage location */
            CO_eeprom_readBlock(entry->storageModule, entry->addr,
                                entry->eepromAddr, entry->len);

            /* Verify CRC, except for auto storage variables */
            if (!isAuto) {
                uint16_t crc = crc16_ccitt(entry->addr, entry->len, 0);
                if (crc != entry->crc) {
                    dataCorrupt = true;
                }
            }
        }

        /* additional info in case of error */
        if (dataCorrupt) {
            uint32_t errorBit = entry->subIndexOD;
            if (errorBit > 31) errorBit = 31;
            *storageInitError |= ((uint32_t) 1) << errorBit;
            ret = CO_ERROR_DATA_CORRUPT;
        }
    } /* for (entries) */

    storage->enabled = true;
    return ret;
}


/******************************************************************************/
void CO_storageEeprom_auto_process(CO_storage_t *storage, bool_t saveAll) {
    /* verify arguments */
    if (storage == NULL || !storage->enabled) {
        return;
    }

    /* loop through entries */
    for (uint8_t i = 0; i < storage->entriesCount; i++) {
        CO_storage_entry_t *entry = &storage->entries[i];

        if ((entry->attr & CO_storage_auto) == 0)
            continue;

        if (saveAll) {
            /* update all bytes */
            for (size_t i = 0; i < entry->len; ) {
                uint8_t dataByteToUpdate = ((uint8_t *)(entry->addr))[i];
                size_t eepromAddr = entry->eepromAddr + i;
                if (CO_eeprom_updateByte(entry->storageModule,
                                         dataByteToUpdate,
                                         eepromAddr)
                ) {
                    i++;
                }
            }
        }
        else {
            /* update one data byte and if successful increment to next */
            uint8_t dataByteToUpdate = ((uint8_t*)(entry->addr))[entry->offset];
            size_t eepromAddr = entry->eepromAddr + entry->offset;
            if (CO_eeprom_updateByte(entry->storageModule, dataByteToUpdate,
                                     eepromAddr)
            ) {
                if (++entry->offset >= entry->len) {
                    entry->offset = 0;
                }
            }
        }
    }
}

#endif /* (CO_CONFIG_STORAGE) & CO_CONFIG_STORAGE_ENABLE */
