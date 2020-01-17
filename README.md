CANopenNode
===========

CANopenNode is free and open source CANopen Stack.

CANopen is the internationally standardized (EN 50325-4)
([CiA301](http://can-cia.org/standardization/technical-documents))
CAN-based higher-layer protocol for embedded control system. For more
information on CANopen see http://www.can-cia.org/

CANopenNode is written in ANSI C in object-oriented way. It runs on
different microcontrollers, as standalone application or with RTOS.
Stack includes master functionalities. For Linux implementation with
CANopen master functionalities see
https://github.com/CANopenNode/CANopenSocket.

Variables (communication, device, custom) are ordered in CANopen Object
Dictionary and are accessible from both: C code and from CAN network.

CANopenNode homepage is https://github.com/CANopenNode/CANopenNode


CANopen Features
----------------
 - NMT slave to start, stop, reset device. Simple NMT master.
 - Heartbeat producer/consumer error control.
 - PDO linking and dynamic mapping for fast exchange of process variables.
 - SDO expedited, segmented and block transfer for service access to all parameters.
 - SDO master.
 - Emergency message.
 - Sync producer/consumer.
 - Time protocol (producer/consumer).
 - Non-volatile storage.
 - LSS master and slave, LSS fastscan

### RTR
RTR (remote transmission request) is a feature of CAN bus. Usage of RTR
is not recommended for CANopen and it is not implemented in CANopenNode.

### Self start
Object **0x1F80** from Object Dictionary enables the NMT slaves to start
automatically or allows it to start the whole network. It is specified in
DSP302-2 standard. Standard allows two values for slaves for object 0x1F80:
- Object 0x1F80, value = **0x8** - "NMT slave shall enter the NMT state
  Operational after the NMT state Initialization autonomously (self starting)"
- Object 0x1F80, value = **0x2** - "NMT slave shall execute the NMT service
  start remote node with node-ID set to 0"

Note: When node is stated (in NMT operational state), it is allowed to send or
receive Process Data Objects (PDO). If Error Register (object 0x1001) is set,
then NMT operational state is not allowed.


Usage of the CANopenNode
------------------------
CANopenNode itself doesn't have complete working code for any microcontroller.
It is only the library with the stack It has example, which should compile
on any system with template driver (drvTemplate), which actually doesn't
access CAN hardware. CANopenNode should be used as a git submodule included
in a project with specific hardware and specific application.


Documentation, support and contributions
----------------------------------------
Code is documented in header files. Running [doxygen](http://www.doxygen.nl/)
or `make doc` in project base folder will produce complete html documentation.
Just open CANopenNode/doc/html/index.html in browser.

Report issues on https://github.com/CANopenNode/CANopenNode/issues

Older and still active discussion group is on Sourceforge
http://sourceforge.net/p/canopennode/discussion/387151/

For some implementations of CANopenNode on real hardware see
[Device support](#device-support) section.
[CANopenSocket](https://github.com/CANopenNode/CANopenSocket) is nice
implementation for Linux devices. It includes command line interface for
master access of the CANopen network. There is also some Getting started.

Contributions are welcome. Best way to contribute your code is to fork
a project, modify it and then send a pull request. Some basic formatting
rules should be followed: Linux style with indentation of 4 spaces. There
is also a configuration file for `clang-format` tool.


Flowchart of a typical CANopenNode implementation
-------------------------------------------------
~~~
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

              -----------------------
             | LSS client (optional) |
             |                       |
             | - Can be called by    |
             |   external application|
             | - Can do LSS requests |
             | - Can request node    |
             |   enumeration         |
              -----------------------


~~~


File structure
--------------
 - **CANopen.h/.c** - Initialization and processing of CANopen objects. Most
   usual implementation of CANopen device.
 - **stack** - Directory with all CANopen objects in separate files.
   - **CO_Emergency.h/.c** - CANopen Emergency object.
   - **CO_NMT_Heartbeat.h/.c** - CANopen Network slave and Heartbeat producer object.
   - **CO_HBconsumer.h/.c** - CANopen Heartbeat consumer object.
   - **CO_LSS.h** - CANopen LSS common. This is common to LSS master and slave.
   - **CO_LSSmaster.h/.c** - CANopen LSS master functionality.
   - **CO_LSSslave.h/.c** - CANopen LSS slave functionality.
   - **CO_SYNC.h/.c** - CANopen SYNC producer and consumer object.
   - **CO_TIME.h/.c** - CANopen TIME protocol object.
   - **CO_SDO.h/.c** - CANopen SDO server object. It serves data from Object dictionary.
   - **CO_PDO.h/.c** - CANopen PDO object. It configures, receives and transmits CANopen process data.
   - **CO_SDOmaster.h/.c** - CANopen SDO client object (master functionality).
   - **CO_trace.h/.c** - Trace object with timestamp for monitoring variables from Object Dictionary (optional).
   - **crc16-ccitt.h/.c** - CRC calculation object.
   - **drvTemplate** - Directory with microcontroller specific files. In this
     case it is template for new implementations. It is also documented, other
     directories are not.
     - **CO_driver.h/.c** - Microcontroller specific objects for CAN module.
     - **eeprom.h/.c** - Functions for storage of Object dictionary, optional.
     - **helpers.h/.c** - Some optional files with specific helper functions.
   - **socketCAN** - Directory for Linux socketCAN interface.
   - **PIC32** - Directory for PIC32 devices from Microchip.
   - **PIC24_dsPIC33** - Directory for PIC24 and dsPIC33 devices from Microchip.
   - **dsPIC30F** - Directory for dsPIC30F devices from Microchip.
   - **eCos** - Directory for all devices supported by eCos RTOS.
   - **SAM3X** - Directory for SAM3X ARM Cortex M3 devices with ASF library from Atmel.
   - **STM32** - Directory for STM32 ARM devices from ST.
   - **LPC177x_8x** - Directory for LPC177x (Cortex M3) devices with FreeRTOS from NXP.
   - **MCF5282** - Directory for MCF5282 (ColdFire V2) device from Freescale.
 - **codingStyle** - Description of the coding style.
 - **Doxyfile** - Configuration file for the documentation generator *doxygen*.
 - **Makefile** - Basic makefile.
 - **LICENSE** - License.
 - **README.md** - This file.
 - **example** - Directory with basic example.
   - **main.c** - Mainline and other threads - example template.
   - **application.h/.c** - Separate file with some functions, which are
     called from main.c. May be used for application specific code.
   - **CO_OD.h/.c** - CANopen Object dictionary. Automatically generated files.
   - **IO.eds** - Standard CANopen EDS file, which may be used from CANopen
     configuration tool. Automatically generated file.
   - _ **project.xml** - XML file contains all data for CANopen Object dictionary.
     It is used by *Object dictionary editor* application, which generates other
     files.
   - _ **project.html** - *Object dictionary editor* launcher.


### Object dictionary editor
Object Dictionary is one of the most important parts of CANopen. Its
implementation in CANopenNode is quite outdated and there are efforts to
rewrite it. Anyway, currently it is fully operational and works well.

To customize the Object Dictionary it is necessary to use the
external application. There are two:
 - [libedssharp](https://github.com/robincornelius/libedssharp) -
   recommended, can be used with mono.
 - [Object_Dictionary_Editor](http://sourceforge.net/p/canopennode/code_complete/) -
   originally part of CANopenNode. It is still operational, but requiers
   very old version of Firefox to run.


Device support
--------------
CANopenNode can be implemented on many different devices. It is
necessary to implement interface to specific hardware, so called 'driver'.
Currently driver files are part of CANopenNode, but they will be split from
it in the future.

Most up to date information on device support can be found on
[CANopenNode/wiki](https://github.com/CANopenNode/CANopenNode/wiki).


### Note for contributors
For the driver developers, who wish to share and cooperate, I recommend the following approach:
1. Make own git repo for the Device specific demo project on the Github or somewhere.
2. Add https://github.com/CANopenNode/CANopenNode into your project (or at side of your project).
   For example, include it in your project as a git submodule:
   `git submodule add https://github.com/CANopenNode/CANopenNode`
3. Add specific driver and other files.
4. **Add a note** about your specific implementation here on
   [CANopenNode/wiki](https://github.com/CANopenNode/CANopenNode/wiki) with some
   basic description and status. Write a note, even it has an Alpha status.
5. Make a demo folder, which contains project files, etc., necessary to run the demo.
6. Write a good README.md file, where you describe your project, specify demo board, tools used, etc.


History of the project
----------------------
Project was initially hosted on http://sourceforge.net/projects/canopennode/
It started in 2004 with PIC18F microcontrollers from Microchip.
Fresh, cleaned repository of CANopenNode stack started on 25.7.2015.
For older history see http://sourceforge.net/p/canopennode/code_complete/


License
-------
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
