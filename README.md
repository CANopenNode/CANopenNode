CANopenNode
===========

CANopenNode is free and open source CANopen protocol stack.

CANopen is the internationally standardized (EN 50325-4) ([CiA301](http://can-cia.org/standardization/technical-documents)) higher-layer protocol for embedded control system built on top of CAN. For more information on CANopen see http://www.can-cia.org/

CANopenNode is written in ANSI C in object-oriented way. It runs on different microcontrollers, as standalone application or with RTOS.

Variables (communication, device, custom) are collected in CANopen Object Dictionary and are accessible from both: C code and from CANopen network.

CANopenNode homepage is https://github.com/CANopenNode/CANopenNode

This is version 4 of CANopenNode with new Object Dictionary implementation. For older versions `git checkout` branches `v1.3-master` or `v2.0-master`.


Characteristics
---------------
### CANopen
 - [Object Dictionary](https://www.can-cia.org/can-knowledge/canopen/device-architecture/) offers clear and flexible organisation of any variables. Variables can be accessed directly or via read/write functions.
 - [NMT](https://www.can-cia.org/can-knowledge/canopen/network-management/) slave to start, stop, reset device. Simple NMT master.
 - [Heartbeat](https://www.can-cia.org/can-knowledge/canopen/error-control-protocols/) producer/consumer error control for monitoring of CANopen devices.
 - [PDO](https://www.can-cia.org/can-knowledge/canopen/pdo-protocol/) for broadcasting process data with high priority and no protocol overhead. Variables from Object Dictionary can be dynamically mapped to the TPDO, which is then transmitted according to communication rules and received as RPDO by another device.
 - [SDO](https://www.can-cia.org/can-knowledge/canopen/sdo-protocol/) server enables expedited, segmented and block transfer access to all Object Dictionary variables inside CANopen device.
 - [SDO](https://www.can-cia.org/can-knowledge/canopen/sdo-protocol/) client can access any Object Dictionary variable on any CANopen device inside the network.
 - [Emergency](https://www.can-cia.org/can-knowledge/canopen/special-function-protocols/) message producer/consumer.
 - [Sync](https://www.can-cia.org/can-knowledge/canopen/special-function-protocols/) producer/consumer enables network synchronized transmission of the PDO objects, etc.
 - [Time-stamp](https://www.can-cia.org/can-knowledge/canopen/special-function-protocols/) producer/consumer enables date and time synchronization in millisecond resolution.
 - [LSS](https://www.can-cia.org/can-knowledge/canopen/cia305/) CANopen node-id and bitrate setup, master and slave, LSS fastscan.
 - [CANopen gateway](https://www.can-cia.org/can-knowledge/canopen/cia309/), CiA309-3 Ascii command interface for NMT master, LSS master and SDO client.
 - CANopen Safety, EN 50325-5, CiA304, "PDO like" communication in safety-relevant networks

### Other
 - [Suitable for 16-bit microcontrollers and above](#device-support)
 - [Multithreaded, real-time](#canopenNode-flowchart)
 - [Object Dictionary editor](#object-dictionary-editor)
 - Non-volatile storage for Object Dictionary or other variables. Automatic or controlled by standard CANopen commands, configurable.
 - [Power saving possible](#power-saving)
 - [Bootloader possible](https://github.com/CANopenNode/CANopenNode/issues/111) (for firmware update)


Related projects
----------------
 - [CANopenNode](https://github.com/CANopenNode/CANopenNode) (this project): CANopen protocol stack, base for CANopen device. It contains no device specific code (drivers), which must be added separately for each target system. An example shows the basic principles, compiles on any system, but does not connect to any CAN hardware.
 - [CANopenDemo](https://github.com/CANopenNode/CANopenDemo): Demo device with CANopenNode and different target systems, tutorial and testing tools.
 - [CANopenNode.github.io](https://github.com/CANopenNode/CANopenNode.github.io): Html documentation, compiled by doxygen, for CANopenDemo, CANopenNode and other devices, available also online: https://canopennode.github.io
 - [CANopenEditor](https://github.com/CANopenNode/CANopenEditor): Object Dictionary editor, external GUI tool for editing CANopen Object Dictionary for custom device. It generates C source code, electronic data sheet and documentation for the device. It is a fork from [libedssharp](https://github.com/robincornelius/libedssharp).
 - [CANopenLinux](https://github.com/CANopenNode/CANopenLinux): CANopenNode on Linux devices. It can be a basic CANopen device or more advanced with commander functionalities.
 - [CANopenPIC](https://github.com/CANopenNode/CANopenPIC): CANopenNode on PIC microcontrollers from Microchip. Works with 16-bit and 32 bit devices. Includes example for Arduino style [Max32](https://reference.digilentinc.com/reference/microprocessor/max32/start) board.
 - [doc/deviceSupport.md](doc/deviceSupport.md): List of other implementations of CANopenNode on different devices.


Documentation, support and contributions
----------------------------------------
All code is documented in the source header files. Some additional documents are in `doc` directory.

To generate complete html documentation, run [doxygen](http://www.doxygen.nl/) in the project base directory: `sudo apt install doxygen graphviz pdf2svg; doxygen > /dev/null`

Complete generated documentation is also available online: https://canopennode.github.io

Tutorial, demo device and tests are available in [CANopenDemo](https://github.com/CANopenNode/CANopenDemo) repository.

Report issues on https://github.com/CANopenNode/CANopenNode/issues

Older discussion group is on Sourceforge: http://sourceforge.net/p/canopennode/discussion/387151/

Contributions are welcome. Best way to contribute your code is to fork a project, modify it and then send a pull request. Some basic formatting
rules should be followed: Linux style with indentation of 4 spaces. There is also a `codingStyle` file with example and a configuration file for `clang-format` tool.


CANopenNode flowchart
---------------------
Flowchart of a typical CANopenNode implementation:
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
 ----------------------    ------------------------    -----------------------
| CAN receive thread   |  | Timer interval thread  |  | Mainline thread       |
|                      |  |                        |  |                       |
| - Fast response.     |  | - Realtime thread with |  | - Processing of time  |
| - Detect CAN ID.     |  |   constant interval,   |  |   consuming tasks     |
| - Partially process  |  |   typically 1ms.       |  |   in CANopen objects: |
|   messages and copy  |  | - Network synchronized |  |    - SDO server,      |
|   data to target     |  | - Copy inputs (RPDOs,  |  |    - Emergency,       |
|   CANopen objects.   |  |   HW) to Object Dict.  |  |    - Network state,   |
|                      |  | - May call application |  |    - Heartbeat.       |
|                      |  |   for some processing. |  |    - LSS slave        |
|                      |  | - Copy variables from  |  | - Gateway (optional): |
|                      |  |   Object Dictionary to |  |    - NMT master       |
|                      |  |   outputs (TPDOs, HW). |  |    - SDO client       |
|                      |  |                        |  |    - LSS master       |
|                      |  |                        |  | - May cyclically call |
|                      |  |                        |  |   application code.   |
 ----------------------    ------------------------    -----------------------
~~~

All code of the CANopenNode is non-blocking. Code in source files is collected into objects. Parts of the code can be enabled/disabled, so only files and parts of code can be used, which are required for the project. See stack configuration in 301/CO_config.h file.

For most efficiency code can run in different thread as seen in above flowchart. This is suitable for microcontrollers. It is also possible to run everything from single thread, as available on Linux devices. Code includes mechanisms, which triggers processing of OD objects when necessary.

In CANopen initialization section all CANopen objects are initialized. In run time CANopen objects are processed cyclically.

Files CANopen.h and CANopen.c is a joint of all CANopen objects. It may seems complex, but offers some flexibility and is suitable for most common configurations of the CANopen objects. CANopen objects can be defined in global space or can be dynamically allocated. Object dictionary can be used default (OD.h/.c files), but configuration with multiple object dictionaries is also possible by using the #CO_config_t structure. CANopen.h and CANopen.c files can also be only a reference for more customized implementation of CANopenNode based device.

Object Dictionary is a collection of all network accessible variables and offers most flexible usage. OD variables can be initialized by object dictionary or application can specify own read/write access functions for specific OD variables. Groups of OD variables are also able to be stored to non-volatile memory, either on command or automatically.


File structure
--------------
 - **301/** - CANopen application layer and communication profile.
   - **CO_config.h** - Configuration macros for CANopenNode.
   - **CO_driver.h** - Interface between CAN hardware and CANopenNode.
   - **CO_ODinterface.h/.c** - CANopen Object Dictionary interface.
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
 - **304/** - CANopen Safety.
   - **CO_SRDO.h/.c** - CANopen Safety-relevant Data Object protocol.
   - **CO_GFC.h/.c** - CANopen Global Failsafe Command (producer and consumer).
 - **305/** - CANopen layer setting services (LSS) and protocols.
   - **CO_LSS.h** - CANopen Layer Setting Services protocol (common).
   - **CO_LSSmaster.h/.c** - CANopen Layer Setting Service - master protocol.
   - **CO_LSSslave.h/.c** - CANopen Layer Setting Service - slave protocol.
 - **309/** - CANopen access from other networks.
   - **CO_gateway_ascii.h/.c** - Ascii mapping: NMT master, LSS master, SDO client.
 - **storage/**
   - **CO_storage.h/.c** - CANopen data storage base object.
   - **CO_storageEeprom.h/.c** - CANopen data storage object for storing data into block device (eeprom).
   - **CO_eeprom.h** - Eeprom interface for use with CO_storageEeprom, functions are target system specific.
 - **extra/**
   - **CO_trace.h/.c** - CANopen trace object for recording variables over time.
 - **example/** - Directory with basic example, should compile on any system.
   - **CO_driver_target.h** - Example hardware definitions for CANopenNode.
   - **CO_driver_blank.c** - Example blank interface for CANopenNode.
   - **main_blank.c** - Mainline and other threads - example template.
   - **CO_storageBlank.h/.c** - Example blank demonstration for data storage to non-volatile memory.
   - **Makefile** - Makefile for example.
   - **DS301_profile.xpd** - CANopen device description file for DS301. It includes also CANopenNode specific properties. This file is also available in Profiles in Object dictionary editor.
   - **DS301_profile.eds**, **DS301_profile.md** - Standard CANopen EDS file and markdown documentation file, automatically generated from DS301_profile.xpd.
   - **OD.h/.c** - CANopen Object dictionary source files, automatically generated from DS301_profile.xpd.
 - **doc/** - Directory with documentation
   - **CHANGELOG.md** - Change Log file.
   - **deviceSupport.md** - Information about supported devices.
   - **objectDictionary.md** - Description of CANopen object dictionary interface.
   - **CANopenNode.png** - Little icon.
   - **html** - Directory with documentation - must be generated by Doxygen.
 - **CANopen.h/.c** - Initialization and processing of CANopen objects, suitable for common configurations.
 - **codingStyle** - Example of the coding style.
 - **Doxyfile** - Configuration file for the documentation generator *doxygen*.
 - **LICENSE** - License.
 - **README.md** - This file.


Object dictionary editor
------------------------
Object Dictionary is one of the most essential parts of CANopen.

To customize the Object Dictionary it is necessary to use external application: [CANopenEditor](https://github.com/CANopenNode/CANopenEditor). Latest pre-compiled [binaries](https://github.com/CANopenNode/CANopenEditor/archive/refs/heads/build.zip) are also available. Just extract the zip file and run the `EDSEditor.exe`. In Linux it runs with mono, which is available by default on Ubuntu. Just set file permissions to "executable" and then execute the program.

In program, in preferences, set exporter to "CANopenNode_V4". Then start new project or open the existing project file.

Many project file types are supported, EDS, XDD v1.0, XDD v1.1, old custom XML format. Generated project file can then be saved in XDD v1.1 file format (xmlns="http://www.canopen.org/xml/1.1"). Project file can also be exported to other formats, it can be used to generate documentation and CANopenNode source files for Object Dictionary.

If new project was started, then `DS301_profile.xpd` may be inserted. If existing (old) project is edited, then existing `Communication Specific Parameters` may be deleted and then new `DS301_profile.xpd` may be inserted. Alternative is editing existing communication parameters with observation to Object Dictionary Requirements By CANopenNode in [objectDictionary.md](doc/objectDictionary.md).

To clone, add or delete, select object(s) and use right click. Some knowledge of CANopen is required to correctly set-up the custom Object Dictionary. Separate objects can also be inserted from another project.

CANopenNode includes some custom properties inside standard project file. See [objectDictionary.md](doc/objectDictionary.md) for more information.


Device support
--------------
CANopenNode can run on many different devices. Each device (or microcontroller) must have own interface to CANopenNode. CANopenNode can run with or without operating system.

It is not practical to have all device interfaces in a single project. Interfaces to other microcontrollers are in separate projects. See [deviceSupport.md](doc/deviceSupport.md) for list of known device interfaces.


Some details
------------
### RTR
RTR (remote transmission request) is a feature of CAN bus. Usage of RTR is not recommended for CANopen and it is not implemented in CANopenNode.

### Error control
When node is started (in NMT operational state), it is allowed to send or receive Process Data Objects (PDO). If Error Register (object 0x1001) is set, then NMT operational state may not be allowed.

### Power saving
All CANopen objects calculates next timer info for OS. Calculation is based on various timers which expire in known time. Can be used to put microcontroller into sleep and wake at the calculated time.


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
