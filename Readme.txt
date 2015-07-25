CANopenNode is an opensource CANopen Stack.

CANopen is the internationally standardized (EN 50325-4) CAN-based higher-layer protocol for embedded control system. For more information on CANopen see http://www.can-cia.org/

Stack is written in ANSI C in object-oriented way. Code is documented. License is LGPL V2.1.

Variables (communication, device, custom) are ordered in CANopen object dictionary and are accessible from both: C code and from CAN network.


CANopen Features:
 - NMT slave to start, stop, reset device.
 - Heartbeat producer/consumer error control.
 - PDO linking and dynamic mapping for fast exchange of process variables.
 - SDO expedited, segmented and block transfer for service access to all parameters.
 - SDO master.
 - Emergency message.
 - Sync producer/consumer.
 - Nonvolatile storage.


Supported controllers:
 - Microchip: PIC32, PIC24H, dsPIC33F, dsPIC30F
 - Linux SocketCAN
 - eCos RTOS with all supported microcontrollers
 - Third parity contributors: LPC177x_8x, SAM3X, STM32

History of the project:
Project was initially hosted on http://sourceforge.net/projects/canopennode/
It started in 2004 with PIC18F microcontrollers from Microchip.
Fresh, cleaned repository of CANopenNode stack started on 25.7.2015.
For older history see http://sourceforge.net/p/canopennode/code_complete/
