CANopenNode
===========

CANopenNode is free and open source CANopen protocol stack.

CANopen is the internationally standardized (EN 50325-4)
([CiA301](http://can-cia.org/standardization/technical-documents))
CAN-based higher-layer protocol for embedded control system. For more
information on CANopen see http://www.can-cia.org/

CANopenNode is written in ANSI C in object-oriented way. It runs on
different microcontrollers, as standalone application or with RTOS.
Linux implementation with CANopen master functionalities is included.

Variables (communication, device, custom) are ordered in CANopen Object
Dictionary and are accessible from both: C code and from CANopen network.

CANopenNode homepage is https://github.com/CANopenNode/CANopenNode


Characteristics
---------------
### CANopen
 - [NMT](https://www.can-cia.org/can-knowledge/canopen/network-management/)
   slave to start, stop, reset device. Simple NMT master.
 - [Heartbeat](https://www.can-cia.org/can-knowledge/canopen/error-control-protocols/)
   producer/consumer error control.
 - [PDO](https://www.can-cia.org/can-knowledge/canopen/pdo-protocol/) linking
   and dynamic mapping for fast exchange of process variables.
 - [SDO](https://www.can-cia.org/can-knowledge/canopen/sdo-protocol/) expedited,
   segmented and block transfer for service access to all parameters.
 - [SDO](https://www.can-cia.org/can-knowledge/canopen/sdo-protocol/) client.
 - [Emergency](https://www.can-cia.org/can-knowledge/canopen/special-function-protocols/)
   producer/consumer.
 - [Sync](https://www.can-cia.org/can-knowledge/canopen/special-function-protocols/)
   producer/consumer.
 - [Time-stamp](https://www.can-cia.org/can-knowledge/canopen/special-function-protocols/)
   protocol producer/consumer.
 - [LSS](https://www.can-cia.org/can-knowledge/canopen/cia305/) master and
   slave, LSS fastscan.
 - [CANopen gateway](https://www.can-cia.org/can-knowledge/canopen/cia309/),
   CiA309-3 Ascii command interface for NMT master, LSS master and SDO client.

### Other
 - [Suitable for 16-bit microcontrollers and above](#device-support)
 - [Multithreaded, real-time](#flowchart-of-a-typical-canopennode-implementation)
 - [Object Dictionary editor](#object-dictionary-editor)
 - Non-volatile storage.
 - [Power saving possible](#power-saving)
 - [Bootloader possible](https://github.com/CANopenNode/CANopenNode/issues/111) (for firmware update)


Documentation, support and contributions
----------------------------------------
Documentation with [Getting started](doc/gettingStarted.md),
[LSS usage](doc/LSSusage.md) and [Trace usage](doc/traceUsage.md) is in `doc`
directory.
Code is documented in header files. Running [doxygen](http://www.doxygen.nl/)
in project base directory will produce complete html documentation.
Just open CANopenNode/doc/html/index.html in the browser. Alternatively browse
documentation [online](https://canopennode.github.io/CANopenSocket/).

Report issues on https://github.com/CANopenNode/CANopenNode/issues

Older and still active discussion group is on Sourceforge
http://sourceforge.net/p/canopennode/discussion/387151/

Contributions are welcome. Best way to contribute your code is to fork
a project, modify it and then send a pull request. Some basic formatting
rules should be followed: Linux style with indentation of 4 spaces. There is
also a `codingStyle` file with example and a configuration file for
`clang-format` tool.


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
|                       |  |   for some processing.|  |    - LSS slave        |
|                       |  | - Copy variables from |  | - May cyclically call |
|                       |  |   Object Dictionary to|  |   application code.   |
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
             | LSS Master (optional) |
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
 - **301/** - CANopen application layer and communication profile.
   - **CO_config.h** - Configuration macros for CANopenNode.
   - **CO_driver.h** - Interface between CAN hardware and CANopenNode.
   - **CO_Emergency.h/.c** - CANopen Emergency protocol.
   - **CO_HBconsumer.h/.c** - CANopen Heartbeat consumer protocol.
   - **CO_NMT_Heartbeat.h/.c** - CANopen Network management and Heartbeat producer protocol.
   - **CO_PDO.h/.c** - CANopen Process Data Object protocol.
   - **CO_SDOclient.h/.c** - CANopen Service Data Object - client protocol (master functionality).
   - **CO_SDOserver.h/.c** - CANopen Service Data Object - server protocol.
   - **CO_SYNC.h/.c** - CANopen Synchronisation protocol (producer and consumer).
   - **CO_TIME.h/.c** - CANopen Time-stamp protocol.
   - **CO_fifo.h/.c** - Fifo buffer for SDO and gateway data transfer.
   - **crc16-ccitt.h/.c** - Calculation of CRC 16 CCITT polynomial.
 - **303/** - CANopen Recommendation
   - **CO_LEDs.h/.c** - CANopen LED Indicators
 - **305/** - CANopen layer setting services (LSS) and protocols.
   - **CO_LSS.h** - CANopen Layer Setting Services protocol (common).
   - **CO_LSSmaster.h/.c** - CANopen Layer Setting Service - master protocol.
   - **CO_LSSslave.h/.c** - CANopen Layer Setting Service - slave protocol.
 - **309/** - CANopen access from other networks.
   - **CO_gateway_ascii.h/.c** - Ascii mapping: NMT master, LSS master, SDO client.
 - **extra/**
   - **CO_trace.h/.c** - CANopen trace object for recording variables over time.
 - **example/** - Directory with basic example, should compile on any system.
   - **CO_driver_target.h** - Example hardware definitions for CANopenNode.
   - **CO_driver_blank.c** - Example blank interface for CANopenNode.
   - **CO_OD.h/.c** - CANopen Object dictionary. Automatically generated files.
   - **main_blank.c** - Mainline and other threads - example template.
   - **Makefile** - Makefile for example.
   - **IO.eds** - Standard CANopen EDS file, which may be used from CANopen
     configuration tool. Automatically generated file.
   - _ **project.xml** - XML file contains all data for CANopen Object dictionary.
     It is used by *Object dictionary editor* application, which generates other
     files.
   - _ **project.html** - *Object dictionary editor* launcher.
 - **socketCAN/** - Directory for Linux socketCAN interface.
   - **CO_driver_target.h** - Linux socketCAN specific definitions for CANopenNode.
   - **CO_driver.c** - Interface between Linux socketCAN and CANopenNode.
   - **CO_error.h/.c** - Linux socketCAN Error handling object.
   - **CO_error_msgs.h** - Error definition strings and logging function.
   - **CO_Linux_threads.h/.c** - Helper functions for implementing CANopen threads in Linux.
   - **CO_OD_storage.h/.c** - Object Dictionary storage object for Linux SocketCAN.
   - **CO_main_basic.c** - Mainline for socketCAN (basic usage).
 - **doc/** - Directory with documentation
   - **CHANGELOG.md** - Change Log file.
   - **deviceSupport.md** - Information about supported devices.
   - **gettingStarted.md, LSSusage.md, traceUsage.md** - Getting started and usage.
   - **index.html** - Soft link to html/md_README.html.
   - **html** - Directory with documentation - must be generated by Doxygen.
 - **CANopen.h/.c** - Initialization and processing of CANopen objects.
 - **codingStyle** - Example of the coding style.
 - **.clang-format** - Definition file for the coding style.
 - **Doxyfile** - Configuration file for the documentation generator *doxygen*.
 - **Makefile** - Makefile for Linux socketCAN.
 - **canopend** - Executable for Linux, build with `make`.
 - **LICENSE** - License.
 - **README.md** - This file.


Object dictionary editor
------------------------
Object Dictionary is one of the most important parts of CANopen. Its
implementation in CANopenNode is quite outdated and there are efforts to
rewrite it. Anyway, currently it is fully operational and works well.

To customize the Object Dictionary it is necessary to use external application:
[libedssharp](https://github.com/robincornelius/libedssharp). It can be used in
Windows or Linux with Mono.

Please note: since rearrangement in directory structure it is necessary to
manually update CO_OD.c file - it must include: `301/CO_driver.h`, `CO_OD.h` and
`301/CO_SDOserver.h`.


Device support
--------------
CANopenNode can run on many different devices. Each device (or microcontroller)
must have own interface to CANopenNode. CANopenNode can run with or without
operating system.

It is not practical to have all device interfaces in a single project. For that
reason CANopenNode project only includes interface to Linux socketCAN.
Interfaces to other microcontrollers are in separate projects. See
[deviceSupport.md](doc/deviceSupport.md) for list of known device interfaces.


Some details
------------
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

### Error control
When node is stated (in NMT operational state), it is allowed to send or
receive Process Data Objects (PDO). If Error Register (object 0x1001) is set,
then NMT operational state is not allowed.

### Power saving
All CANopen objects calculates next timer info for OS. Calculation is based on
various timers which expire in known time. Can be used to put microcontroller
into sleep and wake at the calculated time.


Change Log
----------
See [CHANGELOG.md](doc/CHANGELOG.md)


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
