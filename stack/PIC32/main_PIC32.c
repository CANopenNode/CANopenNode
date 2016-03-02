/*
 * CANopen main program file for PIC32 microcontroller.
 *
 * @file        main_PIC32.c
 * @author      Janez Paternoster
 * @copyright   2010 - 2015 Janez Paternoster
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


#define CO_FSYS     64000      /* (8MHz Quartz used) */
#define CO_PBCLK    32000      /* peripheral bus clock */


#include "CANopen.h"
#include "application.h"
#ifdef USE_EEPROM
    #include "eeprom.h"            /* 25LC128 eeprom chip connected to SPI2A port. */
#endif
#include <xc.h>                 /* for interrupts */
#include <sys/attribs.h>        /* for interrupts */


/* Configuration bits */
    #pragma config FVBUSONIO = OFF      /* USB VBUS_ON Selection (OFF = pin is controlled by the port function) */
    #pragma config FUSBIDIO = OFF       /* USB USBID Selection (OFF = pin is controlled by the port function) */
    #pragma config UPLLEN = OFF         /* USB PLL Enable */
    #pragma config UPLLIDIV = DIV_12    /* USB PLL Input Divider */
    #pragma config FCANIO = ON          /* CAN IO Pin Selection (ON = default CAN IO Pins) */
    #pragma config FETHIO = ON          /* Ethernet IO Pin Selection (ON = default Ethernet IO Pins) */
    #pragma config FMIIEN = ON          /* Ethernet MII Enable (ON = MII enabled) */
    #pragma config FSRSSEL = PRIORITY_7 /* SRS (Shadow registers set) Select */
    #pragma config POSCMOD = XT         /* Primary Oscillator */
    #pragma config FSOSCEN = OFF        /* Secondary oscillator Enable */
    #pragma config FNOSC = PRIPLL       /* Oscillator Selection */
    #pragma config FPLLIDIV = DIV_2     /* PLL Input Divider */
    #pragma config FPLLMUL = MUL_16     /* PLL Multiplier */
    #pragma config FPLLODIV = DIV_1     /* PLL Output Divider Value */
    #pragma config FPBDIV = DIV_2       /* Bootup PBCLK divider */
    #pragma config FCKSM = CSDCMD       /* Clock Switching and Monitor Selection */
    #pragma config OSCIOFNC = OFF       /* CLKO Enable */
    #pragma config IESO = OFF           /* Internal External Switch Over */
#pragma config FWDTEN = OFF          /* Watchdog Timer Enable */
    #pragma config WDTPS = PS1024       /* Watchdog Timer Postscale Select (in milliseconds) */
#pragma config CP = OFF              /* Code Protect Enable */
    #pragma config BWP = ON             /* Boot Flash Write Protect */
    #pragma config PWP = PWP256K        /* Program Flash Write Protect */
#ifdef CO_ICS_PGx1
    #pragma config ICESEL = ICS_PGx1    /* ICE/ICD Comm Channel Select */
#else
    #pragma config ICESEL = ICS_PGx2    /* ICE/ICD Comm Channel Select (2 for Explorer16 board) */
#endif
    #pragma config DEBUG = ON           /* Background Debugger Enable */


/* macros */
    #define CO_TMR_TMR          TMR2             /* TMR register */
    #define CO_TMR_PR           PR2              /* Period register */
    #define CO_TMR_CON          T2CON            /* Control register */
    #define CO_TMR_ISR_FLAG     IFS0bits.T2IF    /* Interrupt Flag bit */
    #define CO_TMR_ISR_PRIORITY IPC2bits.T2IP    /* Interrupt Priority */
    #define CO_TMR_ISR_ENABLE   IEC0bits.T2IE    /* Interrupt Enable bit */

    #define CO_CAN_ISR() void __ISR(_CAN_1_VECTOR, IPL5SOFT) CO_CAN1InterruptHandler(void)
    #define CO_CAN_ISR_FLAG     IFS1bits.CAN1IF  /* Interrupt Flag bit */
    #define CO_CAN_ISR_PRIORITY IPC11bits.CAN1IP /* Interrupt Priority */
    #define CO_CAN_ISR_ENABLE   IEC1bits.CAN1IE  /* Interrupt Enable bit */

    #define CO_CAN_ISR2() void __ISR(_CAN_2_VECTOR, IPL5SOFT) CO_CAN2InterruptHandler(void)
    #define CO_CAN_ISR2_FLAG     IFS1bits.CAN2IF  /* Interrupt Flag bit */
    #define CO_CAN_ISR2_PRIORITY IPC11bits.CAN2IP /* Interrupt Priority */
    #define CO_CAN_ISR2_ENABLE   IEC1bits.CAN2IE  /* Interrupt Enable bit */

    #define CO_clearWDT() (WDTCONSET = _WDTCON_WDTCLR_MASK)

/* Global variables and objects */
    volatile uint16_t CO_timer1ms = 0U; /* variable increments each millisecond */
    const CO_CANbitRateData_t   CO_CANbitRateData[8] = {CO_CANbitRateDataInitializers};
    static uint32_t tmpU32;
#ifdef USE_EEPROM
    CO_EE_t                     CO_EEO;         /* Eeprom object */
#endif


/* helpers */
void CANrx_lockCbSync(bool_t syncReceived) {
    if(syncReceived) {
        CO_CAN_ISR_ENABLE = 0;
        CO_CAN_ISR2_ENABLE = 0;
    }
}


/* main ***********************************************************************/
int main (void){
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

    /* Configure system for maximum performance. plib is necessary for that.*/
    /* SYSTEMConfig(CO_FSYS*1000, SYS_CFG_WAIT_STATES | SYS_CFG_PCACHE); */

    /* Enable system multi vectored interrupts */
    INTCONbits.MVEC = 1;
    __builtin_enable_interrupts();

    /* Disable JTAG and trace port */
    DDPCONbits.JTAGEN = 0;
    DDPCONbits.TROEN = 0;


    /* Verify, if OD structures have proper alignment of initial values */
    if(CO_OD_RAM.FirstWord != CO_OD_RAM.LastWord) while(1) CO_clearWDT();
    if(CO_OD_EEPROM.FirstWord != CO_OD_EEPROM.LastWord) while(1) CO_clearWDT();
    if(CO_OD_ROM.FirstWord != CO_OD_ROM.LastWord) while(1) CO_clearWDT();


    /* initialize EEPROM - part 1 */
#ifdef USE_EEPROM
    CO_ReturnError_t eeStatus = CO_EE_init_1(&CO_EEO, (uint8_t*) &CO_OD_EEPROM, sizeof(CO_OD_EEPROM),
                            (uint8_t*) &CO_OD_ROM, sizeof(CO_OD_ROM));
#endif


    programStart();


    /* increase variable each startup. Variable is stored in eeprom. */
    OD_powerOnCounter++;


    while(reset != CO_RESET_APP){
/* CANopen communication reset - initialize CANopen objects *******************/
        CO_ReturnError_t err;
        uint16_t timer1msPrevious;
        uint16_t TMR_TMR_PREV = 0;
        uint8_t nodeId;
        uint16_t CANBitRate;

        /* disable CAN and CAN interrupts */
        CO_CAN_ISR_ENABLE = 0;
        CO_CAN_ISR2_ENABLE = 0;

        /* Read CANopen Node-ID and CAN bit-rate from object dictionary */
        nodeId = OD_CANNodeID;
        if(nodeId<1 || nodeId>127) nodeId = 0x10;
        CANBitRate = OD_CANBitRate;/* in kbps */

        /* initialize CANopen */
        err = CO_init(ADDR_CAN1, nodeId, CANBitRate);
        if(err != CO_ERROR_NO){
            while(1) CO_clearWDT();
            /* CO_errorReport(CO->em, CO_EM_MEMORY_ALLOCATION_ERROR, CO_EMC_SOFTWARE_INTERNAL, err); */
        }


        /* initialize eeprom - part 2 */
#ifdef USE_EEPROM
        CO_EE_init_2(&CO_EEO, eeStatus, CO->SDO[0], CO->em);
#endif


        /* Configure callback functions */
        CO_SYNC_initCallback(CO->SYNC, CANrx_lockCbSync);


        /* initialize variables */
        timer1msPrevious = CO_timer1ms;
        OD_performance[ODA_performance_mainCycleMaxTime] = 0;
        OD_performance[ODA_performance_timerCycleMaxTime] = 0;
        reset = CO_RESET_NOT;



        /* Configure Timer interrupt function for execution every 1 millisecond */
        CO_TMR_CON = 0;
        CO_TMR_TMR = 0;
        #if CO_PBCLK > 65000
            #error wrong timer configuration
        #endif
        CO_TMR_PR = CO_PBCLK - 1;  /* Period register */
        CO_TMR_CON = 0x8000;       /* start timer (TON=1) */
        CO_TMR_ISR_FLAG = 0;       /* clear interrupt flag */
        CO_TMR_ISR_PRIORITY = 3;   /* interrupt - set lower priority than CAN (set the same value in interrupt) */

        /* Configure CAN1 Interrupt (Combined) */
        CO_CAN_ISR_FLAG = 0;       /* CAN1 Interrupt - Clear flag */
        CO_CAN_ISR_PRIORITY = 5;   /* CAN1 Interrupt - Set higher priority than timer (set the same value in '#define CO_CAN_ISR_PRIORITY') */
        CO_CAN_ISR2_FLAG = 0;      /* CAN2 Interrupt - Clear flag */
        CO_CAN_ISR2_PRIORITY = 5;  /* CAN Interrupt - Set higher priority than timer (set the same value in '#define CO_CAN_ISR_PRIORITY') */


        communicationReset();


        /* start CAN and enable interrupts */
        CO_CANsetNormalMode(CO->CANmodule[0]);
        CO_TMR_ISR_ENABLE = 1;
        CO_CAN_ISR_ENABLE = 1;

#if CO_NO_CAN_MODULES >= 2
        CO_CANsetNormalMode(CO->CANmodule[1]);
        CO_CAN_ISR2_ENABLE = 1;
#endif


        while(reset == CO_RESET_NOT){
/* loop for normal program execution ******************************************/
            uint16_t timer1msCopy, timer1msDiff;

            CO_clearWDT();


            /* calculate cycle time for performance measurement */
            timer1msCopy = CO_timer1ms;
            timer1msDiff = timer1msCopy - timer1msPrevious;
            timer1msPrevious = timer1msCopy;
            uint16_t t0 = CO_TMR_TMR;
            uint16_t t = t0;
            if(t >= TMR_TMR_PREV){
                t = t - TMR_TMR_PREV;
                t = (timer1msDiff * 100) + (t / (CO_PBCLK / 100));
            }
            else if(timer1msDiff){
                t = TMR_TMR_PREV - t;
                t = (timer1msDiff * 100) - (t / (CO_PBCLK / 100));
            }
            else t = 0;
            OD_performance[ODA_performance_mainCycleTime] = t;
            if(t > OD_performance[ODA_performance_mainCycleMaxTime])
                OD_performance[ODA_performance_mainCycleMaxTime] = t;
            TMR_TMR_PREV = t0;


            /* Application asynchronous program */
            programAsync(timer1msDiff);

            CO_clearWDT();


            /* CANopen process */
            reset = CO_process(CO, timer1msDiff, NULL);

            CO_clearWDT();


#ifdef USE_EEPROM
            CO_EE_process(&CO_EEO);
#endif
        }
    }


/* program exit ***************************************************************/
//    CO_DISABLE_INTERRUPTS();

    /* delete objects from memory */
    programEnd();
    CO_delete(ADDR_CAN1);

    /* reset */
    SYSKEY = 0x00000000;
    SYSKEY = 0xAA996655;
    SYSKEY = 0x556699AA;
    RSWRSTSET = 1;
    tmpU32 = RSWRST;
    while(1);
}


/* timer interrupt function executes every millisecond ************************/
#ifndef USE_EXTERNAL_TIMER_1MS_INTERRUPT
void __ISR(_TIMER_2_VECTOR, IPL3SOFT) CO_TimerInterruptHandler(void){

    CO_TMR_ISR_FLAG = 0;

    CO_timer1ms++;

    if(CO->CANmodule[0]->CANnormal) {
        bool_t syncWas;

        /* Process Sync and read inputs */
        syncWas = CO_process_SYNC_RPDO(CO, 1000);

        /* Re-enable CANrx, if it was disabled by SYNC callback */
        CO_CAN_ISR_ENABLE = 1;
#if CO_NO_CAN_MODULES >= 2
        CO_CAN_ISR2_ENABLE = 1;
#endif

        /* Further I/O or nonblocking application code may go here. */
        program1ms();

        /* Write outputs */
        CO_process_TPDO(CO, syncWas, 1000);

        /* verify timer overflow */
        if(CO_TMR_ISR_FLAG == 1){
            CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL, 0);
            CO_TMR_ISR_FLAG = 0;
        }
   }

    /* calculate cycle time for performance measurement */
    uint16_t t = CO_TMR_TMR / (CO_PBCLK / 100);
    OD_performance[ODA_performance_timerCycleTime] = t;
    if(t > OD_performance[ODA_performance_timerCycleMaxTime])
        OD_performance[ODA_performance_timerCycleMaxTime] = t;
}
#endif


/* CAN interrupt function *****************************************************/
CO_CAN_ISR(){
    CO_CANinterrupt(CO->CANmodule[0]);
    /* Clear combined Interrupt flag */
    CO_CAN_ISR_FLAG = 0;
}

#if CO_NO_CAN_MODULES >= 2
CO_CAN_ISR2(){
    CO_CANinterrupt(CO->CANmodule[1]);
    /* Clear combined Interrupt flag */
    CO_CAN_ISR2_FLAG = 0;
}
#endif
