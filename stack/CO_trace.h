/**
 * CANopen trace interface.
 *
 * @file        CO_trace.h
 * @ingroup     CO_trace
 * @author      Janez Paternoster
 * @copyright   2016 Janez Paternoster
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


#ifndef CO_TRACE_H
#define CO_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "CO_driver.h"
#include "CO_SDO.h"


/**
 * @defgroup CO_trace Trace
 * @ingroup CO_CANopen
 * @{
 *
 * CANopen trace for recording variables over time.
 *
 * In embedded systems there is often a need to monitor some variables over time.
 * Results are then displayed on graph, similar as in oscilloscope.
 *
 * CANopen trace is a configurable object, accessible via CANopen Object
 * Dictionary, which records chosen variable over time. It generates a curve,
 * which can be read via SDO and can then be displayed in a graph.
 *
 * CO_trace_process() runs in 1 ms intervals and monitors one variable. If it
 * changes, it makes a record with timestamp into circular buffer. When trace is
 * accessed by CANopen SDO object, it reads latest points from the the circular
 * buffer, prints a SVG curve into string and sends it as a SDO response. If a
 * SDO request was received from the same device, then no traffic occupies CAN
 * network.
 */


/**
 * Start index of traceConfig and Trace objects in Object Dictionary.
 */
#ifndef OD_INDEX_TRACE_CONFIG
#define OD_INDEX_TRACE_CONFIG   0x2301
#define OD_INDEX_TRACE          0x2401
#endif


/**
 *  structure for reading variables and printing points for specific data type.
 */
typedef struct {
    /** Function pointer for getting the value from OD variable. **/
    int32_t (*pGetValue) (void *OD_variable);
    /** Function pointer for printing the start point to trace.plot */
    uint32_t (*printPointStart)(char *s, uint32_t size, uint32_t timeStamp, int32_t value);
    /** Function pointer for printing the point to trace.plot */
    uint32_t (*printPoint)(char *s, uint32_t size, uint32_t timeStamp, int32_t value);
    /** Function pointer for printing the end point to trace.plot */
    uint32_t (*printPointEnd)(char *s, uint32_t size, uint32_t timeStamp, int32_t value);
} CO_trace_dataType_t;


/**
 * Trace object.
 */
typedef struct {
    bool_t              enabled;        /**< True, if trace is enabled. */
    CO_SDO_t           *SDO;            /**< From CO_trace_init(). */
    uint32_t           *timeBuffer;     /**< From CO_trace_init(). */
    int32_t            *valueBuffer;    /**< From CO_trace_init(). */
    uint32_t            bufferSize;     /**< From CO_trace_init(). */
    volatile uint32_t   writePtr;       /**< Location in buffer, which will be next written. */
    volatile uint32_t   readPtr;        /**< Location in buffer, which will be next read. */
    uint32_t            lastTimeStamp;  /**< Last time stamp. If zero, then last point contains last timestamp. */
    void               *OD_variable;    /**< Pointer to variable, which is monitored */
    const CO_trace_dataType_t *dt;      /**< Data type specific function pointers. **/
    int32_t             valuePrev;      /**< Previous value of value. */
    uint32_t           *map;            /**< From CO_trace_init(). */
    uint8_t            *format;         /**< From CO_trace_init(). */
    int32_t            *value;          /**< From CO_trace_init(). */
    int32_t            *minValue;       /**< From CO_trace_init(). */
    int32_t            *maxValue;       /**< From CO_trace_init(). */
    uint32_t           *triggerTime;    /**< From CO_trace_init(). */
    uint8_t            *trigger;        /**< From CO_trace_init(). */
    int32_t            *threshold;      /**< From CO_trace_init(). */
} CO_trace_t;


/**
 * Initialize trace object.
 *
 * Function must be called in the communication reset section.
 *
 * @param trace This object will be initialized.
 * @param SDO SDO server object.
 * @param enabled Is trace enabled.
 * @param timeBuffer Memory block for storing time records.
 * @param valueBuffer Memory block for storing value records.
 * @param bufferSize Size of the above buffers.
 * @param map Map to variable in Object Dictionary, which will be monitored. Same structure as in PDO.
 * @param Format Format of the plot. If first bit is 1, above variable is unsigned. For more info see Object Dictionary.
 * @param trigger If different than zero, trigger time is recorded, when variable goes through threshold.
 * @param threshold Used with trigger.
 * @param value Pointer to variable, which will show last value of the variable.
 * @param minValue Pointer to variable, which will show minimum value of the variable.
 * @param maxValue Pointer to variable, which will show maximum value of the variable.
 * @param triggerTime Pointer to variable, which will show last trigger time of the variable.
 * @param idx_OD_traceConfig Index in Object Dictionary.
 * @param idx_OD_trace Index in Object Dictionary.
 *
 * @return 0 on success, -1 on error.
 */
void CO_trace_init(
        CO_trace_t             *trace,
        CO_SDO_t               *SDO,
        uint8_t                 enabled,
        uint32_t               *timeBuffer,
        int32_t                *valueBuffer,
        uint32_t                bufferSize,
        uint32_t               *map,
        uint8_t                *format,
        uint8_t                *trigger,
        int32_t                *threshold,
        int32_t                *value,
        int32_t                *minValue,
        int32_t                *maxValue,
        uint32_t               *triggerTime,
        uint16_t                idx_OD_traceConfig,
        uint16_t                idx_OD_trace);


/**
 * Process trace object.
 *
 * Function must be called cyclically in 1ms intervals.
 *
 * @param trace This object.
 * @param timestamp Timestamp (usually in millisecond resolution).
 *
 * @return 0 on success, -1 on error.
 */
void CO_trace_process(CO_trace_t *trace, uint32_t timestamp);

#ifdef __cplusplus
}
#endif /*__cplusplus*/

/** @} */
#endif
