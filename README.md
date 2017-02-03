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
 - Non-volatile storage.


Usage of the CANopenNode
------------------------
CANopenNode itself doesn't have complete working code for any microcontroller.
It is only the library with the stack and drivers for different
microcontrollers. It has example, which should compile on any system with
template driver (drvTemplate), which actually doesn't access CAN hardware.
CANopenNode should be used as a git submodule included in a project with
specific hardware and specific application.


Documentation, support and contributions
-------------------------
Code is documented in header files. Running
[doxygen](http://www.stack.nl/~dimitri/doxygen/index.html) in project
base folder will produce complete html documentation. Just open
CANopenNode/doc/html/index.html in browser.

Report issues on https://github.com/CANopenNode/CANopenNode/issues

Older and still active discussion group is on Sourceforge
http://sourceforge.net/p/canopennode/discussion/387151/

For some implementations of CANopenNode on real hardware see table below.
[CANopenSocket](https://github.com/CANopenNode/CANopenSocket) is nice
implementation for Linux devices. It includes command line interface for
master access of the CANopen network. There is also some Getting started.

Contributions are welcome. Best way to contribute your code is to fork
a project, modify it and then send a pull request.


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
~~~


File structure
--------------
 - **CANopen.h/.c** - Initialization and processing of CANopen objects. Most
   usual implementation of CANopen device.
 - **stack** - Directory with all CANopen objects in separate files.
   - **CO_Emergency.h/.c** - CANopen Emergency object.
   - **CO_NMT_Heartbeat.h/.c** - CANopen Network slave and Heartbeat producer object.
   - **CO_HBconsumer.h/.c** - CANopen Heartbeat consumer object.
   - **CO_SYNC.h/.c** - CANopen SYNC producer and consumer object.
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
   - **CO_OD.h/.c** - CANopen Object dictionary. Automatically generated file.
   - **IO.eds** - Standard CANopen EDS file, which may be used from CANopen
     configuration tool. Automatically generated file.
   - _ **project.xml** - XML file contains all data for CANopen Object dictionary.
     It is used by *Object dictionary editor* application, which generates other
     files. *Object dictionary editor* is currently fully  functional, but old
     web application. See http://sourceforge.net/p/canopennode/code_complete/.
   - _ **project.html** - *Object dictionary editor* launcher.

Microcontroller support
-----------------------

|                               | Status (date) | OD storage | Example |
|-------------------------------|:-------------:|:----------:|---------|
| drvTemplate                   | OK            | template   | [here](https://github.com/CANopenNode/CANopenNode) |
| socketCAN (Linux)             | beta   (2016) | Yes        | [CANopenSocket](https://github.com/CANopenNode/CANopenSocket) |
| Microchip PIC32 (MPLABX)      | stable (2015) | Yes        | [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) |
| Microchip PIC24, 33 (MPLABX)  | stable (2015) | no         | [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) |
| Microchip dsPIC30F (MPLABX)   | beta   (2013) | no         | [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) |
| LPC1768 (MBED)                | beta   (2016) | no         | [exmachina](https://github.com/exmachina-dev/CANopenMbed) |
| RTOS eCos                     | stable (2013) | Yes        | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| Atmel SAM3X                   | ?             | Yes        | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| ST STM32                      | ?             | no         | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| NXP LPC177x_8x                | ?             | no         | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |
| Freescale MCF5282             | ?             | no         | [old repo](http://sourceforge.net/p/canopennode/code_complete/) |


### Other known implementations with source code
 - AD ADSP-CM408 mixed signal controller Contributed by Analog devices, Inc. (dec 2014)
   http://sourceforge.net/projects/canopennode-for-adsp-cm408f/
 - Discontinued implementations from earlier versions of CANopenNode
   - Microchip PIC18F
   - BECK IPC Embedded Web-Controller SC243


### Note for contributors
 - Implementations for some other microcontrollers may go into CANopenNode.
 - Each implementation should have a basic example, which should run on
   implemented microcontroller.
 - Use drvTemplate for template. Another template may be PIC32 microcontroller,
   code for it is most maintained.
 - Remove documentation comments from CO_driver.h (from drvTemplate). Make
   only basic comments, same as in PIC32.
 - Keep styling in CO_driver.(ch) unchanged, so files are easy comparable.
 - In case of some (minor) changes in CANopenNode interface, change will
   be updated for all microcontrollers.


History of the project
----------------------
Project was initially hosted on http://sourceforge.net/projects/canopennode/
It started in 2004 with PIC18F microcontrollers from Microchip.
Fresh, cleaned repository of CANopenNode stack started on 25.7.2015.
For older history see http://sourceforge.net/p/canopennode/code_complete/


License
-------
CANopenNode is distributed under the terms of the GNU General Public
License version 2 with the classpath exception.

CANopenNode is free and open source software: you can redistribute
it and/or modify it under the terms of the GNU General Public License
as published by the Free Software Foundation, either version 2 of the
License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see http://www.gnu.org/licenses/.

### GPL linking exception
CANopenNode can be easely used also in commercial embedded projects.
Following clarification and special
[linking exception to the GNU General Public License](https://en.wikipedia.org/wiki/GPL_linking_exception)
is included to the distribution terms of CANopenNode:

Linking this library statically or dynamically with other modules is
making a combined work based on this library. Thus, the terms and
conditions of the GNU General Public License cover the whole combination.

As a special exception, the copyright holders of this library give
you permission to link this library with independent modules to
produce an executable, regardless of the license terms of these
independent modules, and to copy and distribute the resulting
executable under terms of your choice, provided that you also meet,
for each linked independent module, the terms and conditions of the
license of that module. An independent module is a module which is
not derived from or based on this library. If you modify this
library, you may extend this exception to your version of the
library, but you are not obliged to do so. If you do not wish
to do so, delete this exception statement from your version.
