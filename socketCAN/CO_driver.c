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
#include <sys/epoll.h>
#include <time.h>

#include "301/CO_driver.h"
#include "CO_error.h"


pthread_mutex_t CO_EMCY_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t CO_OD_mutex = PTHREAD_MUTEX_INITIALIZER;

#if CO_DRIVER_MULTI_INTERFACE == 0
static CO_ReturnError_t CO_CANmodule_addInterface(CO_CANmodule_t *CANmodule, const void *CANptr);
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
    int ret;
    uint32_t i;
    CO_ReturnError_t retval;

    /* insert a filter that doesn't match any messages */
    retval = CO_ERROR_NO;
    for (i = 0; i < CANmodule->CANinterfaceCount; i ++) {
        ret = setsockopt(CANmodule->CANinterfaces[i].fd, SOL_CAN_RAW, CAN_RAW_FILTER,
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
    int ret;
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
      ret = setsockopt(CANmodule->CANinterfaces[i].fd, SOL_CAN_RAW, CAN_RAW_FILTER,
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

    CANmodule->CANnormal = false;

    if(CANmodule != NULL) {
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
    struct epoll_event ev;
    (void)CANbitRate;

    /* verify arguments */
    if(CANmodule==NULL || rxArray==NULL || txArray==NULL){
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Create epoll FD */
    CANmodule->fdEpoll = epoll_create(1);
    if(CANmodule->fdEpoll < 0){
        log_printf(LOG_DEBUG, DBG_ERRNO, "epoll_create()");
        CO_CANmodule_disable(CANmodule);
        return CO_ERROR_SYSCALL;
    }

    /* Create notification event */
    CANmodule->fdEvent = eventfd(0, EFD_NONBLOCK);
    if (CANmodule->fdEvent < 0) {
        log_printf(LOG_DEBUG, DBG_ERRNO, "eventfd");
        CO_CANmodule_disable(CANmodule);
        return CO_ERROR_OUT_OF_MEMORY;
    }
    /* ...and add it to epoll */
    ev.events = EPOLLIN;
    ev.data.fd = CANmodule->fdEvent;
    ret = epoll_ctl(CANmodule->fdEpoll, EPOLL_CTL_ADD, ev.data.fd, &ev);
    if(ret < 0){
        log_printf(LOG_DEBUG, DBG_ERRNO, "epoll_ctl(eventfd)");
        CO_CANmodule_disable(CANmodule);
        return CO_ERROR_SYSCALL;
    }

    /* Configure object variables */
    CANmodule->CANinterfaces = NULL;
    CANmodule->CANinterfaceCount = 0;
    CANmodule->rxArray = rxArray;
    CANmodule->rxSize = rxSize;
    CANmodule->txArray = txArray;
    CANmodule->txSize = txSize;
    CANmodule->CANerrorStatus = 0;
    CANmodule->CANnormal = false;
    CANmodule->fdTimerRead = -1;
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
        rxArray[i].CANptr = NULL;
        rxArray[i].timestamp.tv_sec = 0;
        rxArray[i].timestamp.tv_nsec = 0;
    }

#if CO_DRIVER_MULTI_INTERFACE == 0
    /* add one interface */
    ret = CO_CANmodule_addInterface(CANmodule, CANptr);
    if (ret != CO_ERROR_NO) {
        CO_CANmodule_disable(CANmodule);
    }
#else
    ret = CO_ERROR_NO;
#endif
    return ret;
}


/** enable socketCAN *********************************************************/
#if CO_DRIVER_MULTI_INTERFACE == 0
static
#endif
CO_ReturnError_t CO_CANmodule_addInterface(
        CO_CANmodule_t         *CANmodule,
        const void             *CANptr)
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

    interface->CANptr = CANptr;
    ifName = if_indextoname((uintptr_t)interface->CANptr, interface->ifName);
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
    sockAddr.can_ifindex = (uintptr_t)interface->CANptr;
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
    ret = epoll_ctl(CANmodule->fdEpoll, EPOLL_CTL_ADD, ev.data.fd, &ev);
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
    struct timespec wait;

    if (CANmodule == NULL) {
        return;
    }

    /* clear interfaces */
    for (i = 0; i < CANmodule->CANinterfaceCount; i++) {
        CO_CANinterface_t *interface = &CANmodule->CANinterfaces[i];

#if CO_DRIVER_ERROR_REPORTING > 0
        CO_CANerror_disable(&interface->errorhandler);
#endif

        epoll_ctl(CANmodule->fdEpoll, EPOLL_CTL_DEL, interface->fd, NULL);
        close(interface->fd);
        interface->fd = -1;
    }
    if (CANmodule->CANinterfaces != NULL) {
        free(CANmodule->CANinterfaces);
    }
    CANmodule->CANinterfaceCount = 0;

    /* cancel rx */
    if (CANmodule->fdEvent != -1) {
        uint64_t u = 1;
        ssize_t s;
        s = write(CANmodule->fdEvent, &u, sizeof(uint64_t));
        if (s != sizeof(uint64_t)) {
            log_printf(LOG_DEBUG, DBG_ERRNO, "write()");
        }
        /* give some time for delivery */
        wait.tv_sec = 0;
        wait.tv_nsec = 50 /* ms */ * 1000000;
        nanosleep(&wait, NULL);
        close(CANmodule->fdEvent);
    }

    if (CANmodule->fdEpoll >= 0) {
        close(CANmodule->fdEpoll);
    }
    CANmodule->fdEpoll = -1;

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
        buffer->CANptr = NULL;
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
        const void            **const CANptrRx,
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
    if (CANptrRx != NULL) {
      *CANptrRx = buffer->CANptr;
    }
    if (timestamp != NULL) {
      *timestamp = buffer->timestamp;
    }
    if (buffer->CANptr != NULL) {
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

        buffer->CANptr = NULL;

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
        const void             *CANptrTx)
{
    if (CANmodule != NULL) {
        uint32_t index;

        index = CO_CANgetIndexFromIdent(CANmodule->txIdentToIndex, ident);
        if ((index == CO_INVALID_COB_ID) || (index > CANmodule->txSize)) {
            return CO_ERROR_PARAMETERS;
        }
        CANmodule->txArray[index].CANptr = CANptrTx;

        return CO_ERROR_NO;
    }
    return CO_ERROR_PARAMETERS;
}

#endif /* CO_DRIVER_MULTI_INTERFACE */

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
        return CO_ERROR_PARAMETERS;
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

        if ((buffer->CANptr == NULL) ||
            buffer->CANptr == interface->CANptr) {

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


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule)
{
    (void)CANmodule;
    /* Messages are either written to the socket queue or dropped */
}


/******************************************************************************/
void CO_CANmodule_process(CO_CANmodule_t *CANmodule)
{
  /* socketCAN doesn't support microcontroller-like error counters. If an
   * error has occured, a special can message is created by the driver and
   * received by the application like a regular message.
   * Therefore, error counter evaluation is included in rx function.
   * Here we just copy evaluated CANerrorStatus from the first CAN interface. */

#if CO_DRIVER_ERROR_REPORTING > 0
    if (CANmodule->CANinterfaceCount > 0) {
        CANmodule->CANerrorStatus =
            CANmodule->CANinterfaces[0].errorhandler.CANerrorStatus;
    }
#endif
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
int32_t CO_CANrxWait(CO_CANmodule_t *CANmodule, int fdTimer, CO_CANrxMsg_t *buffer)
{
    int32_t retval;
    int32_t ret;
    const void *CANptr = NULL;
    CO_ReturnError_t err;
    CO_CANinterface_t *interface = NULL;
    struct epoll_event ev[1];
    struct can_frame msg;
    struct timespec timestamp;

    if (CANmodule==NULL || CANmodule->CANinterfaceCount==0) {
        return -1;
    }

    if (fdTimer>=0 && fdTimer!=CANmodule->fdTimerRead) {
        /* new timer, timer changed */
        epoll_ctl(CANmodule->fdEpoll, EPOLL_CTL_DEL, CANmodule->fdTimerRead, NULL);
        ev[0].events = EPOLLIN;
        ev[0].data.fd = fdTimer;
        ret = epoll_ctl(CANmodule->fdEpoll, EPOLL_CTL_ADD, ev[0].data.fd, &ev[0]);
        if(ret < 0){
            return -1;
        }
        CANmodule->fdTimerRead = fdTimer;
    }

    /*
     * blocking read using epoll
     */
    do {
        errno = 0;
        ret = epoll_wait(CANmodule->fdEpoll, ev, sizeof(ev) / sizeof(ev[0]), -1);
        if (errno == EINTR) {
            /* try again */
            continue;
        }
        else if (ret < 0) {
            /* epoll failed */
            return -1;
        }
        else if ((ev[0].events & (EPOLLERR | EPOLLHUP)) != 0) {
            /* epoll detected close/error on socket. Try to pull event */
            errno = 0;
            recv(ev[0].data.fd, &msg, sizeof(msg), MSG_DONTWAIT);
            log_printf(LOG_DEBUG, DBG_CAN_RX_EPOLL, ev[0].events, strerror(errno));
            continue;
        }
        else if ((ev[0].events & EPOLLIN) != 0) {
            /* one of the sockets is ready */
            if ((ev[0].data.fd == CANmodule->fdEvent) ||
                (ev[0].data.fd == fdTimer)) {
                /* timer or notification event */
                return -1;
            }
            else {
                /* CAN socket */
                uint32_t i;

                for (i = 0; i < CANmodule->CANinterfaceCount; i ++) {
                    interface = &CANmodule->CANinterfaces[i];

                    if (ev[0].data.fd == interface->fd) {
                        /* get interface handle */
                        CANptr = interface->CANptr;
                        /* get message */
                        err = CO_CANread(CANmodule, interface, &msg, &timestamp);
                        if (err != CO_ERROR_NO) {
                            return -1;
                        }
                        /* no need to continue search */
                        break;
                    }
                }
            }
        }
    } while (errno != 0);

    /*
     * evaluate Rx
     */
    retval = -1;
    if(CANmodule->CANnormal){

        if (msg.can_id & CAN_ERR_FLAG) {
            /* error msg */
#if CO_DRIVER_ERROR_REPORTING > 0
            CO_CANerror_rxMsgError(&interface->errorhandler, &msg);
#endif
        }
        else {
            /* data msg */
            int32_t msgIndex;

#if CO_DRIVER_ERROR_REPORTING > 0
            /* clear listenOnly and noackCounter if necessary */
            CO_CANerror_rxMsg(&interface->errorhandler);
#endif

            msgIndex = CO_CANrxMsg(CANmodule, &msg, buffer);
            if (msgIndex > -1) {
                /* Store message info */
                CANmodule->rxArray[msgIndex].timestamp = timestamp;
                CANmodule->rxArray[msgIndex].CANptr = CANptr;
            }
            retval = msgIndex;
        }
    }
    return retval;
}
