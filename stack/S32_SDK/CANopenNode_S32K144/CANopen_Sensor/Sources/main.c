/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * THIS SOFTWARE IS PROVIDED BY NXP "AS IS" AND ANY EXPRESSED OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL NXP OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
/* ###################################################################
**     Filename    : main.c
**     Project     : CANopen_Sensor_s32k144
**     Processor   : S32K144_100
**     Version     : Driver 01.00
**     Compiler    : GNU C Compiler
**     Date/Time   : 2017-08-10, 11:11, # CodeGen: 0
**     Abstract    :
**         Main module.
**         This module contains user's application code.
**     Settings    :
**     Contents    :
**         No public methods
**
** ###################################################################*/
/*!
** @file main.c
** @version 01.00
** @brief
**         Main module.
**         This module contains user's application code.
*/
/*!
**  @addtogroup main_module main module documentation
**  @{
*/
/* MODULE main */


/* Including needed modules to compile this module/procedure */
#include "Cpu.h"
#include "clockMan1.h"
#include "sbc_uja11691.h"
#include "canCom1.h"
#include "dmaController1.h"
#include "lpit1.h"
#include "lpspiCom1.h"
#include "pin_mux.h"
#if CPU_INIT_CONFIG
  #include "Init_Config.h"
#endif

volatile int exit_code = 0;
/* User includes (#include below this line is not maintained by Processor Expert) */

#include <stdint.h>
#include <stdbool.h>
#include "CANopen.h"

/******************************************************************************
 * Definitions
 ******************************************************************************/

#define GPIO_PORT       PTD
#define LED0            15U    /*RED LED*/
#define LED1            16U    /*GREEN LED*/
#define LED2            0U     /*BLUE LED*/

#define BTN_GPIO        PTC
#define BTN1_PIN        13U
#define BTN2_PIN        12U
#define BTN_PORT        PORTC
#define BTN_PORT_IRQn   PORTC_IRQn

/* Use this define to specify if the application runs as control or sensor */
#define SENSOR_DEVICE

#if defined(UNIT_DEVICE)
    #define CANopen_NodeID        (1U)
#elif defined(SENSOR_DEVICE)
    #define CANopen_NodeID        (6U)          /* The ID of this device. A value between 1 and 127*/
#elif defined(CONTROL_DEVICE)
    #define CANopen_NodeID        (3U)
#endif

/* Definitions for CANopen device configurations */
#define TMR_TASK_INTERVAL   (1000)          /* Interval of LPIT IRQ in microseconds */
#define INCREMENT_1MS(var)  (var++)         /* Increment 1ms variable in tmrTask */

/* Global variables and objects */
volatile uint32_t   CO_timer1ms = 0U;   /* variable increments each millisecond */
volatile int16_t   temperature_sensor = 25U;   /* Sensor simulation variable, start at room temperature */

/******************************************************************************
 * Function prototypes
 ******************************************************************************/
void ClockInit(void);
void GPIOInit(void);
void SBCInit(void);
void LPITInit(void);
void ApplicationInit(void);
void InitError(void);
void buttonISR(void);

/******************************************************************************
 * Functions
 ******************************************************************************/

/*
 * @brief Function initializes CLOCK module
 */
void ClockInit(void)
{
    /* Initialize and configure clocks */
    CLOCK_SYS_Init(g_clockManConfigsArr, CLOCK_MANAGER_CONFIG_CNT,
                   g_clockManCallbacksArr, CLOCK_MANAGER_CALLBACK_CNT);
    CLOCK_SYS_UpdateConfiguration(0U, CLOCK_MANAGER_POLICY_FORCIBLE);
}

/*
 * @brief Function initializes PINS module
 */
void GPIOInit(void)
{
    /* Initialize pins */
    PINS_DRV_Init(NUM_OF_CONFIGURED_PINS, g_pin_mux_InitConfigArr);

    /* Output direction for LEDs */
    PINS_DRV_SetPinsDirection(GPIO_PORT, (1 << LED2) | (1 << LED1) | (1 << LED0));

    /* Set Output value LEDs */
    PINS_DRV_SetPins(GPIO_PORT, (1 << LED2) | (1 << LED1) | (1 << LED0));

    /* Setup button pin */
    PINS_DRV_SetPinsDirection(BTN_GPIO, ~((1 << BTN1_PIN)|(1 << BTN2_PIN)));

    /* Setup button pins interrupt */
    PINS_DRV_SetPinIntSel(BTN_PORT, BTN1_PIN, PORT_INT_RISING_EDGE);
    PINS_DRV_SetPinIntSel(BTN_PORT, BTN2_PIN, PORT_INT_RISING_EDGE);

    /* Install buttons ISR */
    INT_SYS_InstallHandler(BTN_PORT_IRQn, &buttonISR, NULL);

    /* Enable buttons interrupt */
    INT_SYS_EnableIRQ(BTN_PORT_IRQn);
}

/*
 * @brief Function initializes SBC module
 * This module is needed by the CAN to work properly
 */
void SBCInit(void)
{
    /* Initialize and configure the SBC */
    LPSPI_DRV_MasterInit(LPSPICOM1, &lpspiCom1State, &lpspiCom1_MasterConfig0);
    /* Initialize SBC */
    SBC_Init(&sbc_uja11691_InitConfig0, LPSPICOM1);
}

/*
 * @brief Function initializes LPIT module
 */
void LPITInit(void)
{
    /* Initialize and configure the LPIT */
    LPIT_DRV_Init(INST_LPIT1, &lpit1_InitConfig);
    /* Initialize LPIT */
    LPIT_DRV_InitChannel(INST_LPIT1, lpit1_ChnConfig0.triggerSelect, &lpit1_ChnConfig0);
    /* Start Channel Counter */
    LPIT_DRV_StartTimerChannels(INST_LPIT1, 1<<lpit1_ChnConfig0.triggerSelect);
}

/*
 * @brief Function which configures the board for this application
 */
void ApplicationInit(void)
{
    ClockInit();
    GPIOInit();
    SBCInit();
}

/*
 * @brief Function called if error appears in any initialization step
 */
void InitError(void)
{
    uint32_t i = 0U;

    PINS_DRV_SetPins(GPIO_PORT, (1 << LED2) | (1 << LED1) | (1 << LED0));
    while(1)
    {
        i = 1000000U;
        PINS_DRV_TogglePins(GPIO_PORT, (1 << LED0));
        while(i--);
    }
}

/*
 * @brief Function which handles CANOpen timer thread
 * The function runs at a constant interval of 1ms
 */
void LPIT0_Ch0_IRQHandler(void)
{
    /* Clear interrupt flag */
    LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1 << lpit1_ChnConfig0.triggerSelect));

    /* Timer thread */
    INCREMENT_1MS(CO_timer1ms);

    /*
     * Verify if the CAN module is running.
     * At bootup or if a communication reset command has been received it is disabled.
     */
    if(CO->CANmodule[0]->CANnormal) {
        bool_t syncWas;

        /* Process Sync and read inputs */
        syncWas = CO_process_SYNC_RPDO(CO, TMR_TASK_INTERVAL);

        /* Further I/O or nonblocking application code may go here. */

        /* Write outputs */
        CO_process_TPDO(CO, syncWas, TMR_TASK_INTERVAL);

        /* verify timer overflow (if the interrupt takes longer than 1ms to execute) */
        if(LPIT_DRV_GetInterruptFlagTimerChannels(INST_LPIT1, (1 << lpit1_ChnConfig0.triggerSelect)) != 0) {
            CO_errorReport(CO->em, CO_EM_ISR_TIMER_OVERFLOW, CO_EMC_SOFTWARE_INTERNAL, 0U);
            LPIT_DRV_ClearInterruptFlagTimerChannels(INST_LPIT1, (1 << lpit1_ChnConfig0.triggerSelect));
        }
    }
}

/*
 * Button interrupt handler
 * Used to simulate sensor temperature going down or up
 */
void buttonISR(void)
{
    /* Check if one of the buttons was pressed */
    uint32_t buttonsPressed = PINS_DRV_GetPortIntFlag(BTN_PORT) & ((1 << BTN1_PIN) | (1 << BTN2_PIN));

    switch (buttonsPressed)
    {
        case (1 << BTN1_PIN):
            /* Temperature is rising */
            temperature_sensor += 2;
            /* Clear interrupt flag */
            PINS_DRV_ClearPinIntFlagCmd(BTN_PORT, BTN1_PIN);
            break;
        case (1 << BTN2_PIN):
            /* Temperature is falling */
            temperature_sensor -= 2;
            /* Clear interrupt flag */
            PINS_DRV_ClearPinIntFlagCmd(BTN_PORT, BTN2_PIN);
            break;
        default:
            PINS_DRV_ClearPortIntFlagCmd(BTN_PORT);
            break;
    }
}

/*!
  \brief The main function for the project.
  \details The startup initialization sequence is the following:
 * - __start (startup asm routine)
 * - __init_hardware()
 * - main()
 *   - PE_low_level_init()
 *     - Common_Init()
 *     - Peripherals_Init()
*/
int main(void)
{
  /*** Processor Expert internal initialization. DON'T REMOVE THIS CODE!!! ***/
  #ifdef PEX_RTOS_INIT
    PEX_RTOS_INIT();                 /* Initialization of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of Processor Expert internal initialization.                    ***/

    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;

    /* Configure microcontroller. */
    ApplicationInit();

    /*
     * increase variable each startup. Variable is stored in EEPROM.
     * TODO: store variables in EEPROM. Now everything is stored in RAM
     */
    OD_powerOnCounter++;

    while(reset != CO_RESET_APP)
    {
    /* CANopen communication reset - initialize CANopen objects */
        CO_ReturnError_t err = CO_ERROR_NO;
        uint16_t timer1msPrevious = 0U;

        /* Verify, if OD structures have proper alignment of initial values */
        if(CO_OD_RAM.FirstWord != CO_OD_RAM.LastWord) InitError();
        if(CO_OD_EEPROM.FirstWord != CO_OD_EEPROM.LastWord) InitError();
        if(CO_OD_ROM.FirstWord != CO_OD_ROM.LastWord) InitError();

        /* disable CAN interrupts */
        INT_SYS_DisableIRQGlobal();

        /* initialize CANopen */
        err = CO_FLEXCAN_Init(INST_CANCOM1, &canCom1_State, &canCom1_InitConfig0, CANopen_NodeID);
        if(err != CO_ERROR_NO) InitError();

        err = CO_init(INST_CANCOM1, CANopen_NodeID, 500U);
        if(err != CO_ERROR_NO) InitError();

        /* Configure Timer interrupt function for execution every 1 millisecond */
        LPITInit();

        /* Configurations complete. Enable Interrupts */
        INT_SYS_EnableIRQGlobal();

        /* start CAN */
        CO_CANsetNormalMode(CO->CANmodule[0]);

        reset = CO_RESET_NOT;
        timer1msPrevious = CO_timer1ms;

        while(reset == CO_RESET_NOT)
        {
        /* loop for normal program execution */
            uint16_t timer1msCopy, timer1msDiff, timer1msNext;

            timer1msCopy = CO_timer1ms;
            timer1msDiff = timer1msCopy - timer1msPrevious;
            timer1msPrevious = timer1msCopy;

            /* CANopen process */
            reset = CO_process(CO, timer1msDiff, &timer1msNext);

            /* Nonblocking application code may go here. */

            /* copy the temperature into the OD variable for further processing, because OD entries cannot be volatile */
            OD_sensor_Temperature = temperature_sensor;

            /*Turn off LEDs*/
            PINS_DRV_SetPins(GPIO_PORT, ((1<<LED0) | (1<<LED1) |(1<<LED2)));

            switch (OD_unit_State)
            {
                case 0x00:  /* Cooler active */
                    PINS_DRV_TogglePins(GPIO_PORT, (1 << LED1));
                    break;
                case 0x01:  /* Heater active */
                    PINS_DRV_TogglePins(GPIO_PORT, ((1 << LED0) | (1 << LED1)));
                    break;
                case 0x02:  /* Unit Idle */
                    PINS_DRV_TogglePins(GPIO_PORT, ((1 << LED1) | (1 << LED2)));
                    break;
                default:
                    PINS_DRV_TogglePins(GPIO_PORT, (1 << LED0));
                    break;
            }
        }

        /* disable CAN as a reset communication or stop command was received */
        CO_CANmodule_disable(CO->CANmodule[0]);
    }

    /* program exit */
    /* stop threads */
    LPIT_DRV_Deinit(INST_LPIT1);

    /* delete objects from memory */
    CO_delete(INST_CANCOM1);

  /*** Don't write any code pass this line, or it will be deleted during code generation. ***/
  /*** RTOS startup code. Macro PEX_RTOS_START is defined by the RTOS component. DON'T MODIFY THIS CODE!!! ***/
  #ifdef PEX_RTOS_START
    PEX_RTOS_START();                  /* Startup of the selected RTOS. Macro is defined by the RTOS component. */
  #endif
  /*** End of RTOS startup code.  ***/
  /*** Processor Expert end of main routine. DON'T MODIFY THIS CODE!!! ***/
  for(;;) {
    if(exit_code != 0) {
      break;
    }
  }
  return exit_code;
  /*** Processor Expert end of main routine. DON'T WRITE CODE BELOW!!! ***/
} /*** End of main routine. DO NOT MODIFY THIS TEXT!!! ***/

/* END main */
/*!
** @}
*/
/*
** ###################################################################
**
**     This file was created by Processor Expert 10.1 [05.21]
**     for the Freescale S32K series of microcontrollers.
**
** ###################################################################
*/
