Device Support
==============

CANopenNode can run on many different devices. There are possible many different implementations on many different hardware, with many different development tools, by many different developers. It is not possible for single project maintainer to keep all the hardware interfaces updated. For that reason all hardware specific files are not part of the CANopenNode project.

It is necessary to implement interface to specific hardware. Interface to Linux socketCAN is part of this projects. Interfaces to other controllers are separate projects. There are interfaces to: Zephyr RTOS, PIC, Mbed-os RTOS + STM32, etc.


Note for device driver contributors
-----------------------------------
Most up-to-date implementations of CANopenNode are: socketCAN for Linux, which is part of CANopenNode and [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) for PIC32 microcontroller (bare-metal). Those can be used for reference. There is also an example directory, which doesn't include specific device interface. It should compile on any system and can be used as a template. Device interface is documented in common CO_driver.h file.

There are many advantages of sharing the base code such as this. For the driver developers, who wish to share and cooperate, I recommend the following approach:
1. Make own git repo for the Device specific demo project on the Github or somewhere.
2. Add https://github.com/CANopenNode/CANopenNode into your project (or at side of your project). For example, include it in your project as a git submodule: `git submodule add https://github.com/CANopenNode/CANopenNode`
3. Add specific driver and other files.
4. Add description of new device into this file (deviceSupport.md) and make a pull request to CANopenNode. Alternatively create an issue for new device on https://github.com/CANopenNode/CANopenNode/issues.
5. Make a demo folder, which contains project files, etc., necessary to run the demo.
6. Write a good README.md file, where you describe your project, specify demo board, tools used, etc.


Linux
-----
* CANopenNode integration with Linux socketCAN with master command interface. SocketCAN is part of the Linux kernel.
* https://github.com/CANopenNode/CANopenNode (this project).
* CANopenNode version: (will be v2.0)
* Status: stable
* Features: OD storage, SDO client
* Systems: Linux PC, Raspberry PI, etc.
* Development tools: Linux
* Information updated 2020-02-14


PIC32, dsPIC30, dsPIC33
-----------------------
* CANopenNode integration with 16 and 32 bit PIC microcontrollers from Microchip.
* https://github.com/CANopenNode/CANopenPIC
* CANopenNode version: (near v2.0)
* Status: stable
* Features: OD storage, SDO client demo for PIC32, error counters
* Development tools: MPLAB X
* Demo hardware: Explorer 16 from Microchip
* Information updated 2020-02-14


Zephyr RTOS
-----------
* CANopenNode integration with Zephyr RTOS
* https://github.com/zephyrproject-rtos/zephyr/tree/master/subsys/canbus/canopen
* Example integration: https://docs.zephyrproject.org/latest/samples/subsys/canbus/canopen/README.html
* CANopenNode version:
* Status: stable
* Features: OD storage, LED indicators, SDO server demo for Zephyr RTOS
* Development tools: Zephyr SDK
* Demo hardware: Any development board with CAN interface and Zephyr support (see https://docs.zephyrproject.org/latest/boards/index.html)
* Information updated 2020-01-21


Mbed-os RTOS + STM32 (F091RC, L496ZG)
-------------------------------------
* CANopenNode integration with Mbed-os RTOS
* https://github.com/Alphatronics/mbed-os-example-canopen
* CANopenNode version:
* Status: Stable
* Features: OD storage, LED indicators, GPIO
* Development tools: Mbed CLI (gcc 7)
* Demo hardware: STM32 Nucleo F091RC (or similar) development board with CAN interface (see https://os.mbed.com/platforms/ST-Nucleo-F091RC)
* Information updated 2020-01-21


Kinetis K20 (ARM)
-----------------
* CANopenNode driver for Teensy3 (Kinetis K20 ARM)
* https://github.com/c0d3runn3r/CANopenNode/tree/add-k20-driver, https://github.com/CANopenNode/CANopenNode/pull/28
* CANopenNode version: 83f18edc (before V1.0)
* Status: ?
* Features: ?
* Development tools: (see [readme](https://github.com/c0d3runn3r/CANopenNode/tree/add-k20-driver/stack/ARM_Kinetis_K20_teensy))
* Demo hardware: Teensyduino, [Teensy3](https://www.pjrc.com/store/teensy32.html)
* Information updated 2020-02-14


Other old versions
------------------
* LPC1768 (MBED) (released in 2016) - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), [known example from 2016](https://github.com/exmachina-dev/CANopenMbed)
* RTOS eCos - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in 2013, old repo](http://sourceforge.net/p/canopennode/code_complete/))
* Atmel SAM3X - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* ST STM32 - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* NXP LPC177x_8x - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* Freescale MCF5282 - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* BECK IPC Embedded Web-Controller SC243 ([last release 2015, old repo](http://sourceforge.net/p/canopennode/code_complete/))
* Old Object_Dictionary_Editor, originally part of CANopenNode. It requires very old version of Firefox to run. Available on [Sourceforge](http://sourceforge.net/p/canopennode/code_complete/).
* AD ADSP-CM408 mixed signal controller Contributed by Analog devices, Inc. ([released in 2014](http://sourceforge.net/projects/canopennode-for-adsp-cm408f/))
* Microchip PIC18F ([last release 2006](https://sourceforge.net/projects/canopennode/files/canopennode/))
