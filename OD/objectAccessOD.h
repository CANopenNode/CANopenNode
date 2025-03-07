/**
 * Example object oriented access to the Object Dictionary variable.
 *
 * @file        objectAccessOD.h
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

#ifndef objectAccessOD_H
#define objectAccessOD_H

#include "301/CO_ODinterface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Object declaration for objectAccessOD.
 */
typedef struct {
    OD_extension_t OD_demoRecord_extension; /**< Extension for OD object */
    OD_IO_t io_average;                     /**< object for read access to sub-OD variable average*/
    int64_t* i64;                           /**< Pointer to variable in object dictionary */
    uint64_t* u64;                          /**< Pointer to variable in object dictionary */
    float32_t* r32;                         /**< Pointer to variable in object dictionary */
    float64_t* r64;                         /**< Pointer to variable in object dictionary */
    /** Variable initialised from OD sub-entry 'Parameter with default value' */
    uint32_t internalParameter;
} objectAccessOD_t;

/**
 * Initialize objectAccessOD object.
 *
 * @param thisObj This object will be initialized.
 * @param OD_demoRecord Object Dictionary entry for demoRecord.
 * @param [out] errInfo If OD entry is erroneous, errInfo indicates its index.
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO in case of success.
 */
CO_ReturnError_t objectAccessOD_init(objectAccessOD_t* thisObj, OD_entry_t* OD_demoRecord, uint32_t* errInfo);

/**
 * Read "average" variable from Object Dictionary
 *
 * This is a demonstration of extended OD variable. OD variable is not accessed
 * from memory location, because it does not exist. Average is calculated from
 * internal values, so function access is necessary. "read" function specified
 * by OD_extension_init() is called. For "read" function to use,
 * "OD_IO_t io_average" structure has been initialized before. If
 * objectAccessOD_readAverage() is used from mainline, it has to be protected
 * with CO_LOCK_OD / CO_UNLOCK_OD macros as every access to OD variable from
 * mainline.
 *
 * @param thisObj This object contains access information to "average" parameter
 *
 * @return Value of the parameter.
 */
static inline float64_t objectAccessOD_readAverage(objectAccessOD_t* thisObj) {
    float64_t average = 0;
    OD_size_t countRd;

    ODR_t odRet = thisObj->io_average.read(&thisObj->io_average.stream, &average, sizeof(average), &countRd);

    (void)odRet;
    (void)countRd; /* unused */

    return average;
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* objectAccessOD_H */
