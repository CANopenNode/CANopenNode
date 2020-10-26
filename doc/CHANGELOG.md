Change Log
==========

[Unreleased master]
-------------------------
- [Full ChangeLog](https://github.com/CANopenNode/CANopenNode/compare/v1.3...master)
### Removed
- All drivers removed from this project, except Neuberger-socketCAN for Linux.
### Changed
- Directory structure rearranged. Before was all CANopen object files in `stack` directory, now they are in separate directories according to standard (`301`, `305`, `extra`, `socketCAN` for Linux driver). Include directives for that files now contain directory path. `CO_SDO` renamed to `CO_SDOserver` and `CO_SDOmaster` renamed to `CO_SDOclient`. Change of the project files will be necessary.
- Driver interface clarified. Before was pair of CO_driver.h/.c files for each microcontroller, now there is common CO_driver.h file. Drivers for other microcontrollers will be separate projects. Each driver must have own CO_driver_target.h file and function definitions from C_driver.h file. See documentation in CO_driver.h, example/CO_driver_target.h and example/CO_driver.c. There was no other major changes in driver interface.
- Time base is now microsecond in all functions.
- CANopen.h/.c files simplified and changed. `CO_USE_GLOBALS` and `CO_init()` removed. Interface to those functions changed.
- `CO_NMT_sendCommand()` master function renamed and moved from CANopen.c into CO_NMT_Heartbeat.c.
- Heartbeat consumer `CO_HBconsumer_process()` optimized.
- Rename in CO_driver_target.h: `IS_CANrxNew` -> `CO_FLAG_READ`, `SET_CANrxNew` -> `CO_FLAG_SET`, `CLEAR_CANrxNew` -> `CO_FLAG_CLEAR`
- CO_driver.h file, function `CO_CANrxBufferInit()`, last argument (callback) changed from `(*pFunct)(void *object, const CO_CANrxMsg_t *message)` to `void (*CANrx_callback)(void *object, void *message)`. New functions are defined in `CO_driver_target.h` file: `CO_CANrxMsg_readIdent()`, `CO_CANrxMsg_readDLC()` and `CO_CANrxMsg_readData()`.
- It is necessary to manually update CO_OD.c file - it must include: `301/CO_driver.h`, `CO_OD.h` and `301/CO_SDOserver.h`.
- Added `void *object` argument to CO_*_initCallback() functions. API clarified.
- Add emergency receive callback also for own emergency messages.
- Heartbeat is send immediately after NMT state changes.
- SDO client is rewritten. Now includes read/write fifo interface to transfer data.
- LED indicator indication (CiA303-3) moved from NMT into own files. Now fully comply to standard.
- LSS slave is integrated into CANopenNode more directly.
- CO_driver interface: remove Emergency object dependency for reporting CAN errors, use CANerrorStatus own variable instead. Emergency object updated.
### Changed SocketCAN
- ./stack/socketCAN removed from the project, ./stack/Neuberger-socketCAN moved to ./socketCAN
- driver API updated
- CO_Linux_threads.h, function `void CANrx_threadTmr_init(uint16_t interval_in_milliseconds (changed to) uint32_t interval_in_microseconds)`
- CO_CANrxBufferInit(): remove check COB ID already used.
- change macros CO_DRIVER_MULTI_INTERFACE and CO_DRIVER_ERROR_REPORTING. To enable(disable), set to 1(0).
- Rename CO_Linux_threads.h/.c to CO_epoll_interface.h/.c and reorganize them. Move epoll, timerfd and eventfd system calls from CO_driver.c to here.
- Can run in single thread, including gateway.
### Fixed
- Bugfix in `CO_HBconsumer_process()`: argument `timeDifference_us` was set to 0 inside for loop, fixed now.
- BUG in CO_HBconsumer.c #168
### Added
- Documentation added to `doc` directory: CHANGELOG.md, deviceSupport.md, gettingStarted.md, LSSusage.md and traceUsage.md.
- All CANopen objects calculates next timer info for OS. Useful for energy saving.
- Added file CO_config.h for stack configuration. Can be overridden by target specific or by custom definitions. It enables/disables whole CanOpenNode objects or parts of them. It also specifies some constants.
- CO_fifo.h/c for fifo data buffer, used with rewritten SDO client, etc.
- CANopen gateway-ascii command interface according to CiA309-3 as a microcontroller independent module. It includes NMT master, LSS master and SDO client interface. Interface is non-blocking, it is added to mainline. Example for Linux stdio and socket is included.

[v1.3] - 2020-04-27
-------------------
- [Full ChangeLog](https://github.com/CANopenNode/CANopenNode/compare/v1.2...v1.3)
### Changed
- License changed to Apache 2.0.
- NMT self start functionality (OD object 1F80) implemented to strictly follow standard. Default value for object 1F80 have to be updated in OD editor. See README.md.
### Fixed
- Various fixes.
- neuberger-socketCAN fixed.
### Added
- CANopen TIME protocol added.

[v1.2] - 2019-10-08
-------------------
- [Full ChangeLog](https://github.com/CANopenNode/CANopenNode/compare/v1.1...v1.2)
### Fixed
- Memory barrier implemented for setting/clearing flags for CAN received message.
- CO_Emergency and CO_HBconsumer files revised.
### Added
- CANopen LSS master/slave protocol added for configuration of bitrate and node ID.
- Neuberger-socketCAN driver added.
- Emergency consumer added.
- Callbacks added to Emergency and Heartbeat consumer.

[v1.1] - 2019-10-08
-------------------
- [Full ChangeLog](https://github.com/CANopenNode/CANopenNode/compare/v1.0...v1.1)
- Bugfixes. Some non-critical warnings in stack, some formatting warnings in tracing stuff.

[v1.0] - 2017-08-01
-------------------
- [Full ChangeLog](https://github.com/CANopenNode/CANopenNode/compare/v0.5...v1.0)
- Stable.

[v0.5] - 2015-10-20
-------------------
- Git repository started on GitHub.

[v0.4] - 2012-02-26
-------------------
- Git repository started on Sourceforge git.

[v0.1] - 2004-06-29
-------------------
- First edition of CANopenNode on SourceForge, files section. (V0.80 on SourceForge).

------

Changelog written according to recommendations from https://keepachangelog.com/

[Unreleased master]: https://github.com/CANopenNode/CANopenNode
[v1.3]: https://github.com/CANopenNode/CANopenNode/tree/v1.3
[v1.2]: https://github.com/CANopenNode/CANopenNode/tree/v1.2
[v1.1]: https://github.com/CANopenNode/CANopenNode/tree/v1.1
[v1.0]: https://github.com/CANopenNode/CANopenNode/tree/v1.0
[v0.5]: https://github.com/CANopenNode/CANopenNode/tree/v0.5
[v0.4]: https://sourceforge.net/p/canopennode/code_complete/ci/master/tree/
[v0.1]: https://sourceforge.net/projects/canopennode/files/canopennode/CANopenNode-0.80/
