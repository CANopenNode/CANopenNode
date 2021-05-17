Trace usage
===========

**TRACE DOES NOT WORK IN THE LAST VERSION**

CANopenNode includes optional trace functionality (non-standard). It monitors
choosen variables from Object Dictionary. On change of state of variable it
makes a record with timestamp into circular buffer. String with points can later
be read via SDO.

Trace is disabled by default. It can be enabled using Object Dictionary editor.
Include also *CO_trace.h/.c* into project, compile and run.

Here is en example of monitoring variable, connected with buttons
(OD_readInput8Bit, index 0x6000, subindex 0x01). It was tested on PIC32:

```
# Enable trace first:
./canopencomm 0x30 w 0x2400 0 u8 1

# Press and hold the button on Explorer16 and execute SDO read command:
./canopencomm 0x30 r 0x6000 1 u8
[1] 0x08
# It displays same value, as was transmitted via PDO and visible on candump.

# Now get the complete history for that buttons with timestamp for each change
# and store it as a text to the file:
./canopencomm 0x30 r 0x2401 5 vs > plot1.csv
cat plot1.csv
```
If large data blocks are transmitted via CAN bus, then more efficient SDO block
transfer can be enabled with command `./canopencomm set sdo_block 1`

For more info on using trace functionality see CANopenNode/example/IO.html
file. There is also a description of all Object Dictionary variables.

Trace functionality can also be configured on CANopenSocket directly. In that
case CANopenSocket must first receive PDO data from remote node(s) and store it
to the local Object Dictionary variable. CANopenSocket's trace then monitors
that variable. Text buffer is then read with the similar command as above. But
local SDO data access from CANopenSocket itself doesn't occupy CAN bus, so large
data is transfered realy fast. Besides that, Linux machine has much more RAM to
store the monitored data. Except timestamp is less accurate.

