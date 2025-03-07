/*
 * Device identificators for CANopenNode.
 *
 * @file        CO_identificators.c
 * @author      --
 * @copyright   2021 --
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

#include "CO_identificators.h"
#include "301/CO_ODinterface.h"
#include "CO_ident_defs.h"
#include "OD.h"

/*
 * Custom function for reading OD object _Manufacturer device name_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_1008(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead) {
    if (stream == NULL || buf == NULL || countRead == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    OD_size_t len = strlen(CO_DEVICE_NAME);
    if (len > count)
        len = count;
    memcpy(buf, CO_DEVICE_NAME, len);

    *countRead = stream->dataLength = len;
    return ODR_OK;
}

/*
 * Custom function for reading OD object _Manufacturer hardware version_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_1009(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead) {
    if (stream == NULL || buf == NULL || countRead == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    OD_size_t len = strlen(CO_HW_VERSION);
    if (len > count)
        len = count;
    memcpy(buf, CO_HW_VERSION, len);

    *countRead = stream->dataLength = len;
    return ODR_OK;
}

/*
 * Custom function for reading OD object _Manufacturer software version_
 *
 * For more information see file CO_ODinterface.h, OD_IO_t.
 */
static ODR_t OD_read_100A(OD_stream_t* stream, void* buf, OD_size_t count, OD_size_t* countRead) {
    if (stream == NULL || buf == NULL || countRead == NULL) {
        return ODR_DEV_INCOMPAT;
    }

    OD_size_t len = strlen(CO_SW_VERSION);
    if (len > count)
        len = count;
    memcpy(buf, CO_SW_VERSION, len);

    *countRead = stream->dataLength = len;
    return ODR_OK;
}

/* Extensions for OD objects */
OD_extension_t OD_1008_extension = {.object = NULL, .read = OD_read_1008, .write = NULL};
OD_extension_t OD_1009_extension = {.object = NULL, .read = OD_read_1009, .write = NULL};
OD_extension_t OD_100A_extension = {.object = NULL, .read = OD_read_100A, .write = NULL};

/******************************************************************************/
void CO_identificators_init(uint16_t* bitRate, uint8_t* nodeId) {
    /* Set initial CAN bitRate and CANopen nodeId. May be configured by LSS. */
    if (*bitRate == 0)
        *bitRate = CO_BITRATE_INITIAL;
    if (*nodeId == 0)
        *nodeId = CO_NODE_ID_INITIAL;

    /* Initialize OD objects 0x1008, 0x1009 and 0x100A. These are device
     * specific strings. Object dictionary has no default value for strings,
     * so custom read functions are required for objects to work. */
    OD_extension_init(OD_ENTRY_H1008_manufacturerDeviceName, &OD_1008_extension);
    OD_extension_init(OD_ENTRY_H1009_manufacturerHardwareVersion, &OD_1009_extension);
    OD_extension_init(OD_ENTRY_H100A_manufacturerSoftwareVersion, &OD_100A_extension);

    /* Write values directly to the Object Dictionary Identity object. */
    OD_PERSIST_COMM.x1018_identity.vendor_ID = CO_IDENTITY_VENDOR_ID;
    OD_PERSIST_COMM.x1018_identity.productCode = CO_IDENTITY_PRODUCT_CODE;
    OD_PERSIST_COMM.x1018_identity.revisionNumber = CO_IDENTITY_REVISION_NUMBER;
    OD_PERSIST_COMM.x1018_identity.serialNumber = CO_IDENTITY_SERIAL_NUMBER;
}
