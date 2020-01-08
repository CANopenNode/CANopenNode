/*
 * Eeprom object for Microchip PIC32MX microcontroller.
 *
 * @file        eeprom.c
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
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


#include "CO_driver.h"
#include "CO_SDO.h"
#include "CO_Emergency.h"
#include "eeprom.h"
#include "crc16-ccitt.h"


/* Eeprom *********************************************************************/
#define EE_SS_TRIS()    TRISGCLR = 0x0200
#define EE_SSLow()      PORTGCLR = 0x0200
#define EE_SSHigh()     PORTGSET = 0x0200
#define SPIBUF          SPI2ABUF
#define SPICON          SPI2ACON
#define SPISTAT         SPI2ASTAT
#define SPISTATbits     SPI2ASTATbits
#define SPIBRG          SPI2ABRG
static void EE_SPIwrite(uint8_t *tx, uint8_t *rx, uint8_t len);
static void EE_writeEnable();
static void EE_writeByteNoWait(uint8_t data, uint32_t addr);
static uint8_t EE_readByte(uint32_t addr);
static void EE_writeBlock(uint8_t *data, uint32_t addr, uint32_t len);
static void EE_readBlock(uint8_t *data, uint32_t addr, uint32_t len);
static uint8_t EE_verifyBlock(uint8_t *data, uint32_t addr, uint32_t len);
static void EE_writeStatus(uint8_t data);
static uint8_t EE_readStatus();
#define EE_isWriteInProcess()   (EE_readStatus() & 0x01) /* True if write is in process. */

static uint32_t tmpU32;


/* Store parameters ***********************************************************/
static CO_SDO_abortCode_t CO_ODF_1010(CO_ODF_arg_t *ODF_arg){
    CO_EE_t *ee;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    ee = (CO_EE_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* don't change the old value */
        CO_memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

        if(ODF_arg->subIndex == 1){
            if(value == 0x65766173UL){
                EE_MBR_t MBR;

                /* read the master boot record from the last page in eeprom */
                EE_readBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR));
                /* if EEPROM is not yet initilalized, enable it now */
                if(MBR.OD_EEPROMSize != ee->OD_EEPROMSize)
                    ee->OD_EEPROMWriteEnable = true;

                /* prepare MBR */
                MBR.CRC = crc16_ccitt(ee->OD_ROMAddress, ee->OD_ROMSize, 0);
                MBR.OD_EEPROMSize = ee->OD_EEPROMSize;
                MBR.OD_ROMSize = ee->OD_ROMSize;

                /* write to eeprom (blocking function) */
                EE_writeStatus(0); /* unprotect data */
                EE_writeBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR));
                EE_writeBlock(ee->OD_ROMAddress, EE_SIZE/2, ee->OD_ROMSize);
                EE_writeStatus(0x88); /* protect data */

                /* verify data and MBR and status register */
                if(   EE_verifyBlock(ee->OD_ROMAddress, EE_SIZE/2, ee->OD_ROMSize) == 1
                    && EE_verifyBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR)) == 1
                    && (EE_readStatus()&0x8C) == 0x88){
                    /* write successfull */
                    return CO_SDO_AB_NONE;
                }
                return CO_SDO_AB_HW;
            }
            else
                return CO_SDO_AB_DATA_TRANSF;
        }
    }

    return ret;
}


/* Restore default parameters *************************************************/
static CO_SDO_abortCode_t CO_ODF_1011(CO_ODF_arg_t *ODF_arg){
    CO_EE_t *ee;
    uint32_t value;
    CO_SDO_abortCode_t ret = CO_SDO_AB_NONE;

    ee = (CO_EE_t*) ODF_arg->object;
    value = CO_getUint32(ODF_arg->data);

    if(!ODF_arg->reading){
        /* don't change the old value */
        CO_memcpy(ODF_arg->data, (const uint8_t*)ODF_arg->ODdataStorage, 4U);

        if(ODF_arg->subIndex >= 1U){
            if(value == 0x64616F6CUL){
                EE_MBR_t MBR;

                /* read the master boot record from the last page in eeprom */
                EE_readBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR));
                /* verify MBR for safety */
                if(EE_verifyBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR)) == 0)
                    return CO_SDO_AB_HW;

                switch(ODF_arg->subIndex){
                    case 0x01: MBR.OD_ROMSize = 0;              break; /* clear the ROM */
                    /* following don't work, if not enabled in object dictionary */
                    case 0x77: MBR.OD_ROMSize = ee->OD_ROMSize; break; /* restore the ROM back */
                    case 0x7F: MBR.OD_EEPROMSize = 0;           break; /* clear EEPROM */
                    default: return 0x06090011;                        /* Sub-index does not exist. */
                }

                /* write changed MBR */
                EE_writeStatus(0); /* unprotect data */
                EE_writeBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR));
                EE_writeStatus(0x88); /* protect data */

                /* verify MBR and status register */
                if(EE_verifyBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR)) == 1
                    && (EE_readStatus()&0x8C) == 0x88){
                    /* write successfull */
                    return CO_SDO_AB_NONE;
                }
                else{
                    return CO_SDO_AB_HW;
                }
            }
            else
                return CO_SDO_AB_DATA_TRANSF;
        }
    }

    return ret;
}


/******************************************************************************/
CO_ReturnError_t CO_EE_init_1(
        CO_EE_t                *ee,
        uint8_t                *OD_EEPROMAddress,
        uint32_t                OD_EEPROMSize,
        uint8_t                *OD_ROMAddress,
        uint32_t                OD_ROMSize)
{

    /* verify arguments */
    if(ee==NULL || OD_EEPROMAddress==NULL || OD_ROMAddress==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure SPI port for use with eeprom */
    SPICON = 0;           /* Stops and restes the SPI */
    SPISTAT = 0;
    tmpU32 = SPIBUF;      /* Clear the receive buffer */
    SPIBRG = 4;           /* Clock = FPB / ((4+1) * 2) */
    SPICON = 0x00018120;  /* MSSEN = 0 - Master mode slave select enable bit */
                                  /* ENHBUF(bit16) = 1 - Enhanced buffer enable bit */
                                  /* Enable SPI, 8-bit mode */
                                  /* SMP = 0, CKE = 1, CKP = 0 */
                                  /* MSTEN = 1 - master mode enable bit */

    /* Set IOs directions for EEPROM SPI */
    EE_SSHigh();
    EE_SS_TRIS();

    /* verify variables */
    if(OD_ROMSize > (EE_SIZE/2 - EE_PAGE_SIZE)) OD_ROMSize = EE_SIZE/2 - EE_PAGE_SIZE;
    if(OD_EEPROMSize > EE_SIZE/2) OD_EEPROMSize = EE_SIZE/2;

    /* configure object variables */
    ee->OD_EEPROMAddress = OD_EEPROMAddress;
    ee->OD_EEPROMSize = OD_EEPROMSize;
    ee->OD_ROMAddress = OD_ROMAddress;
    ee->OD_ROMSize = OD_ROMSize;
    ee->OD_EEPROMCurrentIndex = 0;
    ee->OD_EEPROMWriteEnable = false;

    /* read the master boot record from the last page in eeprom */
    EE_MBR_t MBR;
    EE_readBlock((uint8_t*)&MBR, EE_SIZE - EE_PAGE_SIZE, sizeof(MBR));

    /* read the CO_OD_EEPROM from EEPROM, first verify, if data are OK */
    if(MBR.OD_EEPROMSize == OD_EEPROMSize && (MBR.OD_ROMSize == OD_ROMSize || MBR.OD_ROMSize == 0)){
        uint32_t firstWordRAM = *((uint32_t*)OD_EEPROMAddress);
        uint32_t firstWordEE, lastWordEE;
        EE_readBlock((uint8_t*)&firstWordEE, 0, 4);
        EE_readBlock((uint8_t*)&lastWordEE, OD_EEPROMSize-4, 4);
        if(firstWordRAM == firstWordEE && firstWordRAM == lastWordEE){
            EE_readBlock(OD_EEPROMAddress, 0, OD_EEPROMSize);
            ee->OD_EEPROMWriteEnable = true;
        }
        else{
            return CO_ERROR_DATA_CORRUPT;
        }
    }
    else{
        return CO_ERROR_DATA_CORRUPT;
    }

    /* read the CO_OD_ROM from EEPROM and verify CRC */
    if(MBR.OD_ROMSize == OD_ROMSize){
        EE_readBlock(OD_ROMAddress, EE_SIZE/2, OD_ROMSize);
        if(crc16_ccitt(OD_ROMAddress, OD_ROMSize, 0) != MBR.CRC){
            return CO_ERROR_CRC;
        }
    }

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_EE_init_2(
        CO_EE_t                *ee,
        CO_ReturnError_t        eeStatus,
        CO_SDO_t               *SDO,
        CO_EM_t                *em)
{
    CO_OD_configure(SDO, OD_H1010_STORE_PARAM_FUNC, CO_ODF_1010, (void*)ee, 0, 0U);
    CO_OD_configure(SDO, OD_H1011_REST_PARAM_FUNC, CO_ODF_1011, (void*)ee, 0, 0U);
    if(eeStatus != CO_ERROR_NO){
        CO_errorReport(em, CO_EM_NON_VOLATILE_MEMORY, CO_EMC_HARDWARE, (uint32_t)eeStatus);
    }
}


/******************************************************************************/
void CO_EE_process(CO_EE_t *ee){
    if(ee && ee->OD_EEPROMWriteEnable && !EE_isWriteInProcess()){
        /* verify next word */
        if(++ee->OD_EEPROMCurrentIndex == ee->OD_EEPROMSize) ee->OD_EEPROMCurrentIndex = 0;
        unsigned int i = ee->OD_EEPROMCurrentIndex;

        /* read eeprom */
        uint8_t RAMdata = ee->OD_EEPROMAddress[i];
        uint8_t EEdata = EE_readByte(i);

        /* if bytes in EEPROM and in RAM are different, then write to EEPROM */
        if(EEdata != RAMdata)
            EE_writeByteNoWait(RAMdata, i);
    }
}


/******************************************************************************/
/* EEPROM 25LC128 on SPI ******************************************************/
/******************************************************************************/
#define EE_CMD_READ     (unsigned)0b00000011
#define EE_CMD_WRITE    (unsigned)0b00000010
#define EE_CMD_WRDI     (unsigned)0b00000100
#define EE_CMD_WREN     (unsigned)0b00000110
#define EE_CMD_RDSR     (unsigned)0b00000101
#define EE_CMD_WRSR     (unsigned)0b00000001


/**
 * Function - EE_SPIwrite
 *
 * Write to SPI and at the same time read from SPI.
 *
 * PIC32 used 16bytes long FIFO buffer with SPI. SPI module is initailized in
 * CO_EE_init.
 *
 * @param tx Ponter to transmitting data. If NULL, zeroes will be transmitted.
 * @param rx Ponter to data buffer, where received data wile be stored.
 * If null, Received data will be disregarded.
 * @param len Length of data buffers. Max 16.
 */
static void EE_SPIwrite(uint8_t *tx, uint8_t *rx, uint8_t len){
    uint32_t i;

    /* write bytes into SPI_TXB fifo buffer */
    if(tx) for(i=0; i<len; i++) SPIBUF = tx[i];
    else   for(i=0; i<len; i++) SPIBUF = 0;

    /* read bytes from SPI_RXB fifo buffer */
    for(i=0; i<len; i++){
        while(SPISTATbits.SPIRBE);    /* wait if buffer is empty */
        if(rx) rx[i] = SPIBUF;
        else  tmpU32 = SPIBUF;
    }
}


/**
 * Function - EE_writeEnable
 *
 * Enable write operation in EEPROM. It is called before every writing to it.
 */
static void EE_writeEnable(){
    uint8_t buf = EE_CMD_WREN;

    EE_SSLow();
    EE_SPIwrite(&buf, 0, 1);
    EE_SSHigh();
}


/*
 * Write one byte of data to eeprom. It triggers write, but it does not wait for
 * write cycle to complete. Before next write cycle, EE_isWriteInProcess must
 * be checked.
 *
 * @param data Data byte to be written.
 * @param addr Address in eeprom, where data will be written.
 */
static void EE_writeByteNoWait(uint8_t data, uint32_t addr){
    uint8_t buf[4];

    EE_writeEnable();

    buf[0] = EE_CMD_WRITE;
    buf[1] = (uint8_t) (addr >> 8);
    buf[2] = (uint8_t) addr;
    buf[3] = data;

    EE_SSLow();
    EE_SPIwrite(buf, 0, 4);
    EE_SSHigh();
}


/*
 * Read one byte of data from eeprom.
 *
 * @param addr Address in eeprom, where data will be written.
 *
 * @return Data byte read.
 */
static uint8_t EE_readByte(uint32_t addr){
    uint8_t bufTx[4];
    uint8_t bufRx[4];

    bufTx[0] = EE_CMD_READ;
    bufTx[1] = (uint8_t) (addr >> 8);
    bufTx[2] = (uint8_t) addr;
    bufTx[3] = 0;

    EE_SSLow();
    EE_SPIwrite(bufTx, bufRx, 4);
    EE_SSHigh();

    return bufRx[3];
}


/*
 * Write block of data to eeprom. It is blockung function, so it waits, untill
 * all data are written.
 *
 * @param data Pointer to data to be written.
 * @param addr Address in eeprom, where data will be written. *If data are
 * stored accross multiple pages, address must be aligned with page.*
 * @param len Length of the data block.
 */
static void EE_writeBlock(uint8_t *data, uint32_t addr, uint32_t len){

    #if EE_PAGE_SIZE != 64
        #error incompatibility in function
    #endif

    while(EE_isWriteInProcess());

    while(len){
        uint8_t buf[3];
        uint32_t i;

        EE_writeEnable();

        buf[0] = EE_CMD_WRITE;
        buf[1] = (uint8_t) (addr >> 8);
        buf[2] = (uint8_t) addr;

        EE_SSLow();
        EE_SPIwrite(buf, 0, 3);

        for(i=0; i<4; i++){
            if(len > 16){
                EE_SPIwrite(data, 0, 16);
                len -= 16;
                data += 16;
            }
            else{
                EE_SPIwrite(data, 0, len);
                len = 0;
                break;
            }
        }
        EE_SSHigh();

        /*  wait for completion of the write operation */
        i=EE_readStatus();
        while(EE_isWriteInProcess());
        addr += EE_PAGE_SIZE;
    }
}


/*
 * Read block of data from eeprom.
 *
 * @param data Pointer to data buffer, where data will be stored.
 * @param addr Address in eeprom, from where data will be read.
 * @param len Length of the data block to be read.
 */
static void EE_readBlock(uint8_t *data, uint32_t addr, uint32_t len){
    uint8_t buf[3];

    buf[0] = EE_CMD_READ;
    buf[1] = (uint8_t) (addr >> 8);
    buf[2] = (uint8_t) addr;

    EE_SSLow();
    EE_SPIwrite(buf, 0, 3);

    while(len){
        if(len > 16){
            EE_SPIwrite(0, data, 16);
            len -= 16;
            data += 16;
        }
        else{
            EE_SPIwrite(0, data, len);
            len = 0;
        }
    }

    EE_SSHigh();
}


/*
 * Compare block of data with data stored in eeprom.
 *
 * @param data Pointer to data buffer, which will be compared.
 * @param addr Address of data in eeprom, which will be compared.
 * @param len Length of the data block to be compared.
 *
 * @return 0 - comparision failed.
 * @return 1 - data are equal.
 */
static uint8_t EE_verifyBlock(uint8_t *data, uint32_t addr, uint32_t len){
    uint8_t buf[16];
    uint8_t equal = 1;

    buf[0] = EE_CMD_READ;
    buf[1] = (uint8_t) (addr >> 8);
    buf[2] = (uint8_t) addr;

    EE_SSLow();
    EE_SPIwrite(buf, 0, 3);

    while(len){
        if(len > 16){
            uint8_t i;
            EE_SPIwrite(0, buf, 16);
            for(i=0; i<16; i++) if(buf[i] != data[i]) equal = 0;
            len -= 16;
            data += 16;
        }
        else{
            uint8_t i;
            EE_SPIwrite(0, buf, len);
            for(i=0; i<len; i++) if(buf[i] != data[i]) equal = 0;
            len = 0;
        }
    }

    EE_SSHigh();
    return equal;
}


/*
 * Write eeprom status register.
 *
 * @param data Data byte to be written to status register.
 */
static void EE_writeStatus(uint8_t data){
    uint8_t buf[2];

    EE_writeEnable();

    EE_SSLow();
    buf[0] = EE_CMD_WRSR;
    buf[1] = data;
    EE_SPIwrite(buf, 0, 2);
    EE_SSHigh();

    while(EE_isWriteInProcess());
}


/*
 * Read eeprom status register.
 *
 * @return Data read from status register.
 */
static uint8_t EE_readStatus(){
    uint8_t bufTx[2] = {EE_CMD_RDSR, 0};
    uint8_t bufRx[2];

    EE_SSLow();
    EE_SPIwrite(bufTx, bufRx, 2);
    EE_SSHigh();

    return bufRx[1];
}
