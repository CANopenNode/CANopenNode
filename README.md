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
 - **301/** - CANopen application layer and communication profile.
   - **CO_driver.h** - Interface between CAN hardware and CANopenNode.
   - **CO_Emergency.h/.c** - CANopen Emergency protocol.
   - **CO_HBconsumer.h/.c** - CANopen Heartbeat consumer protocol.
   - **CO_NMT_Heartbeat.h/.c** - CANopen Network management and Heartbeat producer protocol.
   - **CO_PDO.h/.c** - CANopen Process Data Object protocol.
   - **CO_SDOclient.h/.c** - CANopen Service Data Object - client protocol (master functionality).
   - **CO_SDOserver.h/.c** - CANopen Service Data Object - server protocol.
   - **CO_SYNC.h/.c** - CANopen Synchronisation protocol (producer and consumer).
   - **CO_TIME.h/.c** - CANopen Time-stamp protocol.
   - **crc16-ccitt.h/.c** - Calculation of CRC 16 CCITT polynomial.
 - **305/** - CANopen layer setting services (LSS) and protocols.
   - **CO_LSS.h** - CANopen Layer Setting Services protocol (common).
   - **CO_LSSmaster.h/.c** - CANopen Layer Setting Service - master protocol.
   - **CO_LSSslave.h/.c** - CANopen Layer Setting Service - slave protocol.
 - **extra/**
   - **CO_trace.h/.c** - CANopen trace object for recording variables over time.
 - **example/** - Directory with basic example.
   - **main.c** - Mainline and other threads - example template.
   - **CO_OD.h/.c** - CANopen Object dictionary. Automatically generated files.
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
   - **CO_Linux_threads.h/.c** - Helper functions for implementing CANopen threads in Linux.
   - **CO_notify_pipe.h/.c** - Notify pipe for Linux threads.
   - **CO_OD_storage.h/.c** - Object Dictionary storage object for Linux SocketCAN.
 - **CANopen.h/.c** - Initialization and processing of CANopen objects.
 - **codingStyle** - Example of the coding style.
 - **.clang-format** - Definition file for the coding style.
 - **Doxyfile** - Configuration file for the documentation generator *doxygen*.
 - **Makefile** - Basic makefile.
 - **LICENSE** - License.
 - **README.md** - This file.


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


Change Log
----------
- [Unreleased split-driver](https://github.com/CANopenNode/CANopenNode/tree/split-driver): [Full Changelog](https://github.com/CANopenNode/CANopenNode/compare/master...split-driver)
  - All drivers removed from this project, except Neuberger-socketCAN for Linux.
  - Driver interface clarified, common CO_driver.h, specific CO_driver_target.h.
  - Directory structure rearranged. Change of the project files will be necessary.
  - Time base is now microsecond in all functions. All CANopen objects calculates next timer info for OS. CANopen.h/.c files revised. Compare the example/main.c file to view some differences in interface.
  - Heartbeat consumer optimized and fixed. CO_NMT_sendCommand() master function moved from CANopen.c into CO_NMT_Heartbeat.c.
  - Basic Linux socketCAN example.
- [Unreleased master](https://github.com/CANopenNode/CANopenNode): [Full Changelog](https://github.com/CANopenNode/CANopenNode/compare/v1.2...master)
  - License changed to Apache 2.0.
  - CANopen TIME protocol added.
  - Various fixes.
- **[v1.2](https://github.com/CANopenNode/CANopenNode/tree/v1.2)** - 2019-10-08: [Full Changelog](https://github.com/CANopenNode/CANopenNode/compare/v1.1...v1.2)
  - CANopen LSS master/slave protocol added for configuration for bitrate and node ID.
  - Memory barrier implemented for setting/clearing flags for CAN received message.
  - Neuberger-socketCAN driver added.
  - Emergency consumer added with callbacks. Emergency revised.
  - Heartbeat consumer revised, callbacks added.
- **[v1.1](https://github.com/CANopenNode/CANopenNode/tree/v1.1)** - 2019-10-08: Bugfixes. [Full Changelog](https://github.com/CANopenNode/CANopenNode/compare/v1.0...v1.1)
- **[v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0)** - 2017-08-01: Stable. [Full Changelog](https://github.com/CANopenNode/CANopenNode/compare/v0.5...v1.0)
- **[v0.5](https://github.com/CANopenNode/CANopenNode/tree/v0.5)** - 2015-10-20: Git repository started on GitHub.
- **[v0.4](https://sourceforge.net/p/canopennode/code_complete/ci/master/tree/)** - 2012-02-26: Git repository started on Sourceforge.
- **[v0.1](https://sourceforge.net/projects/canopennode/files/canopennode/CANopenNode-0.80/)** - 2004-06-29: First edition of CANopenNode on SourceForge.


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
