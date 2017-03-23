/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2015 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Following clarification and special exception to the GNU General Public
 * License is included to the distribution terms of CANopenNode:
 *
 * Linking this library statically or dynamically with other modules is
 * making a combined work based on this library. Thus, the terms and
 * conditions of the GNU General Public License cover the whole combination.
 *
 * As a special exception, the copyright holders of this library give
 * you permission to link this library with independent modules to
 * produce an executable, regardless of the license terms of these
 * independent modules, and to copy and distribute the resulting
 * executable under terms of your choice, provided that you also meet,
 * for each linked independent module, the terms and conditions of the
 * license of that module. An independent module is a module which is
 * not derived from or based on this library. If you modify this
 * library, you may extend this exception to your version of the
 * library, but you are not obliged to do so. If you do not wish
 * to do so, delete this exception statement from your version.
 */

/**
 * Implementation notes
 *
 * This was written by c0d3runn3r (JS) for the Kinetis K20, initially as running on the 'Teensy' platform.
 *
 * PDOs remain untested
 *
 * For debug output, #define NN_DEBUG and create a global debug() function to log output.
 */

#ifdef NN_DEBUG
extern void debug(const char *format, ...);
# define DEBUG_PRINT(x) debug x
#else
# define DEBUG_PRINT(x) do {} while (0)
#endif

// Number of message buffers to configure/allow (limited by chip architecture)
#define NO_OF_MB 16

#include "CO_driver.h"
#include "CO_Emergency.h"


/******************************************************************************/
void CO_CANsetConfigurationMode(int32_t CANbaseAddress){
    /* Put CAN module in configuration mode */
    DEBUG_PRINT(("CO_CANsetConfigurationMode\n"));

    // // FlexCAN doesn't seem to have a simple and clean 'configuration mode'... there are some things that need to be done
    // in disable mode, and most everthing else is done from freeze mode (which is the state we are leaving things in here)

    // Disable CAN module
    FLEXCAN0_MCR|=FLEXCAN_MCR_MDIS; // Disable 
    while((FLEXCAN0_MCR & FLEXCAN_MCR_LPM_ACK)==0 /*|| (FLEXCAN0_MCR & FLEXCAN_MCR_MDIS)==0*/); // Wait for disable ACK, 44.4.10.2 says wait for both

    // Set clock source while in disable mode (44.5.1)
    FLEXCAN0_CTRL1 &= ~ FLEXCAN_CTRL_CLK_SRC;

    // Enable CAN module, wait for freeze mode
    FLEXCAN0_MCR&=~FLEXCAN_MCR_MDIS;    // Enable 
    while(FLEXCAN0_MCR & FLEXCAN_MCR_LPM_ACK);  // Wait for disable mode to end


    while((FLEXCAN0_MCR & FLEXCAN_MCR_FRZ_ACK)==0); // Wait for freeze mode to start
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){

    DEBUG_PRINT(("CO_CANsetNormalMode\n"));
    /* Put CAN module in normal mode */

    // Exit freeze mode
    FLEXCAN0_MCR&=~FLEXCAN_MCR_HALT;
    while(FLEXCAN0_MCR & FLEXCAN_MCR_FRZ_ACK);

    // Wait until ready
    while(FLEXCAN0_MCR & FLEXCAN_MCR_NOT_RDY);

    CANmodule->CANnormal = true;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        int32_t                 CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    uint16_t i;

    DEBUG_PRINT(("CO_CANmodule_init\n"));

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANbaseAddress = CANbaseAddress;     // Ignored... we should start using this if we care about multiple CAN modules
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = (rxSize <= 32U) ? true : false;/* microcontroller dependent */
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;
    CANmodule->em = NULL;

    for(i=0U; i<rxSize; i++){
        rxArray[i].ident = 0U;
        rxArray[i].pFunct = NULL;
    }
    for(i=0U; i<txSize; i++){
        txArray[i].bufferFull = false;
    }

    // 44.5.1 initialize the MCR
    FLEXCAN0_IFLAG1 &= FLEXCAN0_IFLAG1;     // Clear all flags before setting MAXMB 44.3.11
    FLEXCAN0_MCR |= (FLEXCAN_MCR_IRMQ |  /*FLEXCAN_MCR_WRN_EN |*/ FLEXCAN_MCR_SRX_DIS | FLEXCAN_MCR_MAXMB(NO_OF_MB)); // Set up individual mailbox masking, warning interrupts (TODO), disable self reception, set mailbox count

    // 44.5.1 initialize the control register (set buadrate lifted from FlexCAN driver)
    if ( 50 == CANbitRate ) {
    FLEXCAN0_CTRL1 = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                      | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(19));
    } else if ( 100 == CANbitRate ) {
    FLEXCAN0_CTRL1 = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                      | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(9));
    } else if ( 250 == CANbitRate ) {
    FLEXCAN0_CTRL1 = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                      | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(3));
    } else if ( 500 == CANbitRate ) {
    FLEXCAN0_CTRL1 = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                      | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(1));
    } else if ( 1000 == CANbitRate ) {
    FLEXCAN0_CTRL1 = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(0)
                      | FLEXCAN_CTRL_PSEG1(1) | FLEXCAN_CTRL_PSEG2(1) | FLEXCAN_CTRL_PRESDIV(1));
    } else { // 125000
    FLEXCAN0_CTRL1 = (FLEXCAN_CTRL_PROPSEG(2) | FLEXCAN_CTRL_RJW(1)
                      | FLEXCAN_CTRL_PSEG1(7) | FLEXCAN_CTRL_PSEG2(3) | FLEXCAN_CTRL_PRESDIV(7));
    }

    /* Configure CAN module hardware filters */
    if(CANmodule->useCANrxFilters){

        // Set EACEN so that incoming frames are matched for both RTR and IDE
        FLEXCAN0_CTRL2 |= FLEXCAN_CTRL2_EACEN;

        // 44.5.1 Initialize the individual mask registers
        uint8_t n;
        for(n=0; n<NO_OF_MB; n++) {
    
            FLEXCAN0_RXIMRn(n)=FLEXCAN_MB_ID_IDSTD(0x07ff);     // Care about all 11 bits (0x7ff)   
        }

    }
    else{

        // They are going to use software to do this. We will accept all messages.
        uint8_t n;
        for(n=0; n<NO_OF_MB; n++) {
    
            FLEXCAN0_RXIMRn(n)=FLEXCAN_MB_ID_IDSTD(0x0);     // Allow any message  
        }
    }



    // Turn off automatic remote request (treat RRQ as any other frame), start matching with MBs (not FIFO)
    FLEXCAN0_CTRL2 |= (FLEXCAN_CTRL2_RRS| FLEXCAN_CTRL2_MRP);

    // Generate an interrupt for all successful receives or transmits (TODO do this in CTRL for error interrupts - we are not currently generating interrupts for errors)
    FLEXCAN0_IMASK1|=0xffffffff; //FLEXCAN_IMASK1_BUF0M;

    // Set MB0 for transmit (probably not really necessary, since we are not putting anything into it)
    FLEXCAN0_MB0_CS=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_INACTIVE);

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    /* turn off the module */
}


/******************************************************************************/
// This has been turned into a macro... see CO_driver.h
// uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
//     return (uint16_t) (rxMsg->ident >> 18)& 0x07ff;
// }


/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        bool_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message))
{

    uint16_t mb_index=index+1;  // MB is offset by one since MB 0 is the TX buffer

    DEBUG_PRINT(("CO_CANrxBufferInit mb_index=%d, index=%d, ident=%#x, mask=%#x, rtr=%#x\n", mb_index, index, ident, mask, rtr));


    CO_ReturnError_t ret = CO_ERROR_NO;

    if(mb_index>=NO_OF_MB){

        ret= CO_ERROR_OUT_OF_MEMORY;

    } else {

        if((CANmodule!=NULL) && (object!=NULL) && (pFunct!=NULL) && (index < CANmodule->rxSize)){
            /* buffer, which will be configured */
            CO_CANrx_t *buffer = &CANmodule->rxArray[index];

            /* Configure object variables */
            buffer->object = object;
            buffer->pFunct = pFunct;
            buffer->ident = ident & 0x07FFU;    /* This is not aligned for the register */
            if(rtr){                            /* We don't use RTR as bit 11, but CanOpenNode seems to.  Not sure if this is really needed? */
                buffer->ident |= 0x0800U;
            }
            buffer->mask = (mask & 0x07FFU) | 0x0800U;

            /* Set CAN hardware module filter and mask. */
            if(CANmodule->useCANrxFilters){  

                FLEXCAN0_RXIMRn(mb_index)=FLEXCAN_MB_ID_IDSTD(mask)     // Previously set to either 0x7FF or 0x0 in module init
                    |((rtr)?0x80000000:0);                              // Set the bit in the RXIMR mask (so it cares about the RTR bit in the message buffer)
            }   

            // 44.5.1 Initialize the message buffers
            FLEXCAN0_MBn_ID(mb_index)=FLEXCAN_MB_ID_IDSTD(ident);   // Set ID
            FLEXCAN0_MBn_CS(mb_index)=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_RX_EMPTY)
                | ((rtr)?FLEXCAN_MB_CS_RTR:0);    // Set CODE, implicitly set IDE and RTR to 0.  EMPTY = allows reception.


            
        }
        else{

            ret = CO_ERROR_ILLEGAL_ARGUMENT;
        }
    }

    if(ret!=CO_ERROR_NO){

        DEBUG_PRINT(("CO_CANrxBufferInit error %#x\n",ret));
    }
    return ret;
}


/**
 * Init a TX buffer
 * Inits the internal RAM buffer, not the chip registers
 * 
 * Okay.  So from what I can tell, they will call txBufferInit at least once before sending
 * CAN packets, and they expect us to save noOfBytes etc for when the actual packet is sent.  
 * This is a little confusing because the buffer itself has a DTR (length) setting and they 
 * don't make clear whether it is set at init time (now) or later at send time.
 * Ths upshot is that we need to use this init call to init the buffer, including DTR.
 * And we need to have the CO_CAN_tx_t struct set up so that they *can* modify DTR later if they want to, 
 * though we don't count on that.
 * For this driver, we actually set up the TX buffer at the same time we configure the rest of the module.
 */
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag)
{

    //DEBUG_PRINT(("CO_CANtxBufferInit ident=%#x, rtr=%#x, noOfBytes=%#x\n", ident, rtr, noOfBytes));


    CO_CANtx_t *buffer = NULL;
    if((CANmodule != NULL) && (index < CANmodule->txSize)){

        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* Save parameters in buffer */ 
        buffer->CODE=FLEXCAN_MB_CODE_TX_ONCE;
        buffer->DLC=noOfBytes;          // Set this as its own independent field, in case a module decides to change it without re-init()ing it
        buffer->DLC_flags=((rtr)?0x10:0);   // Later ORed together with DLC, setting it's upper nibble (this is how our registers expect it)
        buffer->ident = ident & 0x07FFU;  // Not as aligned in register, so we can log it etc if desired
        buffer->bufferFull = false;     // Meaning, there is no data herein waiting to be sent
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){

    DEBUG_PRINT(("CO_CANsend buffer->(ident=%#x, DLC=%d, data=",buffer->ident, buffer->DLC));
    #ifdef NN_DEBUG
    uint8_t n;
    for(n=0; n<buffer->DLC; n++){
        DEBUG_PRINT(("%x ", buffer->data[n]));
    }
    #endif
    DEBUG_PRINT((")\n"));


    CO_ReturnError_t err = CO_ERROR_NO;

    /* If we have been called with a buffer whose bufferFull==true, it means that this buffer was overwritten while waiting to be sent! */
    if(buffer->bufferFull){
        if(!CANmodule->firstCANtxMessage){
            /* don't set error, if bootup message is still on buffers */
            CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, buffer->ident);
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND();

    /* if CAN TX buffer is free, and no previous messages are waiting to be sent, send message now */
    if(FLEXCAN_get_code(FLEXCAN0_MB0_CS)==FLEXCAN_MB_CODE_TX_INACTIVE && CANmodule->CANtxCount == 0){

//        CANmodule->bufferInhibitFlag = buffer->syncFlag;  // TODO... this is for sync PDOs (?)

        // Send the message
        FLEXCAN0_MB0_ID=FLEXCAN_MB_ID_IDSTD(buffer->ident);  // Set ID
        FLEXCAN0_MB0_WORD0=(buffer->data[0] <<24) | (buffer->data[1] << 16) |  (buffer->data[2] <<8) | buffer->data[3];   
        FLEXCAN0_MB0_WORD1=(buffer->data[4] <<24) | (buffer->data[5] << 16) |  (buffer->data[6] <<8) | buffer->data[7];
        FLEXCAN0_MB0_CS=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ONCE)          /* ONCE means ready to send.  Implicitly set TIMESTAMP etc to 0 */
                    | (((buffer->DLC & 0x0f) | (buffer->DLC_flags & 0xf0))<<FLEXCAN_MB_CS_DLC_BIT_NO);

        // Mark buffer for reuse (though it should have been set this way already)
        buffer->bufferFull=false;
    }

    /* if no buffer is free, message will be sent by interrupt (which will also decrement CANtxCount) */
    else{
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
    }
    CO_UNLOCK_CAN_SEND();

    return err;
}


/******************************************************************************/
/* TODO - NOT IMPLEMENTED */
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
    uint32_t tpdoDeleted = 0U;

    CO_LOCK_CAN_SEND();
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if(/*messageIsOnCanBuffer && */CANmodule->bufferInhibitFlag){
        /* clear TXREQ */
        CANmodule->bufferInhibitFlag = false;
        tpdoDeleted = 1U;
    }
    /* delete also pending synchronous TPDOs in TX buffers */
    if(CANmodule->CANtxCount != 0U){
        uint16_t i;
        CO_CANtx_t *buffer = &CANmodule->txArray[0];
        for(i = CANmodule->txSize; i > 0U; i--){
            if(buffer->bufferFull){
                if(buffer->syncFlag){
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;
                    tpdoDeleted = 2U;
                }
            }
            buffer++;
        }
    }
    CO_UNLOCK_CAN_SEND();


    if(tpdoDeleted != 0U){
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_TPDO_OUTSIDE_WINDOW, CO_EMC_COMMUNICATION, tpdoDeleted);
    }
}


/**
 * Report errors
 *
 * This strangely named function appears to be nothing more than a way for us to report errors.
 * We seem to track the old state of errors (CANmodule->errOld) so that we only report changes.
 *
 * When a CAN emergency message is sent
 *  - first two bytes are a standard error code (CO_EM_errorCodes)
 *  - third byte is an error register.  If nonzero, we can't enter operational mode.  From CO_Emergency.c, this seems to happen if we set a O_EM_errorStatusBits with bits 2, 3, or 5 set (probably the constants with ERROR in the title)
 *  - fourth byte is CO_EM_errorStatusBits, a CANopenNode specific error
 *  - remaining 4 bytes are `err`
 *
 * @param {CO_CANmodule_t*} CANmodule 
 */
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule){

    // err will be our one stop shop for storing errors.  TX and RX errors are the top two bytes (31:16).  Other errors are in the bottom two (15:0).  Reading ESR also resets bits 15:10
    uint32_t err=((FLEXCAN0_ECR < 16) & 0xffff0000) | (FLEXCAN0_ESR1 & 0x0000ffff);

    // Get a reference to the error module, and break out tx and rx errors
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;
    uint32_t txErrors=(err & 0x00FF0000U)>>16;
    uint32_t rxErrors=(err & 0xFF000000U)>>24;

    // Reset all errors in ESR
    FLEXCAN0_ESR1=0xffffffff;

    // Report any new errors. First code is CO_EM_errorStatusBits, a specific CANOpenNode error.  Second code is CO_EM_errorCodes, a standard RFC code.
    if(CANmodule->errOld != err){
        CANmodule->errOld = err;

        // Error interrupt.  Something bad has happened, throw a generic error.
        if(err & FLEXCAN_ESR_ERR_INT) {  

            CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_COMMUNICATION, err);  // Lookup bits 15:8 of err in datasheet 44.3.9, "Error and Status 1 register (CANx_ESR1)"
        }

        // The following errors came from the driver template, so I left them.  CAN error counter rules are apparently specified by protocol, which the K20 implements.
        if(txErrors >= 256U){                               /* bus off */
            CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
        }
        else{                                               /* not bus off */
            CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);

            if((rxErrors >= 96U) || (txErrors >= 96U)){     /* bus warning */
                CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
            }

            if(rxErrors >= 128U){                           /* RX bus passive */
                CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
            }
            else{
                CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, err);
            }

            if(txErrors >= 128U){                           /* TX bus passive */
                if(!CANmodule->firstCANtxMessage){
                    CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
                }
            }
            else{
                bool_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
                if(isError){
                    CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
                    CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                }
            }

            if((rxErrors < 96U) && (txErrors < 96U)){       /* no error */
                CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
            }
        }

    }
}


/******************************************************************************/
void CO_CANinterrupt(CO_CANmodule_t *CANmodule){

    // I finally gave up making a CO_CANrxMsg_t that aligns with our registers... the CANopen modules insist on reading data as an array of bytes, but we must read it as two DWORDS.
    static CO_CANrxMsg_t rcvMsg;    // Should be on the heap (I hope...)

    cli();

    // Figure out which buffer triggered the interrupt
    uint8_t mb_index=0;
    uint32_t iflag=FLEXCAN0_IFLAG1;        
    while(!(iflag & 1)){

        iflag >>= 1;
        mb_index++;
    }

    DEBUG_PRINT(("Interrupt: IFLAG= %#x (mb_index %d)\n", iflag, mb_index));


    /* receive interrupt - we use the first 8 MBs for receive */
    if(mb_index > 0 && mb_index < NO_OF_MB){

        uint8_t index=mb_index-1;   // Since MB 0 is for transmit, RX buffers are misaligned from MB by 1
        CO_CANrx_t *buffer = NULL;  /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        // Copy the buffer to rcvMsg.  I tried really hard to have a register aligned CO_CANrxMsg_t but in the end it just didn't seem possible.  Oh well, it is a fast processor and this extra copy shouldn't be a big deal.
        rcvMsg.DLC=FLEXCAN_get_length(FLEXCAN0_MBn_CS(mb_index));
        rcvMsg.ident=(FLEXCAN0_MBn_ID(mb_index) & FLEXCAN_MB_ID_STD_MASK) >> FLEXCAN_MB_ID_STD_BIT_NO;
        rcvMsg.data[0]=(uint8_t)((FLEXCAN0_MBn_WORD0(mb_index)>>24)& 0xFF);
        rcvMsg.data[1]=(uint8_t)((FLEXCAN0_MBn_WORD0(mb_index)>>16)& 0xFF);
        rcvMsg.data[2]=(uint8_t)((FLEXCAN0_MBn_WORD0(mb_index)>>8)& 0xFF);
        rcvMsg.data[3]=(uint8_t)((FLEXCAN0_MBn_WORD0(mb_index)>>0)& 0xFF);
        rcvMsg.data[4]=(uint8_t)((FLEXCAN0_MBn_WORD1(mb_index)>>24)& 0xFF);
        rcvMsg.data[5]=(uint8_t)((FLEXCAN0_MBn_WORD1(mb_index)>>16)& 0xFF);
        rcvMsg.data[6]=(uint8_t)((FLEXCAN0_MBn_WORD1(mb_index)>>8)& 0xFF);
        rcvMsg.data[7]=(uint8_t)((FLEXCAN0_MBn_WORD1(mb_index)>>0)& 0xFF);

        DEBUG_PRINT(("...RX message: ident=%#x, mb_index=%d, index=%d, DLC=%d, DATA=", rcvMsg.ident, mb_index, index, rcvMsg.DLC));
        #ifdef NN_DEBUG
        uint8_t n;
        for(n=0; n<8; n++){
            DEBUG_PRINT(("%x ", rcvMsg.data[n]));
        }
        #endif
        DEBUG_PRINT((")\n"));


        if(CANmodule->useCANrxFilters){
            /* CAN module filters are used. Message with known 11-bit identifier has */
            /* been received */

            DEBUG_PRINT(("useCANrxFilters\n"));

            if(index < CANmodule->rxSize){
                buffer = &CANmodule->rxArray[index];
                /* verify also RTR */
                if(((rcvMsg.ident ^ buffer->ident) & buffer->mask) == 0U){
                    msgMatched = true;
                }
            }
        }
        else{
            /* CAN module filters are not used, message with any standard 11-bit identifier */
            /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
            buffer = &CANmodule->rxArray[0];
            for(index = CANmodule->rxSize; index > 0U; index--){
                if(((rcvMsg.ident ^ buffer->ident) & buffer->mask) == 0U){
                    msgMatched = true;
                    break;
                }
                buffer++;
            }
        }

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer != NULL) && (buffer->pFunct != NULL)){

            DEBUG_PRINT(("...found!\n"));
            buffer->pFunct(buffer->object, &rcvMsg);
        }
        if(!msgMatched){

            DEBUG_PRINT(("...RX not found\n"));
        }

        // // Restore saved CS dword
        // FLEXCAN0_MBn_CS(mb_index)=saved_cs;

        // Set buffer to EMPTY so it can send again
        FLEXCAN0_MBn_CS(mb_index)&=~FLEXCAN_MB_CS_CODE_MASK;
        FLEXCAN0_MBn_CS(mb_index)|=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_RX_EMPTY);  

    }


    /* transmit interrupt */
    else if(mb_index==0){

        /* First CAN message (bootup) was sent successfully */
        CANmodule->firstCANtxMessage = false;
        /* clear flag from previous message */
        CANmodule->bufferInhibitFlag = false;
        /* Are there any new messages waiting to be send */
        if(CANmodule->CANtxCount > 0U){
            uint16_t i;             /* index of transmitting message */

            /* first buffer */
            CO_CANtx_t *buffer = &CANmodule->txArray[0];
            /* search through whole array of pointers to transmit message buffers. */
            for(i = CANmodule->txSize; i > 0U; i--){
                /* if message buffer is full, send it. */
                if(buffer->bufferFull){
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;

//                    CANmodule->bufferInhibitFlag = buffer->syncFlag;  // TODO
                    // Send the message
                    FLEXCAN0_MB0_ID=FLEXCAN_MB_ID_IDSTD(buffer->ident);  // Set ID
                    FLEXCAN0_MB0_WORD0=(buffer->data[0] <<24) | (buffer->data[1] << 16) |  (buffer->data[2] <<8) | buffer->data[3];   
                    FLEXCAN0_MB0_WORD1=(buffer->data[4] <<24) | (buffer->data[5] << 16) |  (buffer->data[6] <<8) | buffer->data[7];
                    FLEXCAN0_MB0_CS=FLEXCAN_MB_CS_CODE(FLEXCAN_MB_CODE_TX_ONCE)          /* ONCE means ready to send.  Implicitly set TIMESTAMP etc to 0 */
                                | (((buffer->DLC & 0x0f) | (buffer->DLC_flags & 0xf0))<<FLEXCAN_MB_CS_DLC_BIT_NO);

                    break;                      /* exit for loop */
                }
                buffer++;
            }/* end of for loop */

            /* Clear counter if no more messages */
            if(i == 0U){
                CANmodule->CANtxCount = 0U;
            }
        }
    }
    else{
        /* some other interrupt reason */
        /* TODO - handle error interrupts here?  (After enabling them) */
    }


    /* Clear interrupt flag */
    FLEXCAN0_IFLAG1 &= (1<<mb_index);
    sei();

}
