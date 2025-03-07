/**
 * Example access to the Object Dictionary variable of type domain.
 *
 * @file        domainDemo.h
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

#ifndef domainDemo_H
#define domainDemo_H

#include "301/CO_ODinterface.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize domainDemo object.
 *
 * @param OD_domainDemo Object Dictionary entry for domainDemo.
 * @param [out] errInfo If OD entry is erroneous, errInfo indicates its index.
 *
 * @return @ref CO_ReturnError_t CO_ERROR_NO in case of success.
 */
CO_ReturnError_t domainDemo_init(OD_entry_t* OD_domainDemo, uint32_t* errInfo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* domainDemo_H */
