LSS usage
=========

LSS (Layer settings service) is an extension to CANopen described in CiA DSP 305. The
interface is described in CiA DS 309 3.0.0 (ASCII mapping).
LSS allows the user to change node ID and bitrate, as well as setting the node ID on an
unconfigured node.

LSS uses the the OD Identity register (0x1018) as an unique value to select a node. Therefore
the LSS address always consists of four 32 bit values. This also means that LSS relies
on this register to actually be unique. (_vendorID_, _productCode_, _revisionNumber_ and
_serialNumber_ must be configured and unique on each device.)

### Preparation for testing on Linux virtual CAN
LSS can be tested on Linux virtual CAN, similar as in gettingStarted.md.

1. Open terminal, setup _vcan0_ and _cd_ to CANopenNode directory.
2. Make some unique CANopen devices:
   - Edit CO_OD.c, change initialization for identity, for example change line to `/*1018*/ {0x4, 0x1L, 0x2L, 0x3L, 0x4L},`
   - `make`
   - `mv canopend canopend4`
   - Edit CO_OD.c, for example: `/*1018*/ {0x4, 0x1L, 0x2L, 0x3L, 0x5L},`
   - `make`
   - `mv canopend canopend5`
   - Repeat this step and create three further "unique" CANopen devices.
3. Clear default OD storage file. We will use default (empty) storage for all instances:
   - `echo "-" > od_storage`
4. Run "master" with command interface and node-id = 1. Note that this device
   has enabled both: LSS master and LSS slave. But LSS master does not 'see' own LSS slave.
   - `make`
   - `./canopend vcan0 -i1 -c "stdio"`
5. Run one CANopen device with node-id=22 in own terminal:
   - `./canopend4 vcan0 -i22`
6. Run other unique CANopen devices with unconfigured node-id, each in own terminal:
   - `./canopend5 vcan0 -i0xFF`
   - `./canopend6 vcan0 -i0xFF`
   - `./canopend7 vcan0 -i0xFF`
   - `./canopend8 vcan0 -i0xFF`
7. Note that `lss_store` does not work in this example. For it to work, OD storage must be used properly.

### Typical usage of LSS
 - Changing the node ID for a known slave, store the new node ID to eeprom, apply new node ID.
   The node currently has the node ID 22.

        help lss

        lss_switch_sel 0x00000001 0x00000002 0x00000003 0x00000004
        lss_set_node 10
        lss_store
        lss_switch_glob 0
        22 reset communication

   Note that the node ID change is not done until reset communication/node.

 - Changing the node ID for a known slave, store the new node ID to eeprom, apply new node ID.
   The node currently has an invalid node ID.

        lss_switch_sel 0x00000001 0x00000002 0x00000003 0x00000005
        lss_set_node 11
        lss_store
        lss_switch_glob 0

   Note that the node ID is automatically applied. This can be seen on `candump`.

### LSS fastscan
 - Search for a node via LSS fastscan, store the new node ID to eeprom, apply new node ID

        _lss_fastscan

        [0] 0x00000001 0x00000002 0x00000003 0x00000006

        lss_set_node 12
        lss_store
        lss_switch_glob 0

    To increase scanning speed, you can use

        _lss_fastscan 25

    where 25 is the scan step delay in ms. Be aware that the scan will become unreliable when
    the delay is set to low.

    We won't configure this node now, reset LSS. Now we have 1+3 nodes operational in our example.

        lss_switch_glob 0

### Auto enumerate all nodes
 - Auto enumerate all nodes via LSS fastscan. Enumeration automatically begins at node ID 2
   and node ID is automatically stored to eeprom. Like with _lss_fastscan, an optional
   parameter can be used to change default delay time.

        lss_allnodes

        # Node-ID 2 assigned to: 0x00000001 0x00000002 0x00000003 0x00000007
        # Node-ID 3 assigned to: 0x00000001 0x00000002 0x00000003 0x00000008
        # Found 2 nodes, search finished.
        [0] OK

 - To get further control over the fastscan process, the lss_allnodes command supports
   an extended parameter set.
   Auto enumerate all nodes via LSS fastscan. Set delay time to 25ms, set enumeration start
   to node ID 7, do not store LSS address in eeprom, enumerate only devices with vendor ID
   "0x428", ignore product code and software revision, scan for serial number

        lss_allnodes 25 7 0 2 0x428 1 0 1 0 0 0

   The parameters are as following:
   - 25     scan step delay time in ms
   - 7      enumeration start
   - 0      store node IDs to eeprom; 0 = no, 1 = yes
   - 2      vendor ID scan selector; 0 = fastscan, 2 = match value in next parameter
   - 0x428  vendor ID to match
   - 1      product code scan selector; 0 = fastscan, 1 = ignore, 2 = match value in next parameter
   - 0      product code to match (ignored in this example)
   - 1      software version scan selector; 0 = fastscan, 1 = ignore, 2 = match value in next parameter
   - 0      software version to match (ignored in this example)
   - 0      serial number scan selector; 0 = fastscan, 1 = ignore, 2 = match value in next parameter
   - 0      serial number to match (not used in this example)

 Note that only unconfigured nodes (those without a valid node ID) will take part in
 fastscan!
