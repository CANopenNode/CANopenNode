/*
 * CO_driver.cpp
 *
 *  Created on: 8 Sep 2016 г.
 *      Author: a.smirnov
 *
 *      CANOpen CAN driver
 */

/** Includes *****************************************************************/
//#include <stdint.h>         // for 'int8_t' to 'uint64_t'
#include "stm32f10x.h"
#include "CO_driver.h"

//#include "task.h"

/** Defines and constants ****************************************************/
#define CAN_ERROR_CHECK_PERIOD_MS             100           // time interval for CAN module error check
#define CAN_WARNING_LIMIT                     95            // CAN module error counter warning limit (see RM0038)
#define CAN_ERROR_PASSIVE_LIMIT               127           // CAN module error counter error passive limit (see RM0038)

/** Private function prototypes **********************************************/

#ifdef __cplusplus
extern "C" {
#endif
#if CAN_NUM == 1
/**
 * @brief  This function handles CAN1_TX_IRQHandler
 * @param  None
 * @retval None
 */
void CAN1_TX_IRQHandler(void);

/**
 * @brief  This function handles CAN1_RX0_IRQHandler
 * @param  None
 * @retval None
 */
void CAN1_RX0_IRQHandler(void);

/**
 * @brief  This function handles CAN1_SCE_IRQHandler
 * @param  None
 * @retval None
 */
void CAN1_SCE_IRQHandler(void);
#endif

#if CAN_NUM == 2
/**
 * @brief  This function handles CAN2_TX_IRQHandler
 * @param  None
 * @retval None
 */
void CAN2_TX_IRQHandler(void);

/**
 * @brief  This function handles CAN2_RX0_IRQHandler
 * @param  None
 * @retval None
 */
void CAN2_RX0_IRQHandler(void);

/**
 * @brief  This function handles CAN2_SCE_IRQHandler
 * @param  None
 * @retval None
 */
void CAN2_SCE_IRQHandler(void);
#endif
#ifdef __cplusplus
}
#endif

/** Private typedef, enumeration, class **************************************/
/** Private functions ********************************************************/
/** Variables and objects ****************************************************/

cCO_Driver                  oCO_Driver;
sCANFilter                  xCO_CANFilter;

/** Class methods ************************************************************/

void cCO_Driver::vInit(void)    {

  acCANDriver::vInit();

  /** create OS objects ******/
  TaskHandle_t                xHandle;                            // task handle
  if (xTaskCreate(vCO_Driver_Task, ( char * ) CO_Driver_Task_name, 500, NULL, CO_Driver_Task_priority,
      &xHandle) != pdPASS)
    for(;;);
  nAddHandle(xHandle);

  return;
}//cCO_Driver::vInit ----------------------------------------------------------

eCANReturn cCO_Driver::vStart(const uint16_t nCANbitRateKB, const uint8_t nCANNum)   {

  if (nCANNum > 2)  return CAN_ERROR_ILLEGAL_ARGUMENT;

  GPIO_InitTypeDef        GPIO_InitStructure;
  CAN_InitTypeDef         CAN_InitStructure;
  CAN_FilterInitTypeDef   CAN_FilterInitStructure;
  NVIC_InitTypeDef        NVIC_InitStructure;
  uint32_t err;
  uint8_t i,j;

  // CAN configure
  CAN_StructInit(&CAN_InitStructure); // Struct init
  CAN_InitStructure.CAN_TTCM = DISABLE;
  CAN_InitStructure.CAN_ABOM = DISABLE;
  CAN_InitStructure.CAN_AWUM = DISABLE;
  CAN_InitStructure.CAN_NART = DISABLE;
  CAN_InitStructure.CAN_RFLM = DISABLE;
  CAN_InitStructure.CAN_TXFP = ENABLE;
  CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;
  CAN_InitStructure.CAN_SJW = CAN_SJW_1tq;
  CAN_InitStructure.CAN_BS1 = CAN_BS1_9tq;
  CAN_InitStructure.CAN_BS2 = CAN_BS2_8tq;

  switch (nCANbitRateKB) {
  case 1000:
    CAN_InitStructure.CAN_Prescaler = 2;
    break;
  case 500:
    CAN_InitStructure.CAN_Prescaler = 4;
    break;
  case 250:
    CAN_InitStructure.CAN_Prescaler = 8;
    break;
  case 125:
    CAN_InitStructure.CAN_Prescaler = 16;
    break;
  case 100:
    CAN_InitStructure.CAN_Prescaler = 20;
    break;
  case 50:
    CAN_InitStructure.CAN_Prescaler = 40;
    break;
  case 20:
    CAN_InitStructure.CAN_Prescaler = 100;
    break;
  case 10:
    CAN_InitStructure.CAN_Prescaler = 200;
    break;
  default:
    return CAN_ERROR_ILLEGAL_ARGUMENT;
    break;
  }//switch (nCANbitRateKB)

  switch (nCANNum) {
  case 1:

#if UNI_VERSION != 400

    NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX0_IRQn;
    // priority of interrupt using API functions must be greater then configMAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h  !!!
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = CAN1_TX_IRQn;
    // priority of interrupt using API functions must be greater then configMAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h  !!!
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = CAN1_SCE_IRQn;
    // priority of interrupt using API functions must be greater then configMAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h  !!!
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

#if UNI_VERSION == 33
    // enabling CAN1 chip (put PD2 low)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOD, GPIO_Pin_2);

    // GPIOD, and AFIO clocks enable
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOD, ENABLE);

    // Configure CAN1 RX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // Configure CAN1 TX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // Remap CAN1 GPIOs to PD0,PD1
    GPIO_PinRemapConfig(GPIO_Remap2_CAN1, ENABLE);
#endif

#if UNI_VERSION == 4
    // GPIOB, and AFIO clocks enable
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE);

    // Configure CAN1 RX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Configure CAN1 TX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Remap CAN1 GPIOs to PB8,PB9
    GPIO_PinRemapConfig(GPIO_Remap1_CAN1, ENABLE);
#endif

    // CAN1 Periph clocks enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    CAN_DeInit(CAN1);               // CAN1 register init
    CAN_Init(CAN1, &CAN_InitStructure);
    err = CAN1->ESR;
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_BUS_WARNING, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_RX_BUS_PASSIVE, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_TX_BUS_PASSIVE, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_TX_OFF, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_RX_OVERFLOW, err);

    // CAN1 filters init
    if (m_pCANFilter->m_nSize == 0)   {
      CAN_FilterInitStructure.CAN_FilterNumber = 1;
      CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
      CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
      CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
      CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
      CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
      CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
      CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_FIFO0;
      CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
      CAN_FilterInit(&CAN_FilterInitStructure);
    }
    else  {
      CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdList;
      CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;
      CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_FIFO0;
      CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
      for(i = 0; i <= m_pCANFilter->m_nSize / 4; i++)   {
        CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
        CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
        CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
        CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
        for(j = 0; j < 4; j++)   {
          if (i*4 + j < m_pCANFilter->m_nSize)  {
            switch (j)  {
            case 0:
              CAN_FilterInitStructure.CAN_FilterIdLow = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            case 1:
              CAN_FilterInitStructure.CAN_FilterMaskIdLow = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            case 2:
              CAN_FilterInitStructure.CAN_FilterIdHigh = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            case 3:
              CAN_FilterInitStructure.CAN_FilterMaskIdHigh = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            }//switch (j)
          }//if (i*4 + j < m_pCANFilter->m_nSize)
          else
            break;
        }//for(uint8_t j
        CAN_FilterInitStructure.CAN_FilterNumber = i+1;
        CAN_FilterInit(&CAN_FilterInitStructure);
      }//for(uint8_t i
    }//if-else (m_pCANFilter->m_nSize == 0)

    // CAN1 interrupt config
    CAN_ITConfig(CAN1, CAN_IT_TME, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_FF0, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_ERR, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_EWG, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_EPV, ENABLE);
    CAN_ITConfig(CAN1, CAN_IT_BOF, ENABLE);
#endif//#if UNI_VERSION != 400

    break;
  case 2:

    NVIC_InitStructure.NVIC_IRQChannel = CAN2_RX0_IRQn;
    // priority of interrupt using API functions must be greater then configMAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h  !!!
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = CAN2_TX_IRQn;
    // priority of interrupt using API functions must be greater then configMAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h  !!!
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = CAN2_SCE_IRQn;
    // priority of interrupt using API functions must be greater then configMAX_SYSCALL_INTERRUPT_PRIORITY in FreeRTOSConfig.h  !!!
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = CAN_IRQ_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

#if UNI_VERSION == 33
    // GPIOB, and AFIO clocks enable
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);    // disable JTAG
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // enabling CAN2 chip (put PB4 low)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOB, GPIO_Pin_4);

    // Configure CAN2 RX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Configure CAN2 TX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Remap CAN2 GPIOs to PB5,PB6
    GPIO_PinRemapConfig(GPIO_Remap_CAN2, ENABLE);
#endif

#if UNI_VERSION == 4
    // GPIOB, and AFIO clocks enable
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOB, ENABLE);

    // Configure CAN2 RX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Configure CAN2 TX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif

#if UNI_VERSION == 400
    // GPIOB, and AFIO clocks enable
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);    // disable JTAG
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // Configure CAN2 RX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Configure CAN2 TX pin
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Remap CAN2 GPIOs to PB5,PB6
    GPIO_PinRemapConfig(GPIO_Remap_CAN2, ENABLE);
#endif

    // CAN2 Periph clocks enable
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN2, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    CAN_DeInit(CAN2);          // CAN2 register init
    CAN_Init(CAN2, &CAN_InitStructure);
    err = CAN2->ESR;
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_BUS_WARNING, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_RX_BUS_PASSIVE, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_TX_BUS_PASSIVE, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_TX_OFF, err);
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_RX_OVERFLOW, err);

    // CAN2 filters init
    if (m_pCANFilter->m_nSize == 0) {
      CAN_FilterInitStructure.CAN_FilterNumber = 14;
      CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdMask;
      CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_32bit;
      CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
      CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
      CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
      CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
      CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_FIFO0;
      CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
      CAN_FilterInit(&CAN_FilterInitStructure);
    }
    else  {
      CAN_FilterInitStructure.CAN_FilterMode = CAN_FilterMode_IdList;
      CAN_FilterInitStructure.CAN_FilterScale = CAN_FilterScale_16bit;
      CAN_FilterInitStructure.CAN_FilterFIFOAssignment = CAN_FIFO0;
      CAN_FilterInitStructure.CAN_FilterActivation = ENABLE;
      for(i = 0; i <= m_pCANFilter->m_nSize / 4; i++)   {
        CAN_FilterInitStructure.CAN_FilterIdLow = 0x0000;
        CAN_FilterInitStructure.CAN_FilterMaskIdLow = 0x0000;
        CAN_FilterInitStructure.CAN_FilterIdHigh = 0x0000;
        CAN_FilterInitStructure.CAN_FilterMaskIdHigh = 0x0000;
        for(j = 0; j < 4; j++)   {
          if (i*4 + j < m_pCANFilter->m_nSize)  {
            switch (j)  {
            case 0:
              CAN_FilterInitStructure.CAN_FilterIdLow = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            case 1:
              CAN_FilterInitStructure.CAN_FilterMaskIdLow = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            case 2:
              CAN_FilterInitStructure.CAN_FilterIdHigh = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            case 3:
              CAN_FilterInitStructure.CAN_FilterMaskIdHigh = m_pCANFilter -> m_aCOBID[i*4 + j] << 5;
              break;
            }//switch (j)
          }//if (i*4 + j < xCAN_Filter.m_nSize)
          else break;
        }//for(uint8_t j
        CAN_FilterInitStructure.CAN_FilterNumber = i+14;
        CAN_FilterInit(&CAN_FilterInitStructure);
      }//for(uint8_t i
    }//if-else (m_pCANFilter->m_nSize == 0)

    // CAN2 interrupt config
    CAN_ITConfig(CAN2, CAN_IT_TME, ENABLE);
    CAN_ITConfig(CAN2, CAN_IT_FMP0, ENABLE);
    CAN_ITConfig(CAN2, CAN_IT_FF0, ENABLE);
    CAN_ITConfig(CAN2, CAN_IT_ERR, ENABLE);
    CAN_ITConfig(CAN2, CAN_IT_EWG, ENABLE);
    CAN_ITConfig(CAN2, CAN_IT_EPV, ENABLE);
    CAN_ITConfig(CAN2, CAN_IT_BOF, ENABLE);

    break;
  }//switch (nCANNum)

  acCANDriver::vStart(nCANbitRateKB, nCANNum);
  m_nState = CAN_STATE_ON;

  return CAN_ERROR_NO;
}//cCO_Driver::vStart ---------------------------------------------------------

bool cCO_Driver::bCANSend(sCANMsg xCANMsg)    {

  if (m_nState != CAN_STATE_ON)   return false;
  if ( (m_nCANNum != 1) && (m_nCANNum != 2) )   return false;

  bool          bResult;
  CAN_TypeDef*  pCAN;
  if (m_nCANNum == 1)
    pCAN = CAN1;
  else
    pCAN = CAN2;

  CAN_ITConfig(pCAN, CAN_IT_TME, DISABLE);    // disable TX interrupt
  if (!m_bIsTransmiting)    {           // not transmitting ?

    m_bIsTransmiting = true;

    CanTxMsg  xTxMsg;
    xTxMsg.StdId = xCANMsg.m_nStdId;
    xTxMsg.IDE = CAN_Id_Standard;
    xTxMsg.RTR = CAN_RTR_Data;
    xTxMsg.DLC = xCANMsg.m_nDLC;
    xTxMsg.Data[0] = xCANMsg.m_aData[0];
    xTxMsg.Data[1] = xCANMsg.m_aData[1];
    xTxMsg.Data[2] = xCANMsg.m_aData[2];
    xTxMsg.Data[3] = xCANMsg.m_aData[3];
    xTxMsg.Data[4] = xCANMsg.m_aData[4];
    xTxMsg.Data[5] = xCANMsg.m_aData[5];
    xTxMsg.Data[6] = xCANMsg.m_aData[6];
    xTxMsg.Data[7] = xCANMsg.m_aData[7];

    CAN_Transmit(pCAN, &xTxMsg);
    bResult = true;
  }
  else  {
    if (xQueueSendToBack(m_xQueueHandle_CANSend, &xCANMsg, 0) == errQUEUE_FULL)
      bResult = false;
    else
      bResult = true;
  }
  CAN_ITConfig(pCAN, CAN_IT_TME, ENABLE);
  return bResult;
}//cCO_Driver::bCANSend -------------------------------------------------------

void cCO_Driver::vCANVerifyErrors(void) const   {

  uint32_t      err;
  CAN_TypeDef*  pCAN;

  if ( (m_nCANNum != 1) && (m_nCANNum != 2) )   return;
  if (m_pCANError == NULL)  return;

  if (m_nCANNum == 1)   {
    err = CAN1->ESR;
    pCAN = CAN1;
  }
  else  {
    err = CAN2->ESR;
    pCAN = CAN2;
  }

  // CAN bus warning
  if (err & CAN_ESR_EWGF)
    m_pCANError->bSignalCANError(CAN_ERROR_BUS_WARNING, err);
  else
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_BUS_WARNING, err);

  //CAN bus passive
  if (CAN_GetReceiveErrorCounter(pCAN) > CAN_ERROR_PASSIVE_LIMIT)      // RX error passive ?
    m_pCANError->bSignalCANError(CAN_ERROR_RX_BUS_PASSIVE, err);
  else
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_RX_BUS_PASSIVE, err);

  if (CAN_GetLSBTransmitErrorCounter(pCAN) > CAN_ERROR_PASSIVE_LIMIT)      // TX error passive ?
    m_pCANError->bSignalCANError(CAN_ERROR_TX_BUS_PASSIVE, err);
  else
    m_pCANError->bSignalCANErrorReleased(CAN_ERROR_TX_BUS_PASSIVE, err);

  return;
}//cCO_Driver::vCANVerifyErrors -----------------------------------------------

void cCO_Driver::vStop(void)    {

  CAN_TypeDef*  pCAN;
  if (m_nCANNum == 1)
    pCAN = CAN1;
  else
    pCAN = CAN2;

  CAN_DeInit(pCAN);                               // reset CAN
  if (m_nCANNum == 2)
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN2, DISABLE);
  else  {
    // CAN1 can't be stopped because slave CAN2 may be in use!!
  }

  // interrupt disable
  CAN_ITConfig(pCAN, CAN_IT_TME, DISABLE);
  CAN_ITConfig(pCAN, CAN_IT_FMP0, DISABLE);
  CAN_ITConfig(pCAN, CAN_IT_FF0, DISABLE);
  CAN_ITConfig(pCAN, CAN_IT_ERR, DISABLE);
  CAN_ITConfig(pCAN, CAN_IT_EPV, DISABLE);
  CAN_ITConfig(pCAN, CAN_IT_EWG, DISABLE);
  CAN_ITConfig(pCAN, CAN_IT_BOF, DISABLE);

  xQueueReset(m_xQueueHandle_CANSend);      // clear queue
  m_nState = CAN_STATE_OFF;
  return;
}//cCO_Driver::vStop ----------------------------------------------------------

/** Exported functions *******************************************************/

void vCO_Driver_Task(void* pvParameters)  {
  for(;;)   {
    vTaskDelay(CAN_ERROR_CHECK_PERIOD_MS / portTICK_RATE_MS);
    if (oCO_Driver.m_nState == CAN_STATE_ON)
      oCO_Driver.vCANVerifyErrors();
  }
}//vCO_Driver_Task ------------------------------------------------------------

/** Interrupt handlers *******************************************************/

#if CAN_NUM == 1
void CAN1_TX_IRQHandler(void)  {

  sCANMsg             xCANMsg;
  CanTxMsg            xTxMsg;
  BaseType_t          xTaskWokenByReceive = pdFALSE;

  CAN_ITConfig(CAN1, CAN_IT_TME, DISABLE);
  if ( xQueueReceiveFromISR(oCO_Driver.m_xQueueHandle_CANSend, &xCANMsg, &xTaskWokenByReceive) == pdTRUE)   {

    oCO_Driver.m_bIsTransmiting = true;

    xTxMsg.StdId = xCANMsg.StdId;
    xTxMsg.IDE = CAN_Id_Standard;
    xTxMsg.RTR = CAN_RTR_Data;
    xTxMsg.DLC = xCANMsg.DLC;
    xTxMsg.Data[0] = xCANMsg.Data[0];
    xTxMsg.Data[1] = xCANMsg.Data[1];
    xTxMsg.Data[2] = xCANMsg.Data[2];
    xTxMsg.Data[3] = xCANMsg.Data[3];
    xTxMsg.Data[4] = xCANMsg.Data[4];
    xTxMsg.Data[5] = xCANMsg.Data[5];
    xTxMsg.Data[6] = xCANMsg.Data[6];
    xTxMsg.Data[7] = xCANMsg.Data[7];

    CAN_Transmit(CAN1, &xTxMsg);
    CAN_ITConfig(CAN1, CAN_IT_TME, ENABLE);
  }
  else  {
    oCO_Driver.m_bIsTransmiting = false;
  }

  return;
}//CAN1_TX_IRQHandler ---------------------------------------------------------

void CAN1_RX0_IRQHandler(void)  {

  CanRxMsg      xRxMsg;
  sCANMsg       xCANMsg;
  uint32_t      err = CAN1_BASE->ESR;

  // receive message and report if FIFO is full
  CAN_Receive(CAN1, CAN_FilterFIFO0, &xRxMsg);
  if (CAN_GetITStatus(CAN1, CAN_RF0R_FULL0) == SET ) {
    CAN_ClearITPendingBit(CAN1, CAN_RF0R_FULL0);
    if (oCO_Driver.m_pCANError != NULL)
      oCO_Driver.m_pCANError->bSignalCANErrorFromISR(CAN_ERROR_RX_OVERFLOW, err);
  }

  // send message to RX object if valid
  if ((xRxMsg.IDE == CAN_Id_Standard) && (xRxMsg.RTR == CAN_RTR_Data)) {

    xCANMsg.StdId = xRxMsg.StdId;
    xCANMsg.DLC = xRxMsg.DLC;
    xCANMsg.Data[0] = xRxMsg.Data[0];
    xCANMsg.Data[1] = xRxMsg.Data[1];
    xCANMsg.Data[2] = xRxMsg.Data[2];
    xCANMsg.Data[3] = xRxMsg.Data[3];
    xCANMsg.Data[4] = xRxMsg.Data[4];
    xCANMsg.Data[5] = xRxMsg.Data[5];
    xCANMsg.Data[6] = xRxMsg.Data[6];
    xCANMsg.Data[7] = xRxMsg.Data[7];

    if (xQueueSendToBackFromISR(oCO_Driver.m_pCANRX->m_xQueueHandle_CANReceive, &xCANMsg, 0)
        == errQUEUE_FULL)           // queue full ?
      oCO_Driver.m_pCANError->bSignalCANErrorFromISR(CAN_ERROR_RX_FORWARD, 0);

  }//if ((xRxMsg.IDE == CAN_Id_Standard) && (xRxMsg.RTR == CAN_RTR_Data))

  return;
}//CAN1_RX0_IRQHandler --------------------------------------------------------

void CAN1_SCE_IRQHandler(void)  {

  uint32_t      err = CAN1_BASE->ESR;
  eCANError     nCANError;

  // CAN bus warning
  if ( CAN_GetITStatus(CAN1, CAN_IT_EWG) == SET ) {
    CAN_ClearITPendingBit(CAN1, CAN_IT_EWG);
    nCANError = CAN_ERROR_BUS_WARNING;
  }

  //CAN bus passive
  if ( CAN_GetITStatus(CAN1, CAN_IT_EPV) == SET ) {
    CAN_ClearITPendingBit(CAN1, CAN_IT_EPV);
    if (CAN_GetReceiveErrorCounter(CAN1) > CAN_ERROR_PASSIVE_LIMIT)     // RX error passive ?
      nCANError = CAN_ERROR_RX_BUS_PASSIVE;
    if (CAN_GetLSBTransmitErrorCounter(CAN1) > CAN_ERROR_PASSIVE_LIMIT)   // TX error passive ?
      nCANError = CAN_ERROR_TX_BUS_PASSIVE;
  }

  // CAN TX bus off
  if ( CAN_GetITStatus(CAN1, CAN_IT_BOF) == SET ) {
    CAN_ClearITPendingBit(CAN1, CAN_IT_BOF);
    nCANError = CAN_ERROR_TX_OFF;
  }

  // report error
  if (oCO_Driver.m_pCANError != NULL)
    oCO_Driver.m_pCANError->bSignalCANErrorFromISR(nCANError, err);

  return;
}//CAN1_SCE_IRQHandler --------------------------------------------------------
#endif

#if CAN_NUM == 2
void CAN2_TX_IRQHandler(void)   {

  sCANMsg             xCANMsg;
  CanTxMsg            xTxMsg;
  BaseType_t          xTaskWokenByReceive = pdFALSE;

  CAN_ITConfig(CAN2, CAN_IT_TME, DISABLE);
  if ( xQueueReceiveFromISR(oCO_Driver.m_xQueueHandle_CANSend, &xCANMsg, &xTaskWokenByReceive) == pdTRUE)   {

    oCO_Driver.m_bIsTransmiting = true;

    xTxMsg.StdId = xCANMsg.m_nStdId;
    xTxMsg.IDE = CAN_Id_Standard;
    xTxMsg.RTR = CAN_RTR_Data;
    xTxMsg.DLC = xCANMsg.m_nDLC;
    xTxMsg.Data[0] = xCANMsg.m_aData[0];
    xTxMsg.Data[1] = xCANMsg.m_aData[1];
    xTxMsg.Data[2] = xCANMsg.m_aData[2];
    xTxMsg.Data[3] = xCANMsg.m_aData[3];
    xTxMsg.Data[4] = xCANMsg.m_aData[4];
    xTxMsg.Data[5] = xCANMsg.m_aData[5];
    xTxMsg.Data[6] = xCANMsg.m_aData[6];
    xTxMsg.Data[7] = xCANMsg.m_aData[7];

    CAN_Transmit(CAN2, &xTxMsg);
    CAN_ITConfig(CAN2, CAN_IT_TME, ENABLE);
  }
  else  {
    oCO_Driver.m_bIsTransmiting = false;
  }

  return;
}//CAN2_TX_IRQHandler ---------------------------------------------------------

void CAN2_RX0_IRQHandler(void)  {

  CanRxMsg      xRxMsg;
  sCANMsg       xCANMsg;
  uint32_t      err = CAN2->ESR;

  // receive message and report if FIFO is full
  CAN_Receive(CAN2, CAN_FilterFIFO0, &xRxMsg);
  if (CAN_GetITStatus(CAN2, CAN_RF0R_FULL0) == SET ) {
    CAN_ClearITPendingBit(CAN2, CAN_RF0R_FULL0);
    if (oCO_Driver.m_pCANError != NULL)
      oCO_Driver.m_pCANError->bSignalCANErrorFromISR(CAN_ERROR_RX_OVERFLOW, err);
  }

  // send message to RX object if valid
  if ((xRxMsg.IDE == CAN_Id_Standard) && (xRxMsg.RTR == CAN_RTR_Data)) {

    xCANMsg.m_nStdId = xRxMsg.StdId;
    xCANMsg.m_nDLC = xRxMsg.DLC;
    xCANMsg.m_aData[0] = xRxMsg.Data[0];
    xCANMsg.m_aData[1] = xRxMsg.Data[1];
    xCANMsg.m_aData[2] = xRxMsg.Data[2];
    xCANMsg.m_aData[3] = xRxMsg.Data[3];
    xCANMsg.m_aData[4] = xRxMsg.Data[4];
    xCANMsg.m_aData[5] = xRxMsg.Data[5];
    xCANMsg.m_aData[6] = xRxMsg.Data[6];
    xCANMsg.m_aData[7] = xRxMsg.Data[7];

    if (xQueueSendToBackFromISR(oCO_Driver.m_pCANRX->m_xQueueHandle_CANReceive, &xCANMsg, 0)
        == errQUEUE_FULL)           // queue full ?
      oCO_Driver.m_pCANError->bSignalCANErrorFromISR(CAN_ERROR_RX_FORWARD, 0);

  }//if ((xRxMsg.IDE == CAN_Id_Standard) && (xRxMsg.RTR == CAN_RTR_Data))

  return;
}//CAN2_RX0_IRQHandler --------------------------------------------------------

void CAN2_SCE_IRQHandler(void)  {

  uint32_t      err = CAN2->ESR;
  eCANError     nCANError;

  // CAN bus warning
  if ( CAN_GetITStatus(CAN2, CAN_IT_EWG) == SET ) {
    CAN_ClearITPendingBit(CAN2, CAN_IT_EWG);
    nCANError = CAN_ERROR_BUS_WARNING;
  }

  //CAN bus passive
  if ( CAN_GetITStatus(CAN2, CAN_IT_EPV) == SET ) {
    CAN_ClearITPendingBit(CAN2, CAN_IT_EPV);
    if (CAN_GetReceiveErrorCounter(CAN2) > CAN_ERROR_PASSIVE_LIMIT)     // RX error passive ?
      nCANError = CAN_ERROR_RX_BUS_PASSIVE;
    if (CAN_GetLSBTransmitErrorCounter(CAN2) > CAN_ERROR_PASSIVE_LIMIT)   // TX error passive ?
      nCANError = CAN_ERROR_TX_BUS_PASSIVE;
  }

  // CAN TX bus off
  if ( CAN_GetITStatus(CAN2, CAN_IT_BOF) == SET ) {
    CAN_ClearITPendingBit(CAN2, CAN_IT_BOF);
    nCANError = CAN_ERROR_TX_OFF;
  }

  // report error
  if (oCO_Driver.m_pCANError != NULL)
    oCO_Driver.m_pCANError->bSignalCANErrorFromISR(nCANError, err);

  return;
}//CAN2_SCE_IRQHandler --------------------------------------------------------
#endif
