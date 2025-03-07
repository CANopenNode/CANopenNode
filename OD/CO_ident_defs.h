/*
 * Device identification definitions for CANopenNode.
 */

/* Initial (newly programmed device) CAN bit rate and CANopen Node Id */
#define CO_BITRATE_INITIAL 500
#define CO_NODE_ID_INITIAL 0xff

/* Manufacturer device name, OD entry 0x1008 */
#define CO_DEVICE_NAME "CANopenDemoRP2040"
/* Manufacturer hardware version, OD entry 0x1009 */
#define CO_HW_VERSION "---"
/* Manufacturer software version, OD entry 0x100A, updated on make from git
 * version. It has form `"<tag>-<n>-g<commit>[-dirty]"`, where `<tag>` is
 * name of the last tag, `<n>` is number of commits above the tag, `g` is for
 * git, `<commit>` is commit ID and `-dirty` shows, if git is not clean. */
#define CO_SW_VERSION "---"

/* Identity, OD entry 0x1018 */
#define CO_IDENTITY_VENDOR_ID 0x00000000
#define CO_IDENTITY_PRODUCT_CODE 0x00000001
#define CO_IDENTITY_REVISION_NUMBER 0x00000000
#define CO_IDENTITY_SERIAL_NUMBER 0x00000003
