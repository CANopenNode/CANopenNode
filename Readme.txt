CANopenNode is an opensource CANopen Stack.

CANopen is the internationally standardized (EN 50325-4) CAN-based higher-layer protocol for embedded control system. For more information on CANopen see http://www.can-cia.org/

Stack is written in ANSI C in object-oriented way. Code is documented. License is LGPL. It includes Example for CANopen Input/Output device and simple CANopen master.

Variables (communication, device, custom) are ordered in CANopen object dictionary and are accessible from both: C code and from CAN network.


Code includes:
 - CANopen Stack for multiple microcontrollers with documenation.
 - Working examples of simple Input Output CANopen device for all microcontrolers.
 - Working CANopen master device with HTML interface to CANopen: CAN log, Emergency log, SDO master, NMT master, Custom CAN message. (It runs on demo board DB240 from beck-ipc. It's available also for simpler custom borad based on SC243.)
 - CANopen Object dictionary editor. For usage of this web application see "about.html".


CANopen Features:
 - NMT slave to start, stop, reset device.
 - Heartbeat producer/consumer error control.
 - PDO linking and dynamic mapping for fast exchange of process variables.
 - SDO expedited and segmented transfer for service access to all parameters.
 - SDO master.
 - Emergency message.
 - Sync producer/consumer.
 - Nonvolatile storage.


Supported controllers:
 - From microchip.com:
   - dsPIC30F
   - PIC24H
   - dsPIC33F
   - PIC32
 - From beck-ipc.com
   - SC2x3

   
****************************************************************************************************
Version next

Style change:
 - SVN version 31 has completely different style from previous. SVN history is broken. However, C code is the same. Files compile to THE SAME hex file in PIC32. See file Misc/convert text.html.
 - Indentation changed from three to four spaces.
 - Comments are now doxigen style.
 - Replacement of typedefs UNSIGNED32 to uint32_t, etc.
Updates:
 - Object dictionary editor: changed HTML output, no ODF functions.
 - SDO server rewritten. Better interface with Object dictionary function.
 - Multiple files was updated for newer Object dictionary function.
Additional features:
 - Added block transfer. It is possible to transfer large block of data (32bit length).
 - Added CRC function. Calculation of CRC in eeprom.c files updated to this function.
 - Added are files for generic driver, which allows compiling the stack without hardware. They should be used as a template for other microcontrollers.
Fixes:
 - Time drift in heartbeat fixed.
FIXES IN PROCESSOR SPECIFIC FILES:
 - !!IMPORTANT BUGFIX!! Problems with synchronous TPDOs. Microcontroller stopped transmitting on tx overflow. Code for CAN transmission is revised and fixed in all microcontrollers. Tested on PIC32 and dsPIC33F.
Note:
 - Object dictionary editor does not work any more in Firefox 17 and above, because: https://developer.mozilla.org/en-US/docs/E4X. (It is possible to re-enable E4X in FF17: type about:config in addressbar and change javascript.options.xml.content to true.)



****************************************************************************************************
Version 3.10
Additional features:
 - Master device with HTML interface to CANopen: CAN log, Emergency log, SDO master, NMT master, Custom CAN message.
 - Additional data types: UNSIGNED64, INTEGER64, REAL32, REAL64, DOMAIN.
 - SDO transfer now supports more than 255 bytes in one communication cycle. (SDO master too.)
 - EEPROM for PIC32 and SC243 fully functional.
Fixes:
 - Synchronous window lenght now works.
 - Filters in PIC32 are dinamicaly editable.
 - Heartbeat consumer is fixed. Now is dinamicaly editable.
 - Some fixes in PDO.
Changes in processor specific files:
 - !!Bugfix!! Some variables are set to volatile.
 - !!Bugfix!! in all processors: Line "CANmodule->bufferInhibitFlag = 0;" in CAN tx interrupt moved up.
 - Added DOMAIN data type.
 - Function CO_ODF() removed from driver.


****************************************************************************************************
Version 3.02
Fixes:
 - SYNC object changes:
    - LED tripple flash corrected.
    - No need for definition of communication cycle period (index 1006).
 - Correction in 'Object dictionary editor' output.
 - PIC32 now resets correctly when receives NMT command.
 - NMT master example now works.


****************************************************************************************************
Version 3.01
Fixes:
 - It compiles now without errors for all controllers.
 - Newly generated files from object dictionary, including CO_OD.c, .h.




****************************************************************************************************
Version 3.00

