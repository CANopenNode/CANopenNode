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
CANopenNode should run on any Linux machine. Examples below was tested on Debian based machines, including Ubuntu and Raspberry PI. It is possible to run tests described below without real CAN interface, because Linux kernel already contains virtual CAN interface.
All necessary Linux specific files are included in socketCAN directory of CANopenNode and Makefile is included in base directory.

Windows or Mac users, who don't have Linux installed, can use [VirtualBox](https://www.virtualbox.org/) and install [Ubuntu](https://ubuntu.com/download/desktop) or similar.


### Preparation
We will use Linux command line interface (Terminal) for all examples below. Open the terminal and cd to your working directory.
First install supporting packages: [can-utils](https://github.com/linux-can/can-utils), which is very useful tool for working with CAN interface and [git](https://git-scm.com/), which is recommended for working with repositories.
Then clone [CANopenNode](https://github.com/CANopenNode/CANopenNode) from Github and build the executable program.

    sudo apt-get install git
    sudo apt-get install can-utils
    git clone https://github.com/CANopenNode/CANopenNode.git
    cd CANopenNode
    # For update just use 'git pull' here
    make

Now prepare CAN virtual device and run _candump_, which will show all CAN traffic. Use a second terminal:

    sudo modprobe vcan
    sudo ip link add dev vcan0 type vcan
    sudo ip link set up vcan0
    candump vcan0


### First CANopen device
Go to the first terminal, where we have recently build executable, named _canopend_.
First print help, then run the program with some options.

    ./canopend --help
    ./canopend vcan0 -i 4 -s od4_storage -a od4_storage_auto

You are now running a fully functional CANopen device on virtual CAN network. It is running in background until you terminate the process (with CTRL+C for example) or it receives a reset message from CAN network. By default process also shows some info messages on terminal, for example changes of NMT state or emergency messages, own and remote.

On the second terminal you can see some CAN traffic. After _canopend_ startup, first messages are:

    vcan0  704   [1]  00                        # Boot-up message.
    vcan0  084   [8]  00 50 01 2F F3 FF FF FF   # Emergency message.
    vcan0  704   [1]  7F                        # Heartbeat messages each second
    vcan0  704   [1]  7F

Boot-up and Heartbeat messages of node 4 have CAN-ID equal to 0x704. CAN-ID is 11-bit standard CAN identifier. 0x7F in heartbeat message means, that node is in NMT pre-operational state.

Also, both, first and second terminal shows, that there is an Emergency message after the boot-up. Also Heartbeat messages shows NMT pre-operational state.

The easiest way to find the reason of the emergency message is to check the byte 4 (errorBit). It has value of 0x2F. Go to CANopenNode source code and open the file "301/CO_Emergency.h", section "Error status bits". 0x2F means "CO_EM_NON_VOLATILE_MEMORY", which is generic, critical error with access to non volatile device memory.

This byte is CANopenNode specific. You can observe also first two bytes, which shows standard error code (0x5000 - Device Hardware) or third byte, which shows error register. If error register is different than zero, then node may be prohibited to enter operational and PDOs can not be exchanged with it.

You can follow the reason of the problem inside the source code. However, there are missing non-default storage files. Go to the first terminal, terminate the application with CTRL+C, add files and run _canopend_ again.

    echo "-" > od4_storage
    echo "-" > od4_storage_auto
    ./canopend vcan0 -i 4 -s od4_storage -a od4_storage_auto

Second terminal now shows operational state (0x05) and one pre-defined PDO message with CAN-ID 0x184 and two bytes of data. To learn more about PDOs, how to configure communication and mapping parameters and how to use them see other sources of CANopen documentation. For example, you can read the article of PDO re-mapping procedure in [CAN newsletter magazine, June 2016](http://can-newsletter.org/engineering/engineering-miscellaneous/160601_can-newsletter-magazine-june-2016).

    vcan0  704   [1]  00
    vcan0  184   [2]  00 00     # PDO message
    vcan0  704   [1]  05


### Second CANopen device
Open the third terminal and cd to the same directory as is in the first terminal. First generate default storage files. Then start second instance of _canopend_ with NodeID = 1. Use default od_storage files and enable command interface on standard IO (terminal).

    echo "-" > od_storage
    echo "-" > od_storage_auto
    ./canopend vcan0 -i1 -c "stdio"

Now you should see in second terminal (_candump_) two CANopen devices sending heartbeats in one second interval each. One with node-ID = 4 and one with node-ID = 1. Both should be operational.


### CANopen command interface
Second instance of _canopend_ was started with command interface enabled. This is CANopen gateway interface with ascii mapping, as specified in standard CiA309-3. This enables usage of CANopen master functionalities via basic terminal. Go to third terminal, type "help" and press enter to see its functionalities.

    help
    help datatype

#### SDO client
For example read Heartbeat producer parameter on CANopen device with ID=4. Parameter is located at index 0x1017, subindex 0, it is 16-bit unsigned integer.

    [1] 4 read 0x1017 0 u16

You should see the response, which says that Heartbeats are transmitted in 1000 ms intervals:

    [1] 1000

In CAN dump you can see some SDO communication. SDO communication can be quite complicated, if observing on _candump_, especially if larger data is split between multiple segments. However, there is no need to know the details, everything should work correctly in the background. Details about SDO communication can be found in CiA301 standard and partly also in 301/CO_SDOserver.h file, description of CO_SDO_state_t enumerator.

    [2] 4 write 0x1017 0 u16 5000
    [2] OK      #response

In _candump_ you will notice, that heartbeats from node 4 are coming in 5 second interval now. You can do the same also for node 1, but you won't see anything on _candump_, because data are written into itself directly. In "stdio" you can omit sequence number, to make typing easier.

    1 w 0x1017 0 u16 2500
    [0] OK

Now store Object dictionary on node-ID 4, so it will preserve variables on next start of the program.

    4 w 0x1010 1 u32 0x65766173
    [0] OK

More details about Object dictionary variables can be found in CiA301 standard or in example/IO.html file.


#### NMT master
If node is operational (started), it can exchange all objects, including PDO, SDO, etc. In NMT pre-operational, PDOs are disabled, SDOs works. In stopped only NMT messages are accepted. Try following commands and observe _candump_.

    set node 4
    [0] OK

    preop
    [0] OK

    start
    [0] OK

    stop
    [0] OK

    r 0x1017 0 i16
    [0] ERROR: 0x05040000 #SDO protocol timed out.

    reset communication
    [0] OK

    reset node
    [0] OK

    1 reset communication
    [0] OK

    1 reset node
    [0] OK


#### Other communication channels
We used simple stdio for command interface. In Linux also sockets can be used, either local or tcp. See `./canopend --help` for options. Simple Linux tool for establishing socket connection is _netcat_ or _nc_. For example `nc -U /tmp/CO_command_socket` for local socket or `nc <host> 60000` for tcp socket. There are also some tools from [CANopenSocket](https://github.com/CANopenNode/CANopenSocket) project.

Please be careful when exposing your CANopen network to the outside world, it is your responsibility, if something dangerous happen.


### Next steps
Assigning Node-ID or CAN bitrate, which support LSS configuration, is described in *LSSusage.md*.

This version of CANopenNode (<= v2) contains old and outdated approach to Object Dictionary. You may try to switch to newer version, for which are available some further examples and tools.

Further CANopenNode related tools and examples are available in [CANopenSocket](https://github.com/CANopenNode/CANopenSocket). Especially interesting is [basicDevice](https://github.com/CANopenNode/CANopenSocket/examples/basicDevice)

Custom CANopen device can be created based on own Object Dictionary, see README.md. There are also many very useful and high quality specifications for different [device profiles](http://www.can-cia.org/standardization/specifications/), some of them are public and free to download, for example CiA401.

For own CANopen device with own microcontroller, see *deviceSupport.md*. There is a bare-metal demo for [PIC microcontrollers](https://github.com/CANopenNode/CANopenPIC), most complete example is for PIC32.

Another interesting tool is [CANopen for Python](https://github.com/christiansandberg/canopen).

Examples here worked in virtual CAN interface, for simplicity. Virtual CAN runs inside Linux kernel only, it does not have much practical usability. If one has real CAN network configuration, then above examples are suitable also for this network, if Linux machine is connected to it and CAN interface is properly configured. When connecting your devices to real CAN network, make sure, you have at least two devices communicating, connected with ground and pair of wires, terminated with two 120ohm resistors, correct baudrate, etc.

Accessing real CANopen devices is the same as described above for virtual CAN interface. Some tested USB to CAN interfaces, which are native in Linux kernel are:
 - Simple serial [USBtin](http://www.fischl.de/usbtin/) - Start with: `sudo slcand -f -o -c -s8 /dev/ttyACM0 can0; sudo ip link set up can0`
 - [EMS CPC-USB](https://www.ems-wuensche.com/?post_type=product&p=746) or [PCAN-USB FD](http://www.peak-system.com/PCAN-USB-FD.365.0.html?&L=1) - Start with: `sudo ip link set up can0 type can bitrate 250000`
 - You can get the idea of other supported CAN interfaces in [Linux kernel source](https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/drivers/net/can) (Kconfig files).
 - Raspberry PI or similar has CAN capes available.

Examples here run in Linux, for simplicity. However, real usability of CANopen network is, when simple, microcontroller based devices are connected together with or without more advanced commander device. CANopenNode is basically written for simple microcontrollers and also has more advanced commander features, like above used CANopen gateway with ascii command interface.

Now you can enter the big world of [CANopen devices](http://can-newsletter.org/hardware).

Here we played with virtual CAN interface and result shown as pixels on screen. If you connect a real CAN interface to your computer, things may become dangerous. Keep control and safety on your machines!
