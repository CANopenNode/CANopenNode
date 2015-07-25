/*--------------------------------------------------------------*/
/*            GENESYS ELECTRONICS DESIGN PTY LTD                */
/*--------------------------------------------------------------*/
/*                                                              */
/* FILE NAME :  CANOpenTask.h                                   */
/*                                                              */
/* REVISION NUMBER: 1.0                                         */
/*                                                              */
/* PURPOSE : This is the implementation of the CANOpen main task*/
/*                                                              */
/*                                                              */
/*--------------------------------------------------------------*/
/*                                                              */
/* AUTHOR:                                                      */
/*  Harshah Sagar                                               */
/*  GENESYS ELECTRONICS DESIGN Pty Ltd                          */
/*  Unit 5/33 Ryde Road                                         */
/*  Pymble NSW 2073                                             */
/*  Australia.                                                  */
/*  Telephone # 61-02-9496 8900                                 */
/*--------------------------------------------------------------*/
/* Revision Number :                                            */
/* Revision By     : Amit Hergass                               */
/* Date            : 30-Jun-2014                                */
/* Description     : Original Issue                             */
/*                                                              */
/* !--------------------------------------------------------!   */
/* ! Revisions to be entered in reverse chronological order !   */
/* !--------------------------------------------------------!   */
/*                                                              */
/*--------------------------------------------------------------*/

void CANOpenTask ( void *pvParameters );
void CAN_StackEnable(bool enableSwitch);
bool CAN_GetStackEnable(void);

