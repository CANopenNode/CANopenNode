/*
 * CAN module object for generic microcontroller.
 *
 * This file is a template for other microcontrollers.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster
 * @copyright   2004 - 2020 Janez Paternoster
 *
 * This file is part of <https://github.com/CANopenNode/CANopenNode>, a CANopen Stack.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and limitations under the License.
 */

#include "301/CO_driver.h"
#include "301/CO_driver_target.h"

#include "can_interface.h"
#include "debug.h"

void CO_CANsetConfigurationMode(void* CANptr) {
    /* Put CAN module in configuration mode */
    CanInterface* can = (CanInterface*)CANptr;
    if (!can->setMode(CAN_MODE::_MODE_CONFIG)) {
        LOG_ERROR("CO_CANsetConfigurationMode: setMode failed\n");
    } else {
        LOG_INFO("CO_CANsetConfigurationMode: setMode success\n");
    }
}

void CO_CANsetNormalMode(CO_CANmodule_t* CANmodule) {
    /* Put CAN module in normal mode */
    CanInterface* can = (CanInterface*)CANmodule->CANptr;
    if (!can->setMode(CAN_MODE::_MCP_NORMAL)) {
        LOG_ERROR("CO_CANsetNormalMode: setMode failed\n");
    }

    CANmodule->CANnormal = true;
    LOG_INFO("CO_CANsetNormalMode: setMode success\n");
}

CO_ReturnError_t CO_CANmodule_init(CO_CANmodule_t* CANmodule,
                                   void* CANptr,
                                   CO_CANrx_t rxArray[],
                                   uint16_t rxSize,
                                   CO_CANtx_t txArray[],
                                   uint16_t txSize,
                                   uint16_t CANbitRate) {
    uint16_t i;

    /* verify arguments */
    if (CANmodule == NULL || rxArray == NULL || txArray == NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    CANmodule->CANptr = CANptr;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->useCANrxFilters = (rxSize <= 32U) ? true : false; /* microcontroller dependent */
    CANmodule->bufferInhibitFlag = false;
    CANmodule->firstCANtxMessage = true;
    CANmodule->CANtxCount = 0U;
    CANmodule->errOld = 0U;

    for (i = 0U; i < rxSize; i++) {
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
    }
    for (i = 0U; i < txSize; i++) {
        txArray[i].bufferFull = false;
    }

    /* Configure CAN module registers */
    CanInterface* can = (CanInterface*)CANptr;
    const CAN_SPEED speed = getSpeed(CANbitRate);
    if (!can->setup(speed)) {
        LOG_ERROR("CO_CANmodule_init: setup failed\n");
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure CAN timing */

    /* Configure CAN module hardware filters */
    if (CANmodule->useCANrxFilters) {
        /* CAN module filters are used, they will be configured with */
        /* CO_CANrxBufferInit() functions, called by separate CANopen */
        /* init functions. */
        /* Configure all masks so, that received message must match filter */
    } else {
        /* CAN module filters are not used, all messages with standard 11-bit */
        /* identifier will be received */
        /* Configure mask 0 so, that all messages with standard identifier are accepted */
    }

    /* configure CAN interrupt registers */

    return CO_ERROR_NO;
}

void CO_CANmodule_disable(CO_CANmodule_t* CANmodule) {
    if (CANmodule != NULL) {
        /* turn off the module */
    }
}

CO_ReturnError_t CO_CANrxBufferInit(CO_CANmodule_t* CANmodule,
                                    uint16_t index,
                                    uint16_t ident,
                                    uint16_t mask,
                                    bool_t rtr,
                                    void* object,
                                    void (*CANrx_callback)(void* object, void* message)) {
    CO_ReturnError_t ret = CO_ERROR_NO;
    LOG_DEBUG("0x%x - 0x%x - 0x%x - 0x%x\n", index, ident, mask, rtr);

    if ((CANmodule != NULL) && (object != NULL) && (CANrx_callback != NULL) && (index < CANmodule->rxSize)) {
        /* buffer, which will be configured */
        CO_CANrx_t* buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;

        /* CAN identifier and CAN mask, bit aligned with CAN module. Different on different microcontrollers. */
        buffer->ident = ident & 0x07FFU;
        if (rtr) {
            buffer->ident |= 0x0800U;
        }
        buffer->mask = (mask & 0x07FFU) | 0x0800U;

        LOG_DEBUG("CO_CANrxBufferInit: ident: 0x%x - mask: 0x%x\n", buffer->ident, buffer->mask);

        /* Set CAN hardware module filter and mask. */
        if (CANmodule->useCANrxFilters) {
            LOG_DEBUG("CO_CANrxBufferInit: useCANrxFilters\n");
        }
    } else {
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
        LOG_DEBUG("CO_CANrxBufferInit: illegal argument ?\n");
    }

    return ret;
}

CO_CANtx_t* CO_CANtxBufferInit(CO_CANmodule_t* CANmodule,
                               uint16_t index,
                               uint16_t ident,
                               bool_t rtr,
                               uint8_t noOfBytes,
                               bool_t syncFlag) {
    CO_CANtx_t* buffer = NULL;

    if ((CANmodule != NULL) && (index < CANmodule->txSize)) {
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];
        buffer->ident = (uint32_t)ident & 0x07FFU;
        if (rtr) {
            buffer->ident |= 0x0800U;
        }
        buffer->DLC = noOfBytes;
        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}

CO_ReturnError_t CO_CANsend(CO_CANmodule_t* CANmodule, CO_CANtx_t* buffer) {
    CO_ReturnError_t err = CO_ERROR_NO;

    /* Verify overflow */
    if (buffer->bufferFull) {
        LOG_WARNING("CO_CANsend: buffer full\n");
        if (!CANmodule->firstCANtxMessage) {
            /* don't set error, if bootup message is still on buffers */
            CANmodule->CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
        }
        err = CO_ERROR_TX_OVERFLOW;
    }

    CO_LOCK_CAN_SEND(CANmodule);
    /* if CAN TX buffer is free, copy message to it */
    CanInterface* can = (CanInterface*)CANmodule->CANptr;
    const CanOpenMsg msg(buffer->ident, buffer->data, buffer->DLC);
    // DEBUG("Ident: 0x%x - DLC: %d", buffer->ident, buffer->DLC);
    // for (uint8_t i = 0; i < buffer->DLC; i++) {
    // DEBUG(" 0x%x", buffer->data[i]);
    // }
    // DEBUG("\r\n");

    if (1 && CANmodule->CANtxCount == 0) {
        // LOG_DEBUG("CO_CANsend\n");
        if (can->send(msg)) {
            LOG_DEBUG("CO_CANsend: sent - NodeID: 0x%x - Fx: 0x%x - Len: 0x%x", msg.getNodeId(), msg.getFunctionCode(),
                      msg.getLen());
            CANmodule->bufferInhibitFlag = buffer->syncFlag;
        } else {
            LOG_ERROR("CO_CANsend: failed - CobID: 0x%x NodeID: 0x%x - Fx: 0x%x - Len: 0x%x", msg.getCobId(),
                      msg.getNodeId(), msg.getFunctionCode(), msg.getLen());
        }
        /* copy message and txRequest */
    }
    /* if no buffer is free, message will be sent by interrupt */
    else {
        LOG_WARNING("CO_CANsend: buffer full\n");
        buffer->bufferFull = true;
        CANmodule->CANtxCount++;
        // can->send(msg);
    }
    CO_UNLOCK_CAN_SEND(CANmodule);

    return err;
}

void CO_CANclearPendingSyncPDOs(CO_CANmodule_t* CANmodule) {
    uint32_t tpdoDeleted = 0U;

    CO_LOCK_CAN_SEND(CANmodule);
    /* Abort message from CAN module, if there is synchronous TPDO.
     * Take special care with this functionality. */
    if (/* messageIsOnCanBuffer && */ CANmodule->bufferInhibitFlag) {
        /* clear TXREQ */
        CANmodule->bufferInhibitFlag = false;
        tpdoDeleted = 1U;
    }
    /* delete also pending synchronous TPDOs in TX buffers */
    if (CANmodule->CANtxCount != 0U) {
        uint16_t i;
        CO_CANtx_t* buffer = &CANmodule->txArray[0];
        for (i = CANmodule->txSize; i > 0U; i--) {
            if (buffer->bufferFull) {
                if (buffer->syncFlag) {
                    buffer->bufferFull = false;
                    CANmodule->CANtxCount--;
                    tpdoDeleted = 2U;
                }
            }
            buffer++;
        }
    }
    CO_UNLOCK_CAN_SEND(CANmodule);

    if (tpdoDeleted != 0U) {
        CANmodule->CANerrorStatus |= CO_CAN_ERRTX_PDO_LATE;
    }
}

/* Get error counters from the module. If necessary, function may use different way to determine errors. */
static uint16_t rxErrors = 0, txErrors = 0, overflow = 0;

void CO_CANmodule_process(CO_CANmodule_t* CANmodule) {
    uint32_t err;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | overflow;

    if (CANmodule->errOld != err) {
        uint16_t status = CANmodule->CANerrorStatus;

        CANmodule->errOld = err;

        if (txErrors >= 256U) {
            /* bus off */
            status |= CO_CAN_ERRTX_BUS_OFF;
        } else {
            /* recalculate CANerrorStatus, first clear some flags */
            status &= 0xFFFF ^ (CO_CAN_ERRTX_BUS_OFF | CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE |
                                CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE);

            /* rx bus warning or passive */
            if (rxErrors >= 128) {
                status |= CO_CAN_ERRRX_WARNING | CO_CAN_ERRRX_PASSIVE;
            } else if (rxErrors >= 96) {
                status |= CO_CAN_ERRRX_WARNING;
            }

            /* tx bus warning or passive */
            if (txErrors >= 128) {
                status |= CO_CAN_ERRTX_WARNING | CO_CAN_ERRTX_PASSIVE;
            } else if (txErrors >= 96) {
                status |= CO_CAN_ERRTX_WARNING;
            }

            /* if not tx passive clear also overflow */
            if ((status & CO_CAN_ERRTX_PASSIVE) == 0) {
                status &= 0xFFFF ^ CO_CAN_ERRTX_OVERFLOW;
            }
        }

        if (overflow != 0) {
            /* CAN RX bus overflow */
            status |= CO_CAN_ERRRX_OVERFLOW;
        }

        CANmodule->CANerrorStatus = status;
    }
}

void CO_CANinterrupt(CO_CANmodule_t* CANmodule) {
    /* receive interrupt */
    CanInterface* can = (CanInterface*)CANmodule->CANptr;
    if (can->read()) {
        CO_CANrxMsg_t rcvMsg;      /* pointer to received message in CAN module */
        uint16_t index;            /* index of received message */
        uint32_t rcvMsgIdent;      /* identifier of the received message */
        CO_CANrx_t* buffer = NULL; /* receive message buffer from CO_CANmodule_t object. */
        bool_t msgMatched = false;

        const CanOpenMsg msg = can->getCanOpenMsg();
        const uint32_t id = msg.getCobId();
        const uint8_t len = msg.getLen();
        rcvMsg.ident = id;
        rcvMsg.DLC = len;
        // DEBUG("MSG received: id 0x%x - len %d", rcvMsg.ident, rcvMsg.DLC);
        for (uint8_t i = 0; i < len; i++) {
            rcvMsg.data[i] = msg.getData(i);
            // DEBUG(" %d", rcvMsg.data[i]);
        }
        // DEBUG("\r\n");
        LOG_DEBUG("MSG received: id 0x%x - len %d\n", rcvMsg.ident, rcvMsg.DLC);
        rcvMsgIdent = rcvMsg.ident;

        /* CAN module filters are not used, message with any standard 11-bit identifier */
        /* has been received. Search rxArray form CANmodule for the same CAN-ID. */
        buffer = &CANmodule->rxArray[0];
        for (index = 0; index < CANmodule->rxSize; index++) {
            // LOG_DEBUG("Buffer: 0x%x - 0x%x - 0x%x\n", buffer->ident, buffer->mask, buffer->object);
            if (((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U) {
                msgMatched = true;
                break;
            }
            buffer++;
        }

        /* Call specific function, which will process the message */
        if (msgMatched && (buffer != NULL) && (buffer->CANrx_callback != NULL)) {
            LOG_DEBUG("Message: 0x%x - 0x%x - 0x%x\n", rcvMsg.ident, buffer->ident, buffer->mask);
            buffer->CANrx_callback(buffer->object, (void*)&rcvMsg);
        }

        /* Clear interrupt flag */
    }

    // /* transmit interrupt */
    // else if (0) {
    //     /* Clear interrupt flag */

    //     /* First CAN message (bootup) was sent successfully */
    //     CANmodule->firstCANtxMessage = false;
    //     /* clear flag from previous message */
    //     CANmodule->bufferInhibitFlag = false;
    //     /* Are there any new messages waiting to be send */
    //     if (CANmodule->CANtxCount > 0U) {
    //         uint16_t i; /* index of transmitting message */

    //         /* first buffer */
    //         CO_CANtx_t* buffer = &CANmodule->txArray[0];
    //         /* search through whole array of pointers to transmit message buffers. */
    //         for (i = CANmodule->txSize; i > 0U; i--) {
    //             /* if message buffer is full, send it. */
    //             if (buffer->bufferFull) {
    //                 buffer->bufferFull = false;
    //                 CANmodule->CANtxCount--;

    //                 /* Copy message to CAN buffer */
    //                 CANmodule->bufferInhibitFlag = buffer->syncFlag;
    //                 /* canSend... */
    //                 break; /* exit for loop */
    //             }
    //             buffer++;
    //         } /* end of for loop */

    //         /* Clear counter if no more messages */
    //         if (i == 0U) {
    //             CANmodule->CANtxCount = 0U;
    //         }
    //     }
    // } else {
    //     /* some other interrupt reason */
    // }
}
