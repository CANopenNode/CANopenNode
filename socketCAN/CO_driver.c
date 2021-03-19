/*
 * Linux socketCAN interface for CANopenNode.
 *
 * @file        CO_driver.c
 * @ingroup     CO_driver
 * @author      Janez Paternoster, Martin Wagner
 * @copyright   2004 - 2015 Janez Paternoster, 2017 - 2020 Neuberger Gebaeudeautomation GmbH
 *
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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <linux/can/raw.h>
#include <linux/can/error.h>
#include <linux/net_tstamp.h>
#include <sys/socket.h>
#include <asm/socket.h>
#include <sys/eventfd.h>
#include <time.h>

#include "301/CO_driver.h"
#include "CO_error.h"

#ifndef CO_SINGLE_THREAD
pthread_mutex_t CO_EMCY_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t CO_OD_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#if CO_DRIVER_MULTI_INTERFACE == 0
static CO_ReturnError_t CO_CANmodule_addInterface(CO_CANmodule_t *CANmodule,
                                                  int can_ifindex);
#endif


#if CO_DRIVER_MULTI_INTERFACE > 0

static const uint32_t CO_INVALID_COB_ID = 0xffffffff;

/******************************************************************************/
void CO_CANsetIdentToIndex(
        uint32_t               *lookup,
        uint32_t                index,
        uint32_t                identNew,
        uint32_t                identCurrent)
{
    /* entry changed, remove old one */
    if (identCurrent<CO_CAN_MSG_SFF_MAX_COB_ID && identNew!=identCurrent) {
        lookup[identCurrent] = CO_INVALID_COB_ID;
    }

    /* check if this COB ID is part of the table */
    if (identNew > CO_CAN_MSG_SFF_MAX_COB_ID) {
        return;
    }

    /* Special case COB ID "0" -> valid value in *xArray[0] (CO_*CAN_NMT),
     * "entry unconfigured" for all others */
    if (identNew == 0) {
        if (index == 0) {
            lookup[0] = 0;
        }
    }
    else {
        lookup[identNew] = index;
    }
}


/******************************************************************************/
static uint32_t CO_CANgetIndexFromIdent(
        uint32_t               *lookup,
        uint32_t                ident)
{
    /* check if this COB ID is part of the table */
    if (ident > CO_CAN_MSG_SFF_MAX_COB_ID) {
        return CO_INVALID_COB_ID;
    }

    return lookup[ident];
}

#endif /* CO_DRIVER_MULTI_INTERFACE */


/** Disable socketCAN rx ******************************************************/
static CO_ReturnError_t disableRx(CO_CANmodule_t *CANmodule)
{
    uint32_t i;
    CO_ReturnError_t retval;

    /* insert a filter that doesn't match any messages */
    retval = CO_ERROR_NO;
    for (i = 0; i < CANmodule->CANinterfaceCount; i ++) {
        int ret = setsockopt(CANmodule->CANinterfaces[i].fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                         NULL, 0);
        if(ret < 0){
            log_printf(LOG_ERR, CAN_FILTER_FAILED,
                       CANmodule->CANinterfaces[i].ifName);
            log_printf(LOG_DEBUG, DBG_ERRNO, "setsockopt()");
            retval = CO_ERROR_SYSCALL;
        }
    }

    return retval;
}


/** Set up or update socketCAN rx filters *************************************/
static CO_ReturnError_t setRxFilters(CO_CANmodule_t *CANmodule)
{
    size_t i;
    int count;
    CO_ReturnError_t retval;

    struct can_filter rxFiltersCpy[CANmodule->rxSize];

    count = 0;
    /* remove unused entries ( id == 0 and mask == 0 ) as they would act as
     * "pass all" filter */
    for (i = 0; i < CANmodule->rxSize; i ++) {
        if ((CANmodule->rxFilter[i].can_id != 0) ||
            (CANmodule->rxFilter[i].can_mask != 0)) {

            rxFiltersCpy[count] = CANmodule->rxFilter[i];

            count ++;
        }
    }

    if (count == 0) {
        /* No filter is set, disable RX */
        return disableRx(CANmodule);
    }

    retval = CO_ERROR_NO;
    for (i = 0; i < CANmodule->CANinterfaceCount; i ++) {
      int ret = setsockopt(CANmodule->CANinterfaces[i].fd, SOL_CAN_RAW, CAN_RAW_FILTER,
                       rxFiltersCpy, sizeof(struct can_filter) * count);
      if(ret < 0){
          log_printf(LOG_ERR, CAN_FILTER_FAILED,
                     CANmodule->CANinterfaces[i].ifName);
          log_printf(LOG_DEBUG, DBG_ERRNO, "setsockopt()");
          retval = CO_ERROR_SYSCALL;
      }
    }

    return retval;
}


/******************************************************************************/
void CO_CANsetConfigurationMode(void *CANptr)
{
    (void)CANptr;
    /* Can't do anything because no reference to CANmodule_t is provided */
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule)
{
    CO_ReturnError_t ret;

    if(CANmodule != NULL) {
        CANmodule->CANnormal = false;
        ret = setRxFilters(CANmodule);
        if (ret == CO_ERROR_NO) {
            /* Put CAN module in normal mode */
            CANmodule->CANnormal = true;
        }
    }
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        void                   *CANptr,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    int32_t ret;
    uint16_t i;
    (void)CANbitRate;

    /* verify arguments */
    if(CANmodule==NULL || CANptr == NULL || rxArray==NULL || txArray==NULL) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_CANptrSocketCan_t *CANptrReal = (CO_CANptrSocketCan_t *)CANptr;

    /* Configure object variables */
    CANmodule->epoll_fd = CANptrReal->epoll_fd;
    CANmodule->CANinterfaces = NULL;
    CANmodule->CANinterfaceCount = 0;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->CANtxCount = 0;

#if CO_DRIVER_MULTI_INTERFACE > 0
    for (i = 0; i < CO_CAN_MSG_SFF_MAX_COB_ID; i++) {
        CANmodule->rxIdentToIndex[i] = CO_INVALID_COB_ID;
        CANmodule->txIdentToIndex[i] = CO_INVALID_COB_ID;
    }
#endif

    /* initialize socketCAN filters
     * CAN module filters will be configured with CO_CANrxBufferInit()
     * functions, called by separate CANopen init functions */
    CANmodule->rxFilter = calloc(CANmodule->rxSize, sizeof(struct can_filter));
    if(CANmodule->rxFilter == NULL){
        log_printf(LOG_DEBUG, DBG_ERRNO, "malloc()");
        return CO_ERROR_OUT_OF_MEMORY;
    }

    for(i=0U; i<rxSize; i++){
        rxArray[i].ident = 0U;
        rxArray[i].mask = 0xFFFFFFFFU;
        rxArray[i].object = NULL;
        rxArray[i].CANrx_callback = NULL;
        rxArray[i].can_ifindex = 0;
        rxArray[i].timestamp.tv_sec = 0;
        rxArray[i].timestamp.tv_nsec = 0;
    }

#if CO_DRIVER_MULTI_INTERFACE == 0
    /* add one interface */
    ret = CO_CANmodule_addInterface(CANmodule,
                                    CANptrReal->can_ifindex);
    if (ret != CO_ERROR_NO) {
        CO_CANmodule_disable(CANmodule);
        return ret;
    }
#endif
    return CO_ERROR_NO;
}


/** enable socketCAN *********************************************************/
#if CO_DRIVER_MULTI_INTERFACE == 0
static
#endif
CO_ReturnError_t CO_CANmodule_addInterface(CO_CANmodule_t *CANmodule,
                                           int can_ifindex)
{
    int32_t ret;
    int32_t tmp;
    int32_t bytes;
    char *ifName;
    socklen_t sLen;
    CO_CANinterface_t *interface;
    struct sockaddr_can sockAddr;
    struct epoll_event ev;
#if CO_DRIVER_ERROR_REPORTING > 0
    can_err_mask_t err_mask;
#endif

    if (CANmodule->CANnormal != false) {
        /* can't change config now! */
        return CO_ERROR_INVALID_STATE;
    }

    /* Add interface to interface list */
    CANmodule->CANinterfaceCount ++;
    CANmodule->CANinterfaces = realloc(CANmodule->CANinterfaces,
        ((CANmodule->CANinterfaceCount) * sizeof(*CANmodule->CANinterfaces)));
    if (CANmodule->CANinterfaces == NULL) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "malloc()");
        return CO_ERROR_OUT_OF_MEMORY;
    }
    interface = &CANmodule->CANinterfaces[CANmodule->CANinterfaceCount - 1];

    interface->can_ifindex = can_ifindex;
    ifName = if_indextoname(can_ifindex, interface->ifName);
    if (ifName == NULL) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "if_indextoname()");
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Create socket */
    interface->fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if(interface->fd < 0){
        log_printf(LOG_DEBUG, DBG_ERRNO, "socket(can)");
        return CO_ERROR_SYSCALL;
    }

    /* enable socket rx queue overflow detection */
    tmp = 1;
    ret = setsockopt(interface->fd, SOL_SOCKET, SO_RXQ_OVFL, &tmp, sizeof(tmp));
    if(ret < 0){
        log_printf(LOG_DEBUG, DBG_ERRNO, "setsockopt(ovfl)");
        return CO_ERROR_SYSCALL;
    }

    /* enable software time stamp mode (hardware timestamps do not work properly
     * on all devices)*/
    tmp = (SOF_TIMESTAMPING_SOFTWARE |
           SOF_TIMESTAMPING_RX_SOFTWARE);
    ret = setsockopt(interface->fd, SOL_SOCKET, SO_TIMESTAMPING, &tmp, sizeof(tmp));
    if (ret < 0) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "setsockopt(timestamping)");
        return CO_ERROR_SYSCALL;
    }

    //todo - modify rx buffer size? first one needs root
    //ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, (void *)&bytes, sLen);
    //ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&bytes, sLen);

    /* print socket rx buffer size in bytes (In my experience, the kernel reserves
     * around 450 bytes for each CAN message) */
    sLen = sizeof(bytes);
    getsockopt(interface->fd, SOL_SOCKET, SO_RCVBUF, (void *)&bytes, &sLen);
    if (sLen == sizeof(bytes)) {
        log_printf(LOG_INFO, CAN_SOCKET_BUF_SIZE, interface->ifName,
                   bytes / 446, bytes);
    }

    /* bind socket */
    memset(&sockAddr, 0, sizeof(sockAddr));
    sockAddr.can_family = AF_CAN;
    sockAddr.can_ifindex = can_ifindex;
    ret = bind(interface->fd, (struct sockaddr*)&sockAddr, sizeof(sockAddr));
    if(ret < 0){
        log_printf(LOG_ERR, CAN_BINDING_FAILED, interface->ifName);
        log_printf(LOG_DEBUG, DBG_ERRNO, "bind()");
        return CO_ERROR_SYSCALL;
    }

#if CO_DRIVER_ERROR_REPORTING > 0
    CO_CANerror_init(&interface->errorhandler, interface->fd, interface->ifName);
    /* set up error frame generation. What actually is available depends on your
     * CAN kernel driver */
#ifdef DEBUG
    err_mask = CAN_ERR_MASK; //enable ALL error frames
#else
    err_mask = CAN_ERR_ACK | CAN_ERR_CRTL | CAN_ERR_BUSOFF | CAN_ERR_BUSERROR;
#endif
    ret = setsockopt(interface->fd, SOL_CAN_RAW, CAN_RAW_ERR_FILTER, &err_mask,
                     sizeof(err_mask));
    if(ret < 0){
        log_printf(LOG_ERR, CAN_ERROR_FILTER_FAILED, interface->ifName);
        log_printf(LOG_DEBUG, DBG_ERRNO, "setsockopt(can err)");
        return CO_ERROR_SYSCALL;
    }
#endif /* CO_DRIVER_ERROR_REPORTING */

    /* Add socket to epoll */
    ev.events = EPOLLIN;
    ev.data.fd = interface->fd;
    ret = epoll_ctl(CANmodule->epoll_fd, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if(ret < 0){
        log_printf(LOG_DEBUG, DBG_ERRNO, "epoll_ctl(can)");
        return CO_ERROR_SYSCALL;
    }

    /* rx is started by calling #CO_CANsetNormalMode() */
    ret = disableRx(CANmodule);

    return ret;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule)
{
    uint32_t i;

    if (CANmodule == NULL) {
        return;
    }

    CANmodule->CANnormal = false;

    /* clear interfaces */
    for (i = 0; i < CANmodule->CANinterfaceCount; i++) {
        CO_CANinterface_t *interface = &CANmodule->CANinterfaces[i];

#if CO_DRIVER_ERROR_REPORTING > 0
        CO_CANerror_disable(&interface->errorhandler);
#endif

        epoll_ctl(CANmodule->epoll_fd, EPOLL_CTL_DEL, interface->fd, NULL);
        close(interface->fd);
        interface->fd = -1;
    }
    CANmodule->CANinterfaceCount = 0;
    if (CANmodule->CANinterfaces != NULL) {
        free(CANmodule->CANinterfaces);
    }
    CANmodule->CANinterfaces = NULL;

    if (CANmodule->rxFilter != NULL) {
        free(CANmodule->rxFilter);
    }
    CANmodule->rxFilter = NULL;
}


/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        bool_t                  rtr,
        void                   *object,
        void                  (*CANrx_callback)(void *object, void *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if((CANmodule!=NULL) && (index < CANmodule->rxSize)){
        CO_CANrx_t *buffer;

        /* buffer, which will be configured */
        buffer = &CANmodule->rxArray[index];

#if CO_DRIVER_MULTI_INTERFACE > 0
        CO_CANsetIdentToIndex(CANmodule->rxIdentToIndex, index, ident,
                                buffer->ident);
#endif

        /* Configure object variables */
        buffer->object = object;
        buffer->CANrx_callback = CANrx_callback;
        buffer->can_ifindex = 0;
        buffer->timestamp.tv_nsec = 0;
        buffer->timestamp.tv_sec = 0;

        /* CAN identifier and CAN mask, bit aligned with CAN module */
        buffer->ident = ident & CAN_SFF_MASK;
        if(rtr){
            buffer->ident |= CAN_RTR_FLAG;
        }
        buffer->mask = (mask & CAN_SFF_MASK) | CAN_EFF_FLAG | CAN_RTR_FLAG;

        /* Set CAN hardware module filter and mask. */
        CANmodule->rxFilter[index].can_id = buffer->ident;
        CANmodule->rxFilter[index].can_mask = buffer->mask;
        if(CANmodule->CANnormal){
            ret = setRxFilters(CANmodule);
        }
    }
    else {
        log_printf(LOG_DEBUG, DBG_CAN_RX_PARAM_FAILED, "illegal argument");
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}

#if CO_DRIVER_MULTI_INTERFACE > 0

/******************************************************************************/
bool_t CO_CANrxBuffer_getInterface(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        int                    *can_ifindexRx,
        struct timespec        *timestamp)
{
    CO_CANrx_t *buffer;

    if (CANmodule == NULL){
        return false;
    }

    const uint32_t index = CO_CANgetIndexFromIdent(CANmodule->rxIdentToIndex, ident);
    if ((index == CO_INVALID_COB_ID) || (index > CANmodule->rxSize)) {
      return false;
    }
    buffer = &CANmodule->rxArray[index];

    /* return values */
    if (can_ifindexRx != NULL) {
      *can_ifindexRx = buffer->can_ifindex;
    }
    if (timestamp != NULL) {
      *timestamp = buffer->timestamp;
    }
    if (buffer->can_ifindex != 0) {
      return true;
    }
    else {
      return false;
    }
}

#endif /* CO_DRIVER_MULTI_INTERFACE */


/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if((CANmodule != NULL) && (index < CANmodule->txSize)){
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

#if CO_DRIVER_MULTI_INTERFACE > 0
       CO_CANsetIdentToIndex(CANmodule->txIdentToIndex, index, ident, buffer->ident);
#endif

        buffer->can_ifindex = 0;

        /* CAN identifier and rtr */
        buffer->ident = ident & CAN_SFF_MASK;
        if(rtr){
            buffer->ident |= CAN_RTR_FLAG;
        }
        buffer->DLC = noOfBytes;
        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}

#if CO_DRIVER_MULTI_INTERFACE > 0

/******************************************************************************/
CO_ReturnError_t CO_CANtxBuffer_setInterface(
        CO_CANmodule_t         *CANmodule,
        uint16_t                ident,
        int                     can_ifindexTx)
{
    if (CANmodule != NULL) {
        uint32_t index;

        index = CO_CANgetIndexFromIdent(CANmodule->txIdentToIndex, ident);
        if ((index == CO_INVALID_COB_ID) || (index > CANmodule->txSize)) {
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }
        CANmodule->txArray[index].can_ifindex = can_ifindexTx;

        return CO_ERROR_NO;
    }
    return CO_ERROR_ILLEGAL_ARGUMENT;
}

#endif /* CO_DRIVER_MULTI_INTERFACE */
#if CO_DRIVER_MULTI_INTERFACE > 0

/* send CAN message ***********************************************************/
static CO_ReturnError_t CO_CANCheckSendInterface(
        CO_CANmodule_t         *CANmodule,
        CO_CANtx_t             *buffer,
        CO_CANinterface_t      *interface)
{
    CO_ReturnError_t err = CO_ERROR_NO;
#if CO_DRIVER_ERROR_REPORTING > 0
    CO_CANinterfaceState_t ifState;
#endif
    ssize_t n;

    if (CANmodule==NULL || interface==NULL || interface->fd < 0) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

#if CO_DRIVER_ERROR_REPORTING > 0
    ifState = CO_CANerror_txMsg(&interface->errorhandler);
    switch (ifState) {
        case CO_INTERFACE_ACTIVE:
            /* continue */
            break;
        case CO_INTERFACE_LISTEN_ONLY:
            /* silently drop message */
            return CO_ERROR_NO;
        default:
            return CO_ERROR_INVALID_STATE;
    }
#endif

    do {
        errno = 0;
        n = send(interface->fd, buffer, CAN_MTU, MSG_DONTWAIT);
        if (errno == EINTR) {
            /* try again */
            continue;
        }
        else if (errno == EAGAIN) {
            /* socket queue full */
            break;
        }
        else if (errno == ENOBUFS) {
            /* socketCAN doesn't support blocking write. You can wait here for
             * a few hundred us and then try again */
#if CO_DRIVER_ERROR_REPORTING > 0
            interface->errorhandler.CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
#endif
            return CO_ERROR_TX_BUSY;
        }
        else if (n != CAN_MTU) {
            break;
        }
    } while (errno != 0);

    if(n != CAN_MTU){
#if CO_DRIVER_ERROR_REPORTING > 0
        interface->errorhandler.CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
#endif
        log_printf(LOG_ERR, DBG_CAN_TX_FAILED, buffer->ident, interface->ifName);
        log_printf(LOG_DEBUG, DBG_ERRNO, "send()");
        err = CO_ERROR_TX_OVERFLOW;
    }

    return err;
}


/*
 * The same as #CO_CANsend(), but ensures that there is enough space remaining
 * in the driver for more important messages.
 *
 * The default threshold is 50%, or at least 1 message buffer. If sending
 * would violate those limits, #CO_ERROR_TX_OVERFLOW is returned and the
 * message will not be sent.
 *
 * (It is not in header, because usage of CO_CANCheckSend() is unclear.)
 *
 * @param CANmodule This object.
 * @param buffer Pointer to transmit buffer, returned by CO_CANtxBufferInit().
 * Data bytes must be written in buffer before function call.
 *
 * @return #CO_ReturnError_t: CO_ERROR_NO, CO_ERROR_TX_OVERFLOW, CO_ERROR_TX_BUSY or
 * CO_ERROR_TX_PDO_WINDOW (Synchronous TPDO is outside window).
 */
CO_ReturnError_t CO_CANCheckSend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer);


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err;
    err = CO_CANCheckSend(CANmodule, buffer);
    if (err == CO_ERROR_TX_BUSY) {
        /* send doesn't have "busy" */
        log_printf(LOG_ERR, DBG_CAN_TX_FAILED, buffer->ident, "CANx");
        log_printf(LOG_DEBUG, DBG_ERRNO, "send()");
        err = CO_ERROR_TX_OVERFLOW;
    }
    return err;
}


/******************************************************************************/
CO_ReturnError_t CO_CANCheckSend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    uint32_t i;
    CO_ReturnError_t err = CO_ERROR_NO;

    /* check on which interfaces to send this messages */
    for (i = 0; i < CANmodule->CANinterfaceCount; i++) {
        CO_CANinterface_t *interface = &CANmodule->CANinterfaces[i];

        if ((buffer->can_ifindex == 0) ||
            buffer->can_ifindex == interface->can_ifindex) {

            CO_ReturnError_t tmp;

            /* match, use this one */
            tmp = CO_CANCheckSendInterface(CANmodule, buffer, interface);
            if (tmp) {
                /* only last error is returned to callee */
                err = tmp;
            }
        }
    }

    return err;
}

#warning CO_CANsend() is outdated for CO_DRIVER_MULTI_INTERFACE > 0

#endif /* CO_DRIVER_MULTI_INTERFACE > 0 */


#if CO_DRIVER_MULTI_INTERFACE == 0

/* Change handling of tx buffer full in CO_CANsend(). Use CO_CANtx_t->bufferFull
 * flag. Re-transmit undelivered message inside CO_CANmodule_process(). */
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer)
{
    CO_ReturnError_t err = CO_ERROR_NO;

    if (CANmodule==NULL || buffer==NULL || CANmodule->CANinterfaceCount==0) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    CO_CANinterface_t *interface = &CANmodule->CANinterfaces[0];
    if (interface == NULL || interface->fd < 0) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Verify overflow */
    if(buffer->bufferFull){
#if CO_DRIVER_ERROR_REPORTING > 0
        interface->errorhandler.CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
#endif
        log_printf(LOG_ERR, DBG_CAN_TX_FAILED, buffer->ident, interface->ifName);
        err = CO_ERROR_TX_OVERFLOW;
    }

    errno = 0;
    ssize_t n = send(interface->fd, buffer, CAN_MTU, MSG_DONTWAIT);
    if (errno == 0 && n == CAN_MTU) {
        /* success */
        if (buffer->bufferFull) {
            buffer->bufferFull = false;
            CANmodule->CANtxCount--;
        }
    }
    else if (errno == EINTR || errno == EAGAIN || errno == ENOBUFS) {
        /* Send failed, message will be re-sent by CO_CANmodule_process() */
        if (!buffer->bufferFull) {
            buffer->bufferFull = true;
            CANmodule->CANtxCount++;
        }
        err = CO_ERROR_TX_BUSY;
    }
    else {
        /* Unknown error */
        log_printf(LOG_DEBUG, DBG_ERRNO, "send()");
#if CO_DRIVER_ERROR_REPORTING > 0
        interface->errorhandler.CANerrorStatus |= CO_CAN_ERRTX_OVERFLOW;
#endif
        err = CO_ERROR_SYSCALL;
    }

    return err;
}

#endif /* CO_DRIVER_MULTI_INTERFACE == 0 */


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    (void)CANmodule;
    /* Messages are either written to the socket queue or dropped */
}


/******************************************************************************/
void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
    if (CANmodule == NULL || CANmodule->CANinterfaceCount == 0) return;

#if CO_DRIVER_ERROR_REPORTING > 0
  /* socketCAN doesn't support microcontroller-like error counters. If an
   * error has occured, a special can message is created by the driver and
   * received by the application like a regular message.
   * Therefore, error counter evaluation is included in rx function.
   * Here we just copy evaluated CANerrorStatus from the first CAN interface. */

    CANmodule->CANerrorStatus =
        CANmodule->CANinterfaces[0].errorhandler.CANerrorStatus;
#endif

#if CO_DRIVER_MULTI_INTERFACE == 0
    /* recall CO_CANsend(), if message was unsent before */
    if (CANmodule->CANtxCount > 0) {
        bool_t found = false;

        for (uint16_t i = 0; i < CANmodule->txSize; i++) {
            CO_CANtx_t *buffer = &CANmodule->txArray[i];

            if (buffer->bufferFull) {
                buffer->bufferFull = false;
                CANmodule->CANtxCount--;
                CO_CANsend(CANmodule, buffer);
                found = true;
                break;
            }
        }

        if (!found) {
            CANmodule->CANtxCount = 0;
        }
    }
#endif /* CO_DRIVER_MULTI_INTERFACE == 0 */
}


/* Read CAN message from socket and verify some errors ************************/
static CO_ReturnError_t CO_CANread(
        CO_CANmodule_t         *CANmodule,
        CO_CANinterface_t      *interface,
        struct can_frame       *msg,        /* CAN message, return value */
        struct timespec        *timestamp)  /* timestamp of CAN message, return value */
{
    int32_t n;
    uint32_t dropped;
    /* recvmsg - like read, but generates statistics about the socket
     * example in berlios candump.c */
    struct iovec iov;
    struct msghdr msghdr;
    char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(dropped))];
    struct cmsghdr *cmsg;

    iov.iov_base = msg;
    iov.iov_len = sizeof(*msg);

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &iov;
    msghdr.msg_iovlen = 1;
    msghdr.msg_control = &ctrlmsg;
    msghdr.msg_controllen = sizeof(ctrlmsg);
    msghdr.msg_flags = 0;

    n = recvmsg(interface->fd, &msghdr, 0);
    if (n != CAN_MTU) {
#if CO_DRIVER_ERROR_REPORTING > 0
        interface->errorhandler.CANerrorStatus |= CO_CAN_ERRRX_OVERFLOW;
#endif
        log_printf(LOG_DEBUG, DBG_CAN_RX_FAILED, interface->ifName);
        log_printf(LOG_DEBUG, DBG_ERRNO, "recvmsg()");
        return CO_ERROR_SYSCALL;
    }

    /* check for rx queue overflow, get rx time */
    for (cmsg = CMSG_FIRSTHDR(&msghdr);
         cmsg && (cmsg->cmsg_level == SOL_SOCKET);
         cmsg = CMSG_NXTHDR(&msghdr, cmsg)) {
        if (cmsg->cmsg_type == SO_TIMESTAMPING) {
            /* this is system time, not monotonic time! */
            *timestamp = ((struct timespec*)CMSG_DATA(cmsg))[0];
        }
        else if (cmsg->cmsg_type == SO_RXQ_OVFL) {
            dropped = *(uint32_t*)CMSG_DATA(cmsg);
            if (dropped > CANmodule->rxDropCount) {
#if CO_DRIVER_ERROR_REPORTING > 0
                interface->errorhandler.CANerrorStatus |= CO_CAN_ERRRX_OVERFLOW;
#endif
                log_printf(LOG_ERR, CAN_RX_SOCKET_QUEUE_OVERFLOW,
                           interface->ifName, dropped);
            }
            CANmodule->rxDropCount = dropped;
            //todo use this info!
        }
    }

    return CO_ERROR_NO;
}


/* find msg inside rxArray and call corresponding CANrx_callback **************/
static int32_t CO_CANrxMsg(                 /* return index of received message in rxArray or -1 */
        CO_CANmodule_t        *CANmodule,
        struct can_frame      *msg,         /* CAN message input */
        CO_CANrxMsg_t         *buffer)      /* If not NULL, msg will be copied to buffer */
{
    int32_t retval;
    const CO_CANrxMsg_t *rcvMsg;  /* pointer to received message in CAN module */
    uint16_t index;               /* index of received message */
    CO_CANrx_t *rcvMsgObj = NULL; /* receive message object from CO_CANmodule_t object. */
    bool_t msgMatched = false;

    /* CANopenNode can message is binary compatible to the socketCAN one, except
     * for extension flags */
    msg->can_id &= CAN_EFF_MASK;
    rcvMsg = (CO_CANrxMsg_t *)msg;

    /* Message has been received. Search rxArray from CANmodule for the
     * same CAN-ID. */
    rcvMsgObj = &CANmodule->rxArray[0];
    for (index = 0; index < CANmodule->rxSize; index ++) {
        if(((rcvMsg->ident ^ rcvMsgObj->ident) & rcvMsgObj->mask) == 0U){
            msgMatched = true;
            break;
        }
        rcvMsgObj++;
    }
    if(msgMatched) {
        /* Call specific function, which will process the message */
        if ((rcvMsgObj != NULL) && (rcvMsgObj->CANrx_callback != NULL)){
            rcvMsgObj->CANrx_callback(rcvMsgObj->object, (void *)rcvMsg);
        }
        /* return message */
        if (buffer != NULL) {
            memcpy(buffer, rcvMsg, sizeof(*buffer));
        }
        retval = index;
    }
    else {
        retval = -1;
    }

    return retval;
}


/******************************************************************************/
bool_t CO_CANrxFromEpoll(CO_CANmodule_t *CANmodule,
                         struct epoll_event *ev,
                         CO_CANrxMsg_t *buffer,
                         int32_t *msgIndex)
{
    if (CANmodule == NULL || ev == NULL || CANmodule->CANinterfaceCount == 0) {
        return false;
    }

    /* Verify for epoll events in CAN socket */
    for (uint32_t i = 0; i < CANmodule->CANinterfaceCount; i ++) {
        CO_CANinterface_t *interface = &CANmodule->CANinterfaces[i];

        if (ev->data.fd == interface->fd) {
            if ((ev->events & (EPOLLERR | EPOLLHUP)) != 0) {
                struct can_frame msg;
                /* epoll detected close/error on socket. Try to pull event */
                errno = 0;
                recv(ev->data.fd, &msg, sizeof(msg), MSG_DONTWAIT);
                log_printf(LOG_DEBUG, DBG_CAN_RX_EPOLL,
                           ev->events, strerror(errno));
            }
            else if ((ev->events & EPOLLIN) != 0) {
                struct can_frame msg;
                struct timespec timestamp;

                /* get message */
                CO_ReturnError_t err = CO_CANread(CANmodule, interface,
                                                  &msg, &timestamp);

                if(err == CO_ERROR_NO && CANmodule->CANnormal) {

                    if (msg.can_id & CAN_ERR_FLAG) {
                        /* error msg */
#if CO_DRIVER_ERROR_REPORTING > 0
                        CO_CANerror_rxMsgError(&interface->errorhandler, &msg);
#endif
                    }
                    else {
                        /* data msg */
#if CO_DRIVER_ERROR_REPORTING > 0
                        /* clear listenOnly and noackCounter if necessary */
                        CO_CANerror_rxMsg(&interface->errorhandler);
#endif
                        int32_t idx = CO_CANrxMsg(CANmodule, &msg, buffer);
                        if (idx > -1) {
                            /* Store message info */
                            CANmodule->rxArray[idx].timestamp = timestamp;
                            CANmodule->rxArray[idx].can_ifindex =
                                                        interface->can_ifindex;
                        }
                        if (msgIndex != NULL) {
                            *msgIndex = idx;
                        }
                    }
                }
            }
            else {
                log_printf(LOG_DEBUG, DBG_EPOLL_UNKNOWN,
                           ev->events, ev->data.fd);
            }
            return true;
        } /* if (ev->data.fd == interface->fd) */
    }
    return false;
}
