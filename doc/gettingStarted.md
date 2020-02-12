Getting Started
===============

CANopen
-------
Before getting started with CANopenNode you should be familiar with the CANopen.
CANopen is the internationally standardized CAN-based higher-layer protocol for embedded control system.
It is specified by CiA301 (or by EN 50325-4) standard. It can be freely downloaded from https://can-cia.org/groups/specifications/.
Some information about CAN and CANopen can be found on https://can-cia.org/can-knowledge/ website. Very efficient way to get familiar with CANopen is by reading a book, for example [Embedded Networking with CAN and CANopen](https://can-newsletter.org/engineering/engineering-miscellaneous/nr_e_cia_can_books_3-2008_emb_can_pfeiffer_120529).

CANopen itself is not a typical master/slave protocol. It is more like producer/consumer protocol. It is also possible to operate CANopen network without a master. For example, pre-configured process data objects (PDO) are transmitted from producers. Each PDO may be consumed by multiple nodes. Other useful CANopen functionalities of each CANopen device are also: Heartbeat producer and consumer, Emergency producer, Sync producer or consumer, Time producer or consumer, SDO server (Service data objects - serve variables from Object dictionary), NMT slave (network management - start or stop parts of communication), LSS slave (configuration of Node-Id and Bitrate).

CANopen network usually has one device with master functionalities for network configuration. It may have additional CANopen functionalities, such as: NMT master, LSS master, SDO client, Emergency consumer. Master functionalities in CANopenNode are implemented with Ascii command line interface according to standard CiA309-3.


CANopenNode on Linux
--------------------
CANopenNode should run on any Linux machine. Examples below was tested on Debian based machines, including **Ubuntu** and **Raspberry PI**. It is possible to run tests described below without real CAN interface, because Linux kernel already contains virtual CAN interface.

----

TODO update all below. (This is currently part of https://github.com/CANopenNode/CANopenSocket, but will become part of CANopenNode.)

CANopenSocket consists of two applications: **canopend**, which runs in background, and **canopencomm**, command interface for SDO and NMT master.


### canopend
**canopend** is an implementation of CANopen device with master functionality. It runs within three threads. Realtime thread processes CANopen SYNC and PDO objects. Mainline thread processes other non time critical objects. Both are nonblocking. Command interface thread is blocking. It accepts commands from socket connection from external application and executes master SDO and NMT tasks.


### canopencomm
**canopencomm** is the other end of the Command interface. It accepts text commands form arguments or from standard input or from file. It sends commands to *canopend* via socket, line after line. Received result is printed to standard output. It is implementation of the CiA 309 standard.


Getting started
---------------
We will run two instances of *CANopend*. First will be basic node with ID=4,
second, with nodeID = 3, will have master functionality.


### Get the project

Clone the project from git repository and get submodules:

    $ git clone https://github.com/CANopenNode/CANopenSocket.git
    $ cd CANopenSocket
    $ git submodule init
    $ git submodule update

(If you want to work on submodule CANopenNode, you can `cd CANopenNode`,
and apply git commands directly on it. Initially is in head detached state,
so you have to `git checkout master` first. Then you can control submodule
separately, for example `git remote add {yourName} {url-of-your-git-repository}`,
and `git pull {yourName} {yourbranch}`)


### First terminal: CAN dump

Prepare CAN virtual (or real) device:

    $ sudo modprobe vcan
    $ sudo ip link add dev vcan0 type vcan
    $ sudo ip link set up vcan0

Run candump from [can-utils](https://github.com/linux-can/can-utils):

    $ sudo apt-get install can-utils
    $ candump vcan0

It will show all CAN traffic on vcan0.


### Second terminal: canopend

Start second terminal, compile and start *canopend*.

    $ cd CANopenSocket/canopend
    $ make
    $ app/canopend --help
    $ app/canopend vcan0 -i 4 -s od4_storage -a od4_storage_auto

You should now see CAN messages on CAN dump terminal. Wait few seconds and
press CTRL-C.

    vcan0  704   [1]  00                        # Bootup message.
    vcan0  084   [8]  00 50 01 2F F3 FF FF FF   # Emergency message.
    vcan0  704   [1]  7F                        # Heartbeat messages
    vcan0  704   [1]  7F                        # one per second.

Heartbeat messages shows pre-operational state (0x7F). If you follow byte 4 of the
Emergency message into [CANopenNode/stack/CO_Emergency.h],
CO_EM_errorStatusBits, you will see under 0x2F "CO_EM_NON_VOLATILE_MEMORY",
which is generic, critical error with access to non volatile device memory.
This byte is CANopenNode specific. You can observe also first two bytes,
which shows standard error code (0x5000 - Device Hardware) or third byte,
which shows error register. If error register is different than zero, then
node is not able to enter operational and PDOs can not be exchanged with it.

You can follow the reason of the problem inside the source code. However,
there are missing non-default storage files. Add them and run it again.

    $ echo - > od4_storage
    $ echo - > od4_storage_auto
    $ app/canopend vcan0 -i 4 -s od4_storage -a od4_storage_auto

    vcan0  704   [1]  00
    vcan0  184   [2]  00 00                     # PDO message
    vcan0  704   [1]  05

Now there is operational state (0x05) and there shows one PDO on CAN
address 0x184. To learn more about PDOs, how to configure communication
and mapping parameters and how to use them see other sources of CANopen
documentation (For example article of PDO re-mapping procedure in [CAN
newsletter magazine, June 2016](http://can-newsletter.org/engineering/engineering-miscellaneous/160601_can-newsletter-magazine-june-2016) ).

Start also second instance of *canopend* (master on nodeID=3) in the same
window (*canopend terminal*). Use default od_storage files and default
socket for command interface.

    $ # press CTRL-Z
    $ bg
    $ app/canopend vcan0 -i 3 -c ""


### Third terminal: canopencomm

Start third terminal, compile and start canopencomm.

    $ cd CANopenSocket/canopencomm
    $ make
    $ ./canopencomm --help

#### SDO master

Play with it and also observe CAN dump terminal. First Heartbeat at
index 0x1017, subindex 0, 16-bit integer, on nodeID 4.

    $ ./canopencomm [1] 4 read 0x1017 0 i16
    $ ./canopencomm [1] 4 write 0x1017 0 i16 5000

In CAN dump you can see some SDO communication. You will notice, that
Heartbeats from node 4 are coming in 5 second interval now. You can do
the same also for node 3. Now store Object dictionary, so it will preserve
variables on next start of the program.

    $ ./canopencomm 4 w 0x1010 1 u32 0x65766173

You can read more about Object dictionary variables for this
CANopenNode in [canopend/CANopenSocket.html].


#### NMT master
If node is operational (started), it can exchange all objects, including
PDO, SDO, etc. In pre-operational, PDOs are disabled, SDOs works. In stopped
only NMT messages are accepted.

    $ ./canopencomm 4 preop
    $ ./canopencomm 4 start
    $ ./canopencomm 4 stop
    $ ./canopencomm 4 r 0x1017 0 i16 		# time out
    $ ./canopencomm 4 reset communication
    $ ./canopencomm 4 reset node
    $ ./canopencomm 3 reset node

In *canopend terminal* you see, that both devices finished. Further commands
are not possible. If you set so, last command can also reset computer.

#### Combining NMT commands into a single file

Create a `commands.txt` file, and for its content enter your commands.
Example:

    [1] 3 start
    [2] 4 start

Make canopencomm use that file:

    $ ./canopencomm -f commands.txt
    [1] OK
    [2] OK


### Next steps
Now you can learn more skills on CANopen from some other sources:
books, data sheet of some CANopen device, standard CiA 301(it's free), etc.
Then you can enter the big world of [CANopen devices](http://can-newsletter.org/hardware).


Accessing real CANopen devices is the same as described above for virtual CAN interface.
Some tested USB to CAN interfaces, which are natively integrated into Linux are:

 - Simple serial [USBtin](http://www.fischl.de/usbtin/) - Start with: `sudo slcand -f -o -c -s8 /dev/ttyACM0 can0; sudo ip link set up can0`
 - [EMS CPC-USB](http://www.ems-wuensche.com/product/datasheet/html/can-usb-adapter-converter-interface-cpcusb.html) - Start with: `sudo ip link set up can0 type can bitrate 250000`
 - [PCAN-USB FD](http://www.peak-system.com/PCAN-USB-FD.365.0.html?&L=1) - Needs newer Linux kernel, supports CAN flexible data rate.
 - You can get the idea of other supported CAN interfaces in [Linux kernel source](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/drivers/net/can).
 - Beaglebone or Paspberry PI or similar has CAN capes available. On RPI worked
   also the above USB interfaces, but it was necessary to compile the kernel.


With [CANopenNode](https://github.com/CANopenNode/CANopenNode) you can also design your
own device. There are many very useful and high quality specifications for different
[device profiles](http://www.can-cia.org/standardization/specifications/),
some of them are public and free to download.


Here we played with virtual CAN interface and result shows as pixels on
screen. If you connect real CAN interface to your computer, things may
become dangerous. Keep control and safety on your machines!
