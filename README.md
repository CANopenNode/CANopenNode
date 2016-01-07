CANopenNode
===========

CANopenNode is an opensource CANopen Stack.

CANopen is the internationally standardized (EN 50325-4) CAN-based higher-layer protocol for embedded control system. For more information on CANopen see http://www.can-cia.org/

Stack is written in ANSI C in object-oriented way. Code is documented. License is LGPL V2.1.

Variables (communication, device, custom) are ordered in CANopen object dictionary and are accessible from both: C code and from CAN network.


CANopen Features
----------------
 - NMT slave to start, stop, reset device.
 - Heartbeat producer/consumer error control.
 - PDO linking and dynamic mapping for fast exchange of process variables.
 - SDO expedited, segmented and block transfer for service access to all parameters.
 - SDO master.
 - Emergency message.
 - Sync producer/consumer.
 - Nonvolatile storage.


Flowchart of a typical CANopenNode implementation
-------------------------------------------------
```
                            -----------------------
                           |     Program start     |
                            -----------------------
                                       |
                            -----------------------
                           |     CANopen init      |
                            -----------------------
                                       |
                            -----------------------
                           |     Start threads     |
                            -----------------------
                                 |     |     |
             --------------------      |      --------------------
            |                          |                          |
 -----------------------    -----------------------    -----------------------
| CAN receive thread    |  | Timer interval thread |  | Mainline thread       |
|                       |  |                       |  |                       |
| - Fast response.      |  | - Realtime thread with|  | - Processing of time  |
| - Detect CAN ID.      |  |   constant interval,  |  |   consuming tasks     |
| - Partially process   |  |   typically 1ms.      |  |   in CANopen objects: |
|   messages and copy   |  | - Network synchronized|  |    - SDO server,      |
|   data to target      |  | - Copy inputs (RPDOs, |  |    - Emergency,       |
|   CANopen objects.    |  |   HW) to Object Dict. |  |    - Network state,   |
|                       |  | - May call application|  |    - Heartbeat.       |
|                       |  |   for some processing.|  | - May cyclically call |
|                       |  | - Copy variables from |  |   application code.   |
|                       |  |   Object Dictionary to|  |                       |
|                       |  |   outputs (TPDOs, HW).|  |                       |
 -----------------------    -----------------------    -----------------------

              -----------------------
             | SDO client (optional) |
             |                       |
             | - Can be called by    |
             |   external application|
             | - Can read or write   |
             |   any variable from   |
             |   Object Dictionary   |
             |   from any node in the|
             |   CANopen network.    |
              -----------------------
```

Usage of the CANopenNode
------------------------
CANopenNode itself doesn't have complete working code for any microcontroller.
It is only the library with the stack and drivers for different
microcontrollers. It has example, which should compile on any sistem with
template driver (drvTemplate), which actually doesn't access CAN hardware.
CANopenNode should be used as a git submodule included in a project with
specific hardware and specific application.

File structure
--------------
 - **`CANopen.h/.c`** - Initialization and processing of CANopen objects. Most
   usual implementation of CANopen device.
 - **`stack`** - Directory with all CANopen objects in separate files.
   - **`CO_Emergency.h/.c`** - CANopen Emergency object.
   - **`CO_NMT_Heartbeat.h/.c`** - CANopen Network slave and Heartbeat producer object.
   - **`CO_HBconsumer.h/.c`** - CANopen Heartbeat consumer object.
   - **`CO_SYNC.h/.c`** - CANopen SYNC producer and consumer object.
   - **`CO_SDO.h/.c`** - CANopen SDO server object. It serves data from Object dictionary.
   - **`CO_PDO.h/.c`** - CANopen PDO object. It configures, receives and transmits CANopen process data.
   - **`CO_SDOmaster.h/.c`** - CANopen SDO client object (master functionality).
   - **`crc16-ccitt.h/.c`** - CRC calculation object.
   - **`drvTemplate`** - Directory with microcontroller specific files. In this
     case it is template for new implementations. It is also documented, other
     directories are not.
     - **`CO_driver.h/.c`** - Microcontroller specific objects for CAN module.
     - **`eeprom.h/.c`** - Functions for storage of Object dictionary, optional.
     - **`(helpers.h/.c)`** - Some optional files with specific helper functions.
   - **`socketCAN`** - Directory for Linux socketCAN interface.
   - **`PIC32`** - Directory for PIC32 devices from Microchip.
   - **`PIC24_dsPIC33`** - Directory for PIC24 and dsPIC33 devices from Microchip.
   - **`dsPIC30F`** - Directory for dsPIC30F devices from Microchip.
   - **`eCos`** - Directory for all devices supported by eCos RTOS.
   - **`SAM3X`** - Directory for SAM3X ARM Cortex M3 devices with ASF library from Atmel.
   - **`STM32`** - Directory for STM32 ARM devices from ST.
   - **`LPC177x_8x`** - Directory for LPC177x (Cortex M3) devices with FreeRTOS from NXP.
   - **`MCF5282`** - Directory for MCF5282 (ColdFire V2) device from Freescale.
 - **`codingStyle`** - Description of the coding style.
 - **`Doxyfile`** - Configuration file for the documentation generator *`doxygen`*.
 - **`Makefile`** - Basic makefile.
 - **`LICENSE`** - License.
 - **`README.md`** - This file.
 - **`example`** - Directory with basic example.
   - **`main.c`** - Mainline and other threads - example template.
   - **`application.h/.c`** - Separate file with some functions, which are
     called from main.c. May be used for application specific code.
   - **`CO_OD.h/.c`** - CANopen Object dictionary. Automatically generated file.
   - **`IO.eds`** - Standard CANopen EDS file, which may be used from CANopen
     configuration tool. Automatically generated file.
   - **`_project.xml`** - XML file contains all data for CANopen Object dictionary.
     It is used by *`Object dictionary editor`* application, which generates other
     files. *`Object dictionary editor`* is currently fully  functional, but old
     web application. See http://sourceforge.net/p/canopennode/code_complete/.
   - **`_project.html`** - *`Object dictionary editor`* launcher.

Microcontroller support
-----------------------

|            | Status (date) | OD storage | Example |
|------------|:-------------:|:----------:|---------|
| drvTemplate| OK            | template   | [here](https://github.com/CANopenNode/CANopenNode) |
| socketCAN  | beta   (2015) | Yes        | [CANopenSocket](https://github.com/CANopenNode/CANopenSocket) |
| PIC32      | stable (2014) | Yes        | [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) |
| PIC24, 33  | stable (2014) | no         | [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) |
| dsPIC30F   | beta   (2013) | no         | [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) |
| eCos       | stable (2013) | Yes        | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| SAM3X      | ?             | Yes        | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| STM32      | ?             | no         | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| LPC177x_8x | ?             | no         | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| MCF5282    | ?             | no         | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |


### Other known implementations with source code
 - AD ADSP-CM408 mixed signal controller
   - http://www.analog.com/en/processors-dsp/cm4xx/adsp-cm408f/products/cm40x-ez/eb.html
   - Contributed by Analog devices, Inc. (dec 2014):
     - See: http://sourceforge.net/p/canopennode/discussion/387151/thread/bfbcab97/
   - Package and setup user guide are available at:
     - http://sourceforge.net/projects/canopennode-for-adsp-cm408f/
     - http://sourceforge.net/projects/canopennode-for-adsp-cm408f/files
 - Discontinued implementations from earlier versions of CANopenNode
   - Microchip PIC18F
   - BECK IPC Embedded Web-Controller SC243


History of the project
----------------------
Project was initially hosted on http://sourceforge.net/projects/canopennode/
It started in 2004 with PIC18F microcontrollers from Microchip.
Fresh, cleaned repository of CANopenNode stack started on 25.7.2015.
For older history see http://sourceforge.net/p/canopennode/code_complete/
