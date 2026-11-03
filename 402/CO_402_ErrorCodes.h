/**
 * CANopen standard error codes according to CiA DS-402.
 *
 * @file        CO_402_ErrorCodes.h
 * @ingroup     CO_402_ErrorCodes
 * @author      Jonathan Demeyer
 * @copyright   2026 Jonathan Demeyer
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

#ifndef CO_402_ERROR_CODES_H
#define CO_402_ERROR_CODES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup CO_EMC402_errorCode_t CANopen Error code
 * @{
 *
 * Standard error codes according to CiA DS-402.
 */
/* Current */

#define CO_EMC402_IN_SHORT_EARTH         0x2110U /**< 0x2110 DS402 Short circuit/earth leakage at input */
#define CO_EMC402_IN_EARTH_LEAK          0x2120U /**< 0x2120 DS402 Earth leakage at input */
#define CO_EMC402_IN_EARTH_L1            0x2121U /**< 0x2121 DS402 Earth leakage phase L1 */
#define CO_EMC402_IN_EARTH_L2            0x2122U /**< 0x2122 DS402 Earth leakage phase L2 */
#define CO_EMC402_IN_EARTH_L3            0x2123U /**< 0x2123 DS402 Earth leakage phase L3 */
#define CO_EMC402_IN_SHORT               0x2130U /**< 0x2130 DS402 Short circuit at input */
#define CO_EMC402_IN_SHORT_L1_L2         0x2131U /**< 0x2131 DS402 Short circuit phases L1-L2 */
#define CO_EMC402_IN_SHORT_L2_L3         0x2132U /**< 0x2132 DS402 Short circuit phases L2-L3 */
#define CO_EMC402_IN_SHORT_L3_L1         0x2133U /**< 0x2133 DS402 Short circuit phases L3-L1 */

#define CO_EMC402_INTERNAL_CUR_1         0x2211U /**< 0x2211 DS402 Internal current No.1 */
#define CO_EMC402_INTERNAL_CUR_2         0x2212U /**< 0x2212 DS402 Internal current No.2 */
#define CO_EMC402_CUR_RAMP_OVER          0x2213U /**< 0x2213 DS402 Over-current in ramp function */
#define CO_EMC402_CUR_SEQUENCE_OVER      0x2214U /**< 0x2214 DS402 Over-current in the sequence */
#define CO_EMC402_CONT_CUR_OVER          0x2220U /**< 0x2220 DS402 Continuous over current (device internal) */
#define CO_EMC402_CONT_CUR_OVER_1        0x2221U /**< 0x2221 DS402 Continuous over current No.1 */
#define CO_EMC402_CONT_CUR_OVER_2        0x2222U /**< 0x2222 DS402 Continuous over current No.2 */
#define CO_EMC402_INTERNAL_SHORT_EARTH   0x2230U /**< 0x2230 DS402 Short circuit/earth leakage (device internal) */
#define CO_EMC402_INTERNAL_EARTH_LEAK    0x2240U /**< 0x2240 DS402 Earth leakage (device internal) */
#define CO_EMC402_INTERNAL_SHORT         0x2250U /**< 0x2250 DS402 Short circuit (device internal) */

#define CO_EMC402_OUT_CONT_CUR_OVER      0x2310U /**< 0x2310 DS402 Continuous over current */
#define CO_EMC402_OUT_CONT_CUR_OVER_1    0x2311U /**< 0x2311 DS402 Continuous over current No.1 */
#define CO_EMC402_OUT_CONT_CUR_OVER_2    0x2312U /**< 0x2312 DS402 Continuous over current No.2 */
#define CO_EMC402_OUT_SHORT_EARTH        0x2320U /**< 0x2320 DS402 Short circuit/earth leakage (motor-side) */
#define CO_EMC402_OUT_EARTH_LEAK         0x2330U /**< 0x2330 DS402 Earth leakage (motor-side) */
#define CO_EMC402_OUT_EARTH_U            0x2331U /**< 0x2331 DS402 Earth leakage phase U */
#define CO_EMC402_OUT_EARTH_V            0x2332U /**< 0x2332 DS402 Earth leakage phase V */
#define CO_EMC402_OUT_EARTH_W            0x2333U /**< 0x2333 DS402 Earth leakage phase W */
#define CO_EMC402_OUT_SHORT              0x2340U /**< 0x2340 DS402 Short circuit (motor-side) */
#define CO_EMC402_OUT_SHORT_U_V          0x2341U /**< 0x2341 DS402 Short circuit phases U-V */
#define CO_EMC402_OUT_SHORT_V_W          0x2342U /**< 0x2342 DS402 Short circuit phases V-W */
#define CO_EMC402_OUT_SHORT_W_U          0x2343U /**< 0x2343 DS402 Short circuit phases W-U */

#define CO_EMC402_LOAD_LEVEL_FAULT       0x2350U /**< 0x2350 DS402 Load level fault (I²t, thermal state) */
#define CO_EMC402_LOAD_LEVEL_WARNING     0x2351U /**< 0x2351 DS402 Load level warning (I²t, thermal state) */


/* Voltage */

#define CO_EMC402_MAINS_VOLT_OVER        0x3110U /**< 0x3110 DS402 Mains over-voltage */
#define CO_EMC402_MAINS_VOLT_OVER_L1     0x3111U /**< 0x3111 DS402 Mains over-voltage phase L1 */
#define CO_EMC402_MAINS_VOLT_OVER_L2     0x3112U /**< 0x3112 DS402 Mains over-voltage phase L2 */
#define CO_EMC402_MAINS_VOLT_OVER_L3     0x3113U /**< 0x3113 DS402 Mains over-voltage phase L3 */
#define CO_EMC402_MAINS_VOLT_UNDER       0x3120U /**< 0x3120 DS402 Mains under-voltage */
#define CO_EMC402_MAINS_VOLT_UNDER_L1    0x3121U /**< 0x3121 DS402 Mains under-voltage phase L1 */
#define CO_EMC402_MAINS_VOLT_UNDER_L2    0x3122U /**< 0x3122 DS402 Mains under-voltage phase L2 */
#define CO_EMC402_MAINS_VOLT_UNDER_L3    0x3123U /**< 0x3123 DS402 Mains under-voltage phase L3 */
#define CO_EMC402_PHASE_FAILURE          0x3130U /**< 0x3130 DS402 Phase failure */
#define CO_EMC402_PHASE_FAILURE_L1       0x3131U /**< 0x3131 DS402 Phase failure L1 */
#define CO_EMC402_PHASE_FAILURE_L2       0x3132U /**< 0x3132 DS402 Phase failure L2 */
#define CO_EMC402_PHASE_FAILURE_L3       0x3133U /**< 0x3133 DS402 Phase failure L3 */
#define CO_EMC402_PHASE_SEQUENCE         0x3134U /**< 0x3134 DS402 Phase sequence */
#define CO_EMC402_MAINS_FREQ             0x3140U /**< 0x3140 DS402 Mains frequency */
#define CO_EMC402_MAINS_FREQ_HIGH        0x3141U /**< 0x3141 DS402 Mains frequency too high */
#define CO_EMC402_MAINS_FREQ_LOW         0x3142U /**< 0x3142 DS402 Mains frequency too low */

#define CO_EMC402_DC_LINK_VOLT_OVER      0x3210U /**< 0x3210 DS402 DC link over-voltage */
#define CO_EMC402_DC_LINK_VOLT_OVER_1    0x3211U /**< 0x3211 DS402 Over-voltage No.1 */
#define CO_EMC402_DC_LINK_VOLT_OVER_2    0x3212U /**< 0x3212 DS402 Over-voltage No.2 */
#define CO_EMC402_DC_LINK_VOLT_UNDER     0x3220U /**< 0x3220 DS402 DC link under-voltage */
#define CO_EMC402_DC_LINK_VOLT_UNDER_1   0x3221U /**< 0x3221 DS402 Under-voltage No.1 */
#define CO_EMC402_DC_LINK_VOLT_UNDER_2   0x3222U /**< 0x3222 DS402 Under-voltage No.2 */
#define CO_EMC402_LOAD_ERROR             0x3230U /**< 0x3230 DS402 Load error */

#define CO_EMC402_OUT_VOLT_OVER          0x3310U /**< 0x3310 DS402 Output over-voltage */
#define CO_EMC402_OUT_VOLT_OVER_U        0x3311U /**< 0x3311 DS402 Output over-voltage phase U */
#define CO_EMC402_OUT_VOLT_OVER_V        0x3312U /**< 0x3312 DS402 Output over-voltage phase V */
#define CO_EMC402_OUT_VOLT_OVER_W        0x3313U /**< 0x3313 DS402 Output over-voltage phase W */
#define CO_EMC402_ARMATURE_CIRCUIT       0x3320U /**< 0x3320 DS402 Armature circuit */
#define CO_EMC402_ARMATURE_INTERRUPTED   0x3321U /**< 0x3321 DS402 Armature circuit interrupted */
#define CO_EMC402_FIELD_CIRCUIT          0x3330U /**< 0x3330 DS402 Field circuit */
#define CO_EMC402_FIELD_INTERRUPTED      0x3331U /**< 0x3331 DS402 Field circuit interrupted */


/* Temperature */

#define CO_EMC402_AMBIENT_TEMP_HIGH      0x4110U /**< 0x4110 DS402 Excess ambient temperature */
#define CO_EMC402_AMBIENT_TEMP_LOW       0x4120U /**< 0x4120 DS402 Too low ambient temperature */
#define CO_EMC402_SUPPLY_AIR_TEMP        0x4130U /**< 0x4130 DS402 Temperature supply air */
#define CO_EMC402_AIR_OUTLET_TEMP        0x4140U /**< 0x4140 DS402 Temperature air outlet */

#define CO_EMC402_DEVICE_TEMP_HIGH       0x4210U /**< 0x4210 DS402 Excess temperature device */
#define CO_EMC402_DEVICE_TEMP_LOW        0x4220U /**< 0x4220 DS402 Too low temperature device */

#define CO_EMC402_TEMP_DRIVE             0x4300U /**< 0x4300 DS402 Temperature drive */
#define CO_EMC402_DRIVE_TEMP_HIGH        0x4310U /**< 0x4310 DS402 Excess temperature drive */
#define CO_EMC402_DRIVE_TEMP_LOW         0x4320U /**< 0x4320 DS402 Too low temperature drive */

#define CO_EMC402_TEMP_SUPPLY            0x4400U /**< 0x4400 DS402 Temperature supply */
#define CO_EMC402_SUPPLY_TEMP_HIGH       0x4410U /**< 0x4410 DS402 Excess temperature supply */
#define CO_EMC402_SUPPLY_TEMP_LOW        0x4420U /**< 0x4420 DS402 Too low temperature supply */


/* Device hardware */

#define CO_EMC402_SUPPLY                  0x5100U /**< 0x5100 DS402 Supply */
#define CO_EMC402_SUPPLY_LOW_VOLT         0x5110U /**< 0x5110 DS402 Supply low voltage */
#define CO_EMC402_SUPPLY_15V              0x5111U /**< 0x5111 DS402 U1 supply +/-15 V */
#define CO_EMC402_SUPPLY_24V              0x5112U /**< 0x5112 DS402 U2 supply +24 V */
#define CO_EMC402_SUPPLY_5V               0x5113U /**< 0x5113 DS402 U3 supply +5 V */
#define CO_EMC402_SUPPLY_U4               0x5114U /**< 0x5114 DS402 U4 manufacturer-specific */
#define CO_EMC402_SUPPLY_U5               0x5115U /**< 0x5115 DS402 U5 manufacturer-specific */
#define CO_EMC402_SUPPLY_U6               0x5116U /**< 0x5116 DS402 U6 manufacturer-specific */
#define CO_EMC402_SUPPLY_U7               0x5117U /**< 0x5117 DS402 U7 manufacturer-specific */
#define CO_EMC402_SUPPLY_U8               0x5118U /**< 0x5118 DS402 U8 manufacturer-specific */
#define CO_EMC402_SUPPLY_U9               0x5119U /**< 0x5119 DS402 U9 manufacturer-specific */
#define CO_EMC402_SUPPLY_INTERMEDIATE     0x5120U /**< 0x5120 DS402 Supply intermediate circuit */

#define CO_EMC402_DEV_HW_CONTROL          0x5200U /**< 0x5200 DS402 Control (Device hardware) */
#define CO_EMC402_MEASUREMENT_CIRCUIT     0x5210U /**< 0x5210 DS402 Measurement circuit */
#define CO_EMC402_COMPUTING_CIRCUIT       0x5220U /**< 0x5220 DS402 Computing circuit */

#define CO_EMC402_OPERATING_UNIT          0x5300U /**< 0x5300 DS402 Operating unit */

#define CO_EMC402_POWER_SECTION           0x5400U /**< 0x5400 DS402 Power section */
#define CO_EMC402_OUTPUT_STAGES           0x5410U /**< 0x5410 DS402 Output stages */
#define CO_EMC402_CHOPPER                 0x5420U /**< 0x5420 DS402 Chopper */
#define CO_EMC402_INPUT_STAGES            0x5430U /**< 0x5430 DS402 Input stages */
#define CO_EMC402_CONTACTS                0x5440U /**< 0x5440 DS402 Contacts */
#define CO_EMC402_CONTACT_1               0x5441U /**< 0x5441 DS402 Contact 1 (manufacturer-specific) */
#define CO_EMC402_CONTACT_2               0x5442U /**< 0x5442 DS402 Contact 2 (manufacturer-specific) */
#define CO_EMC402_CONTACT_3               0x5443U /**< 0x5443 DS402 Contact 3 (manufacturer-specific) */
#define CO_EMC402_CONTACT_4               0x5444U /**< 0x5444 DS402 Contact 4 (manufacturer-specific) */
#define CO_EMC402_CONTACT_5               0x5445U /**< 0x5445 DS402 Contact 5 (manufacturer-specific) */

#define CO_EMC402_FUSES                   0x5450U /**< 0x5450 DS402 Fuses */
#define CO_EMC402_FUSE_1                  0x5451U /**< 0x5451 DS402 Fuse S1 = L1 */
#define CO_EMC402_FUSE_2                  0x5452U /**< 0x5452 DS402 Fuse S2 = L2 */
#define CO_EMC402_FUSE_3                  0x5453U /**< 0x5453 DS402 Fuse S3 = L3 */
#define CO_EMC402_FUSE_4                  0x5454U /**< 0x5454 DS402 Fuse S4 (manufacturer-specific) */
#define CO_EMC402_FUSE_5                  0x5455U /**< 0x5455 DS402 Fuse S5 (manufacturer-specific) */
#define CO_EMC402_FUSE_6                  0x5456U /**< 0x5456 DS402 Fuse S6 (manufacturer-specific) */
#define CO_EMC402_FUSE_7                  0x5457U /**< 0x5457 DS402 Fuse S7 (manufacturer-specific) */
#define CO_EMC402_FUSE_8                  0x5458U /**< 0x5458 DS402 Fuse S8 (manufacturer-specific) */
#define CO_EMC402_FUSE_9                  0x5459U /**< 0x5459 DS402 Fuse S9 (manufacturer-specific) */

#define CO_EMC402_HARDWARE_MEMORY         0x5500U /**< 0x5500 DS402 Hardware memory */
#define CO_EMC402_RAM                     0x5510U /**< 0x5510 DS402 RAM */
#define CO_EMC402_ROM_EPROM               0x5520U /**< 0x5520 DS402 ROM/EPROM */
#define CO_EMC402_EEPROM                  0x5530U /**< 0x5530 DS402 EEPROM */


/* Device software */

#define CO_EMC402_SOFTWARE_RESET          0x6010U /**< 0x6010 DS402 Software reset (watchdog) */

#define CO_EMC402_DATA_RECORD_1           0x6301U /**< 0x6301 DS402 Data record no. 1 */
#define CO_EMC402_DATA_RECORD_2           0x6302U /**< 0x6302 DS402 Data record no. 2 */
#define CO_EMC402_DATA_RECORD_3           0x6303U /**< 0x6303 DS402 Data record no. 3 */
#define CO_EMC402_DATA_RECORD_4           0x6304U /**< 0x6304 DS402 Data record no. 4 */
#define CO_EMC402_DATA_RECORD_5           0x6305U /**< 0x6305 DS402 Data record no. 5 */
#define CO_EMC402_DATA_RECORD_6           0x6306U /**< 0x6306 DS402 Data record no. 6 */
#define CO_EMC402_DATA_RECORD_7           0x6307U /**< 0x6307 DS402 Data record no. 7 */
#define CO_EMC402_DATA_RECORD_8           0x6308U /**< 0x6308 DS402 Data record no. 8 */
#define CO_EMC402_DATA_RECORD_9           0x6309U /**< 0x6309 DS402 Data record no. 9 */
#define CO_EMC402_DATA_RECORD_10          0x630AU /**< 0x630A DS402 Data record no. 10 */
#define CO_EMC402_DATA_RECORD_11          0x630BU /**< 0x630B DS402 Data record no. 11 */
#define CO_EMC402_DATA_RECORD_12          0x630CU /**< 0x630C DS402 Data record no. 12 */
#define CO_EMC402_DATA_RECORD_13          0x630DU /**< 0x630D DS402 Data record no. 13 */
#define CO_EMC402_DATA_RECORD_14          0x630EU /**< 0x630E DS402 Data record no. 14 */
#define CO_EMC402_DATA_RECORD_15          0x630FU /**< 0x630F DS402 Data record no. 15 */

#define CO_EMC402_LOSS_OF_PARAMETERS      0x6310U /**< 0x6310 DS402 Loss of parameters */
#define CO_EMC402_PARAMETER_ERROR         0x6320U /**< 0x6320 DS402 Parameter error */


/* Additional modules */

#define CO_EMC402_POWER                   0x7100U /**< 0x7100 DS402 Power */
#define CO_EMC402_BRAKE_CHOPPER           0x7110U /**< 0x7110 DS402 Brake chopper */
#define CO_EMC402_BRAKE_CHOPPER_FAILURE   0x7111U /**< 0x7111 DS402 Failure brake chopper */
#define CO_EMC402_BRAKE_CHOPPER_OVERCUR   0x7112U /**< 0x7112 DS402 Over-current brake chopper */
#define CO_EMC402_BRAKE_CHOPPER_PROTECT   0x7113U /**< 0x7113 DS402 Protective circuit brake chopper */

#define CO_EMC402_MOTOR                   0x7120U /**< 0x7120 DS402 Motor */
#define CO_EMC402_MOTOR_BLOCKED           0x7121U /**< 0x7121 DS402 Motor blocked */
#define CO_EMC402_MOTOR_ERROR             0x7122U /**< 0x7122 DS402 Motor error or commutation malfunction */
#define CO_EMC402_MOTOR_TILTED            0x7123U /**< 0x7123 DS402 Motor tilted */

#define CO_EMC402_MEASUREMENT             0x7200U /**< 0x7200 DS402 Measurement circuit */

#define CO_EMC402_SENSOR                  0x7300U /**< 0x7300 DS402 Sensor */
#define CO_EMC402_TACHO_FAULT             0x7301U /**< 0x7301 DS402 Tacho fault */
#define CO_EMC402_TACHO_WRONG_POLARITY    0x7302U /**< 0x7302 DS402 Tacho wrong polarity */
#define CO_EMC402_RESOLVER_1_FAULT        0x7303U /**< 0x7303 DS402 Resolver 1 fault */
#define CO_EMC402_RESOLVER_2_FAULT        0x7304U /**< 0x7304 DS402 Resolver 2 fault */
#define CO_EMC402_INCREMENTAL_1_FAULT     0x7305U /**< 0x7305 DS402 Incremental sensor 1 fault */
#define CO_EMC402_INCREMENTAL_2_FAULT     0x7306U /**< 0x7306 DS402 Incremental sensor 2 fault */
#define CO_EMC402_INCREMENTAL_3_FAULT     0x7307U /**< 0x7307 DS402 Incremental sensor 3 fault */
#define CO_EMC402_SPEED                   0x7310U /**< 0x7310 DS402 Speed */
#define CO_EMC402_POSITION                0x7320U /**< 0x7320 DS402 Position */

#define CO_EMC402_COMPUTATION             0x7400U /**< 0x7400 DS402 Computation circuit */

#define CO_EMC402_COMMUNICATION           0x7500U /**< 0x7500 DS402 Communication */
#define CO_EMC402_SERIAL_1                0x7510U /**< 0x7510 DS402 Serial interface no. 1 */
#define CO_EMC402_SERIAL_2                0x7520U /**< 0x7520 DS402 Serial interface no. 2 */
#define CO_EMC402_DATA_STORAGE            0x7600U /**< 0x7600 DS402 Data storage (external) */


/* Drive control */

#define CO_EMC402_TORQUE_CONTROL          0x8300U /**< 0x8300 DS402 Torque control */
#define CO_EMC402_EXCESS_TORQUE           0x8311U /**< 0x8311 DS402 Excess torque */
#define CO_EMC402_DIFFICULT_STARTUP       0x8312U /**< 0x8312 DS402 Difficult start up */
#define CO_EMC402_STANDSTILL_TORQUE       0x8313U /**< 0x8313 DS402 Standstill torque */
#define CO_EMC402_INSUFFICIENT_TORQUE     0x8321U /**< 0x8321 DS402 Insufficient torque */
#define CO_EMC402_TORQUE_FAULT            0x8331U /**< 0x8331 DS402 Torque fault */

#define CO_EMC402_VELOCITY_CONTROLLER     0x8400U /**< 0x8400 DS402 Velocity/speed controller */
#define CO_EMC402_POSITION_CONTROLLER     0x8500U /**< 0x8500 DS402 Position controller */

#define CO_EMC402_POSITIONING_CONTROLLER  0x8600U /**< 0x8600 DS402 Positioning controller */
#define CO_EMC402_FOLLOWING_ERROR         0x8611U /**< 0x8611 DS402 Following error */
#define CO_EMC402_REFERENCE_LIMIT         0x8612U /**< 0x8612 DS402 Reference limit */
#define CO_EMC402_HOMING_ERROR            0x8613U /**< 0x8613 DS402 Homing error */

#define CO_EMC402_SYNC_CONTROLLER         0x8700U /**< 0x8700 DS402 Sync controller */
#define CO_EMC402_WINDING_CONTROLLER      0x8800U /**< 0x8800 DS402 Winding controller */
#define CO_EMC402_PROCESS_DATA_MONITORING 0x8900U /**< 0x8900 DS402 Process data monitoring */
#define CO_EMC402_CONTROL_MONITORING      0x8A00U /**< 0x8A00 DS402 Control monitoring */

/** @} */ /* CO_EMC402_errorCode_t */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CO_402_ERROR_CODES_H */
