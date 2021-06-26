Device Support
==============

CANopenNode can run on many different devices. There are possible many different implementations on many different hardware, with many different development tools, by many different developers. It is not possible for single project maintainer to keep all the hardware interfaces updated. For that reason all hardware specific files are not part of the CANopenNode project.

It is necessary to implement interface to specific hardware. Interface to Linux socketCAN is part of this projects. Interfaces to other controllers are separate projects. There are interfaces to: Zephyr RTOS, PIC, Mbed-os RTOS + STM32, NXP, etc.


Note for device driver contributors
-----------------------------------
Most up-to-date implementations of CANopenNode are: [CANopenLinux](https://github.com/CANopenNode/CANopenLinux) and [CANopenPIC](https://github.com/CANopenNode/CANopenPIC) for PIC32 microcontroller (bare-metal). Those can be used for reference. There is also an example directory, which doesn't include specific device interface. It should compile on any system and can be used as a template. Device interface is documented in common CO_driver.h file.

There are many advantages of sharing the base code such as this. For the driver developers, who wish to share and cooperate, I recommend the following approach:
1. Make own git repo for the Device specific demo project on the Github or somewhere.
2. Add https://github.com/CANopenNode/CANopenNode into your project (or at side of your project). For example, include it in your project as a git submodule: `git submodule add https://github.com/CANopenNode/CANopenNode`
3. Add specific driver and other files.
4. Write a good README.md file, where you describe your project, specify demo board, tools used, etc.
5. Optionally prepare a demoDevice in [CANopenDemo](https://github.com/CANopenNode/CANopenDemo) repository and run the tests.
6. Share your work: add description of new device into this file (deviceSupport.md) and make a pull request to CANopenNode. Alternatively create an issue for new device on https://github.com/CANopenNode/CANopenNode/issues.
7. Offer your work for inclusion under the CANopenNode project and become its developer. It will increase code quality and functionality.


Linux
-----
* CANopenNode integration with Linux socketCAN with master command interface. SocketCAN is part of the Linux kernel.
* https://github.com/CANopenNode/CANopenLinux.
* CANopenNode version: (v4.0)
* Status: stable
* Features: OD storage, error counters, master (SDO client, LSS master, NMT master)
* Systems: Linux PC, Raspberry PI, etc.
* Development tools: Linux
* Information updated 2021-05-21


PIC32, dsPIC30, dsPIC33
-----------------------
* CANopenNode integration with 16 and 32 bit PIC microcontrollers from Microchip.
* https://github.com/CANopenNode/CANopenPIC
* CANopenNode version: (v4.0)
* Status: stable
* Features: OD storage for PIC32, SDO client demo for PIC32, error counters
* Development tools: MPLAB X
* Demo hardware: Explorer 16 from Microchip, [Max32 board](https://reference.digilentinc.com/reference/microprocessor/max32/start)
* Information updated 2021-05-07


Zephyr RTOS
-----------
* CANopenNode integration with Zephyr RTOS
* https://github.com/zephyrproject-rtos/zephyr/tree/master/subsys/canbus/canopen
* Example integration: https://docs.zephyrproject.org/latest/samples/subsys/canbus/canopen/README.html
* CANopenNode version: v1.3
* Status: stable
* Features: OD storage, LED indicators, Program Download, SDO server demo for Zephyr RTOS
* Development tools: Zephyr SDK
* Demo hardware: Any development board with CAN interface and Zephyr support (see https://docs.zephyrproject.org/latest/boards/index.html)
* Information updated 2020-09-28


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


Kinetis K20 (NXP Arm)
---------------------
* CANopenNode driver for Teensy3 (Kinetis K20 Arm)
* Driver source: https://github.com/c0d3runn3r/CANopenNode/tree/add-k20-driver/stack/ARM_Kinetis_K20_teensy
* Discussion: https://github.com/CANopenNode/CANopenNode/pull/28
* CANopenNode version: v1.0, 2017-08-01
* Status: seems to be stable
* Features: error counters, ?
* Development tools: (see [readme](https://github.com/c0d3runn3r/CANopenNode/tree/add-k20-driver/stack/ARM_Kinetis_K20_teensy))
* Demo hardware: Teensyduino, [Teensy3](https://www.pjrc.com/store/teensy32.html)
* Information updated 2020-03-02


S32DS (NXP S32 Design studio for Arm or Powerpc)
------------------------------------------------
* CANopenNode driver for S32DS and supported devices
* Driver source: https://github.com/bubi-soft/CANopenNode/tree/S32_SDK_support/stack/S32_SDK
* Documentation: included in driver, example available
* CANopenNode version: v1.0, 2017-08-01
* Status: seems to be stable
* Features: error counters, ?
* Development tools: [NXP S32 Design studio for Arm or Powerpc](https://www.nxp.com/design/software/development-software/s32-design-studio-ide:S32-DESIGN-STUDIO-IDE?&fsrch=1&sr=1&pageNum=1)
* Demo hardware: [S32K144EVB-Q100](https://www.nxp.com/design/development-boards/automotive-development-platforms/s32k-mcu-platforms/s32k144-evaluation-board:S32K144EVB), [DEVKIT-MPC5748G](https://www.nxp.com/design/development-boards/automotive-development-platforms/mpc57xx-mcu-platforms/mpc5748g-development-board-for-secure-gateway:DEVKIT-MPC5748G)
* Information updated 2020-03-02


Other
-----
* [ESP32](https://github.com/CANopenNode/CANopenNode/issues/198#issuecomment-658429391), 2020-07-14
* [FreeRTOS](https://github.com/martinwag/CANopenNode/tree/neuberger-freertos/stack/neuberger-FreeRTOS) by Neuberger, 2020-06-23, based on v1.3-master, see also [issue 198](https://github.com/CANopenNode/CANopenNode/issues/198).
* [STM32CubeMX HAL](https://github.com/w1ne/CANOpenNode-CubeMX-HAL), 2019-05-03, demo project for Atollic studio, tested on Nucleo STM32L452xx board.
* K64F_FreeRTOS, Kinetis SDK, 2018-02-13, [zip file](https://github.com/CANopenNode/CANopenNode/pull/28#issuecomment-365392867)
* LPC1768 (MBED) (released in 2016) - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), [known example from 2016](https://github.com/exmachina-dev/CANopenMbed)


Other old versions
------------------
* RTOS eCos - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in 2013, old repo](http://sourceforge.net/p/canopennode/code_complete/))
* Atmel SAM3X - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* ST STM32 - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* NXP LPC177x_8x - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* Freescale MCF5282 - [CANopenNode v1.0](https://github.com/CANopenNode/CANopenNode/tree/v1.0), ([released in old repo](http://sourceforge.net/p/canopennode/code_complete/))
* BECK IPC Embedded Web-Controller SC243 ([last release 2015, old repo](http://sourceforge.net/p/canopennode/code_complete/))
* Old Object_Dictionary_Editor, originally part of CANopenNode. It requires very old version of Firefox to run. Available on [Sourceforge](http://sourceforge.net/p/canopennode/code_complete/).
* AD ADSP-CM408 mixed signal controller Contributed by Analog devices, Inc. ([released in 2014](http://sourceforge.net/projects/canopennode-for-adsp-cm408f/))
* Microchip PIC18F ([last release 2006](https://sourceforge.net/projects/canopennode/files/canopennode/))
