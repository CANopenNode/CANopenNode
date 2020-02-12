LSS usage
=========

LSS (Layer settings service) is an extension to CANopen described in CiA DSP 305. The
interface is described in CiA DS 309 2.1.0 (ASCII mapping).
LSS allows the user to change node ID and bitrate, as well as setting the node ID on an
unconfigured node.

LSS uses the the OD Identity register as an unique value to select a node. Therefore
the LSS address always consists of four 32 bit values. This also means that LSS relies
on this register to actually be unique.

To use LSS, a compatible node is needed. Note that canopend only includes LSS master
functionality.

The following example show some typical use cases for LSS:

 - Changing the node ID for a known slave, store the new node ID to eeprom, apply new node ID.
   The node currently has the node ID 22.

        $ ./canopencomm lss_switch_sel 0x00000428 0x00000431 0x00000002 0x5C17EEBC
        $ ./canopencomm lss_set_node 4
        $ ./canopencomm lss_store
        $ ./canopencomm lss_switch_glob 0
        $ ./canopencomm 22 reset communication

   Note that the node ID change is not done until reset communication/node

 - Changing the node ID for a known slave, store the new node ID to eeprom, apply new node ID.
   The node currently has an invalid node ID.

        $ ./canopencomm lss_switch_sel 0x00000428 0x00000431 0x00000002 0x5C17EEBC
        $ ./canopencomm lss_set_node 4
        $ ./canopencomm lss_store
        $ ./canopencomm lss_switch_glob 0

   Note that the node ID is automatically applied.

 - Search for a node via LSS fastscan, store the new node ID to eeprom, apply new node ID

        $ ./canopencomm [1] _lss_fastscan

        [1] 0x00000428 0x00000432 0x00000002 0x6C81413C

        $ ./canopencomm lss_set_node 4
        $ ./canopencomm lss_store
        $ ./canopencomm lss_switch_glob 0

    To increase scanning speed, you can use

        $ ./canopencomm [1] _lss_fastscan 25

    where 25 is the scan step delay in ms. Be aware that the scan will become unreliable when
    the delay is set to low.

 - Auto enumerate all nodes via LSS fastscan. Enumeration automatically begins at node ID 2
   and node ID is automatically stored to eeprom. Like with _lss_fastscan, an optional
   parameter can be used to change default delay time.

        $ ./canopencomm lss_allnodes

        [1] OK, found 3 nodes starting at node ID 2.

 - To get further control over the fastscan process, the lss_allnodes command supports
   an extended parameter set. If you want to use this set, all parameters are mandatory.
   Auto enumerate all nodes via LSS fastscan. Set delay time to 25ms, set enumeration start
   to node ID 7, do not store LSS address in eeprom, enumerate only devices with vendor ID
   "0x428", ignore product code and software revision, scan for serial number

        $ ./canopencomm lss_allnodes 25 7 0 2 0x428 1 0 1 0 0 0

        [1] OK, found 2 nodes starting at node ID 7.

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
