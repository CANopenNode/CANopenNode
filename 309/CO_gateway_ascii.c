/*
 * CANopen access from other networks - ASCII mapping (CiA 309-3 DS v3.0.0)
 *
 * @file        CO_gateway_ascii.c
 * @ingroup     CO_CANopen_309_3
 * @author      Janez Paternoster
 * @author      Martin Wagner
 * @copyright   2020 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "309/CO_gateway_ascii.h"

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>

/* verify configuration */
#if !((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ENABLE)
 #error CO_CONFIG_FIFO_ENABLE must be enabled.
#endif
#if !((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_COMMANDS)
 #error CO_CONFIG_FIFO_ASCII_COMMANDS must be enabled.
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
 #if !((CO_CONFIG_FIFO) & CO_CONFIG_FIFO_ASCII_DATATYPES)
  #error CO_CONFIG_FIFO_ASCII_DATATYPES must be enabled.
 #endif
#endif

/******************************************************************************/
CO_ReturnError_t CO_GTWA_init(CO_GTWA_t* gtwa,
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO) || defined CO_DOXYGEN
                              CO_SDOclient_t* SDO_C,
                              uint16_t SDOclientTimeoutTime_ms,
                              bool_t SDOclientBlockTransfer,
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT) || defined CO_DOXYGEN
                              CO_NMT_t *NMT,
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS) || defined CO_DOXYGEN
                              CO_LSSmaster_t *LSSmaster,
#endif
#if ((CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS) || defined CO_DOXYGEN
                              CO_LEDs_t *LEDs,
#endif
                              uint8_t dummy)
{
    (void)dummy;
    /* verify arguments */
    if (gtwa == NULL
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
        || SDO_C == NULL || SDOclientTimeoutTime_ms == 0
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT
        || NMT == NULL
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
        || LSSmaster == NULL
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
        || LEDs == NULL
#endif
    ) {
        return CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* clear the object */
    memset(gtwa, 0, sizeof(CO_GTWA_t));

    /* initialize variables */
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
    gtwa->SDO_C = SDO_C;
    gtwa->SDOtimeoutTime = SDOclientTimeoutTime_ms;
    gtwa->SDOblockTransferEnable = SDOclientBlockTransfer;
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT
    gtwa->NMT = NMT;
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
    gtwa->LSSmaster = LSSmaster;
#endif
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
    gtwa->LEDs = LEDs;
#endif
    gtwa->net_default = -1;
    gtwa->node_default = -1;
    gtwa->state = CO_GTWA_ST_IDLE;
    gtwa->respHold = false;

    CO_fifo_init(&gtwa->commFifo,
                 &gtwa->commBuf[0],
                 CO_CONFIG_GTWA_COMM_BUF_SIZE + 1);

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG
    CO_fifo_init(&gtwa->logFifo,
                 &gtwa->logBuf[0],
                 CO_CONFIG_GTWA_LOG_BUF_SIZE + 1);
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG) */

    return CO_ERROR_NO;
}


/******************************************************************************/
void CO_GTWA_initRead(CO_GTWA_t* gtwa,
                      size_t (*readCallback)(void *object,
                                             const char *buf,
                                             size_t count,
                                             uint8_t *connectionOK),
                      void *readCallbackObject)
{
    if (gtwa != NULL) {
        gtwa->readCallback = readCallback;
        gtwa->readCallbackObject = readCallbackObject;
    }
}


/******************************************************************************/
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG
void CO_GTWA_log_print(CO_GTWA_t* gtwa, const char *message) {
    if (gtwa != NULL && message != NULL) {
        const char *c;

        for (c = &message[0]; *c != 0; c++) {
            CO_fifo_putc_ov(&gtwa->logFifo, (const uint8_t)*c);
        }
    }
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG */


/*******************************************************************************
 * HELPER FUNCTIONS
 ******************************************************************************/
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP
/* help strings ("\n" is used between string, "\r\n" closes the response.) */
static const char CO_GTWA_helpString[] =
"\nCommand strings start with '\"[\"<sequence>\"]\"' followed by:\n" \
"[[<net>] <node>] r[ead] <index> <subindex> [<datatype>]        # SDO upload.\n" \
"[[<net>] <node>] w[rite] <index> <subindex> <datatype> <value> # SDO download.\n" \
"\n" \
"[[<net>] <node>] start                   # NMT Start node.\n" \
"[[<net>] <node>] stop                    # NMT Stop node.\n" \
"[[<net>] <node>] preop[erational]        # NMT Set node to pre-operational.\n" \
"[[<net>] <node>] reset node              # NMT Reset node.\n" \
"[[<net>] <node>] reset comm[unication]   # NMT Reset communication.\n" \
"\n" \
"[<net>] set network <value>              # Set default net.\n" \
"[<net>] set node <value>                 # Set default node.\n" \
"[<net>] set sdo_timeout <value>          # Configure SDO client time-out in ms.\n" \
"[<net>] set sdo_block <0|1>              # Enable/disable SDO block transfer.\n" \
"\n" \
"help [datatype|lss]                      # Print this or datatype or lss help.\n" \
"led                                      # Print status LEDs of this device.\n" \
"log                                      # Print message log.\n" \
"\n" \
"Response:\n" \
"\"[\"<sequence>\"]\" OK | <value> |\n" \
"                 ERROR:<SDO-abort-code> | ERROR:<internal-error-code>\n" \
"\n" \
"* Every command must be terminated with <CR><LF> ('\\r\\n'). characters. Same\n" \
"  is response. String is not null terminated, <CR> is optional in command.\n" \
"* Comments started with '#' are ignored. They may be on the beginning of the\n" \
"  line or after the command string.\n" \
"* 'sdo_timeout' is in milliseconds, 500 by default. Block transfer is\n" \
"  disabled by default.\n" \
"* If '<net>' or '<node>' is not specified within commands, then value defined\n" \
"  by 'set network' or 'set node' command is used.\r\n";

static const char CO_GTWA_helpStringDatatypes[] =
"\nDatatypes:\n" \
"b                  # Boolean.\n" \
"i8, i16, i32, i64  # Signed integers.\n" \
"u8, u16, u32, u64  # Unsigned integers.\n" \
"x8, x16, x32, x64  # Unsigned integers, displayed as hexadecimal, non-standard.\n" \
"r32, r64           # Real numbers.\n" \
"vs                 # Visible string (between double quotes if multi-word).\n" \
"os, us             # Octet, unicode string, (mime-base64 (RFC2045) based, line).\n" \
"d                  # domain (mime-base64 (RFC2045) based, one line).\n" \
"hex                # Hexagonal data, optionally space separated, non-standard.\r\n";

static const char CO_GTWA_helpStringLss[] =
"\nLSS commands:\n" \
"lss_switch_glob <0|1>                  # Switch state global command.\n" \
"lss_switch_sel <vendorID> <product code> \\\n" \
"               <revisionNo> <serialNo> #Switch state selective.\n" \
"lss_set_node <node>                    # Configure node-ID.\n" \
"lss_conf_bitrate <table_selector=0> \\\n" \
"                 <table_index>         # Configure bit-rate.\n" \
"lss_activate_bitrate <switch_delay_ms> # Activate new bit-rate.\n" \
"lss_store                              # LSS store configuration.\n" \
"lss_inquire_addr [<LSSSUB=0..3>]       # Inquire LSS address.\n" \
"lss_get_node                           # Inquire node-ID.\n" \
"_lss_fastscan [<timeout_ms>]           # Identify fastscan, non-standard.\n" \
"lss_allnodes [<timeout_ms> [<nodeStart=1..127> <store=0|1>\\\n" \
"                [<scanType0> <vendorId> <scanType1> <productCode>\\\n" \
"                 <scanType2> <revisionNo> <scanType3> <serialNo>]]]\n" \
"                                       # Node-ID configuration of all nodes.\n" \
"\n" \
"* All LSS commands start with '\"[\"<sequence>\"]\" [<net>]'.\n" \
"* <table_index>: 0=1000 kbit/s, 1=800 kbit/s, 2=500 kbit/s, 3=250 kbit/s,\n" \
"                 4=125 kbit/s, 6=50 kbit/s, 7=20 kbit/s, 8=10 kbit/s, 9=auto\n" \
"* <scanType>: 0=fastscan, 1=ignore, 2=match value in next parameter\r\n";
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
#define CO_GTWA_LED_PRINTOUTS_SIZE 5
static const char *CO_GTWA_LED_PRINTOUTS[CO_GTWA_LED_PRINTOUTS_SIZE] = {
    " CANopen status LEDs: R  G         \r",
    " CANopen status LEDs: R  G*        \r",
    " CANopen status LEDs: R* G         \r",
    " CANopen status LEDs: R* G*        \r",
    "                                   \r"
};
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS */


/* Get uint32 number from token, verify limits and set *err if necessary */
static inline uint32_t getU32(char *token, uint32_t min,
                              uint32_t max, bool_t *err)
{
    char *sRet;
    uint32_t num = strtoul(token, &sRet, 0);

    if (sRet != strchr(token, '\0') || num < min || num > max) {
        *err = true;
    }

    return num;
}

/* Verify net and node, return true on error */
static bool_t checkNetNode(CO_GTWA_t *gtwa,
                           int32_t net, int16_t node, uint8_t NodeMin,
                           CO_GTWA_respErrorCode_t *errCode)
{
    bool_t e = false;
    CO_GTWA_respErrorCode_t eCode;

    if (node == -1) {
        eCode = CO_GTWA_respErrorNoDefaultNodeSet;
        e = true;
    }
    else if (node < NodeMin || node > 127) {
        eCode = CO_GTWA_respErrorUnsupportedNode;
        e = true;
    }
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_MULTI_NET
    else if (net == -1) {
        eCode = CO_GTWA_respErrorNoDefaultNetSet;
        e = true;
    }
    /* not implemented */
    else if (net < CO_CONFIG_GTW_NET_MIN || net > CO_CONFIG_GTW_NET_MAX) {
        eCode = CO_GTWA_respErrorUnsupportedNet;
        e = true;
    }
#endif
    else {
        gtwa->net = (uint16_t)net;
        gtwa->node = (uint8_t)node;
    }
    if (e) {
        *errCode = eCode;
    }
    return e;
}

/* Verify net, return true on error */
static bool_t checkNet(CO_GTWA_t *gtwa, int32_t net,
                       CO_GTWA_respErrorCode_t *errCode)
{
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_MULTI_NET
    bool_t e = false;
    CO_GTWA_respErrorCode_t eCode;

    if (net == -1) {
        eCode = CO_GTWA_respErrorNoDefaultNetSet;
        e = true;
    }
    /* not implemented */
    else if (net < CO_CONFIG_GTW_NET_MIN || net > CO_CONFIG_GTW_NET_MAX) {
        eCode = CO_GTWA_respErrorUnsupportedNet;
        e = true;
    }
    else {
        gtwa->net = (uint16_t)net;
    }
    if (e) {
        *errCode = eCode;
    }
    return e;
#else
    (void)errCode;    /* unused */
    #define CO_CONFIG_GTW_NET_MIN 0
    #define CO_CONFIG_GTW_NET_MAX 0xFFFF
    gtwa->net = (uint16_t)net;
    return false;
#endif
}


#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
/* data types for SDO read or write */
static const CO_GTWA_dataType_t dataTypes[] = {
    {"hex", 0, CO_fifo_readHex2a, CO_fifo_cpyTok2Hex},  /* hex, non-standard */
    {"b",   1, CO_fifo_readU82a,  CO_fifo_cpyTok2U8},   /* BOOLEAN */
    {"i8",  1, CO_fifo_readI82a,  CO_fifo_cpyTok2I8},   /* INTEGER8 */
    {"i16", 2, CO_fifo_readI162a, CO_fifo_cpyTok2I16},  /* INTEGER16 */
    {"i32", 4, CO_fifo_readI322a, CO_fifo_cpyTok2I32},  /* INTEGER32 */
    {"i64", 8, CO_fifo_readI642a, CO_fifo_cpyTok2I64},  /* INTEGER64 */
    {"u8",  1, CO_fifo_readU82a,  CO_fifo_cpyTok2U8},   /* UNSIGNED8 */
    {"u16", 2, CO_fifo_readU162a, CO_fifo_cpyTok2U16},  /* UNSIGNED16 */
    {"u32", 4, CO_fifo_readU322a, CO_fifo_cpyTok2U32},  /* UNSIGNED32 */
    {"u64", 8, CO_fifo_readU642a, CO_fifo_cpyTok2U64},  /* UNSIGNED64 */
    {"x8",  1, CO_fifo_readX82a,  CO_fifo_cpyTok2U8},   /* UNSIGNED8 */
    {"x16", 2, CO_fifo_readX162a, CO_fifo_cpyTok2U16},  /* UNSIGNED16 */
    {"x32", 4, CO_fifo_readX322a, CO_fifo_cpyTok2U32},  /* UNSIGNED32 */
    {"x64", 8, CO_fifo_readX642a, CO_fifo_cpyTok2U64},  /* UNSIGNED64 */
    {"r32", 4, CO_fifo_readR322a, CO_fifo_cpyTok2R32},  /* REAL32 */
    {"r64", 8, CO_fifo_readR642a, CO_fifo_cpyTok2R64},  /* REAL64 */
    {"vs",  0, CO_fifo_readVs2a,  CO_fifo_cpyTok2Vs},   /* VISIBLE_STRING */
    {"os",  0, CO_fifo_readB642a, CO_fifo_cpyTok2B64},  /* OCTET_STRING base64*/
    {"us",  0, CO_fifo_readB642a, CO_fifo_cpyTok2B64},/* UNICODE_STRING base64*/
    {"d",   0, CO_fifo_readB642a, CO_fifo_cpyTok2B64}   /* DOMAIN - base64 */
};


/* get data type from token */
static const CO_GTWA_dataType_t *CO_GTWA_getDataType(char *token, bool_t *err) {
    if (token != NULL && *err == false) {
        int i;
        int len = sizeof(dataTypes) / sizeof(CO_GTWA_dataType_t);

        for (i = 0; i < len; i++) {
            const CO_GTWA_dataType_t *dt = &dataTypes[i];
            if (strcmp(token, dt->syntax) == 0) {
                return dt;
            }
        }
    }

    *err = true;
    return NULL;
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */


/* transfer response buffer and verify if all bytes was read. Return true on
 * success, or false, if communication is broken. */
static bool_t respBufTransfer(CO_GTWA_t *gtwa) {
    uint8_t connectionOK = 1;

    if (gtwa->readCallback == NULL) {
        /* no callback registered, just purge the response */
        gtwa->respBufOffset = 0;
        gtwa->respBufCount = 0;
        gtwa->respHold = false;
    }
    else {
        /* transfer response to the application */
        size_t countRead =
        gtwa->readCallback(gtwa->readCallbackObject,
                           (const char *)&gtwa->respBuf[gtwa->respBufOffset],
                           gtwa->respBufCount,
                           &connectionOK);

        if (countRead < gtwa->respBufCount) {
            gtwa->respBufOffset += countRead;
            gtwa->respBufCount -= countRead;
            gtwa->respHold = true;
        }
        else {
            gtwa->respBufOffset = 0;
            gtwa->respBufCount = 0;
            gtwa->respHold = false;
        }
    }
    return connectionOK != 0;
}


#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_ERROR_DESC
#ifndef CO_CONFIG_GTW_ASCII_ERROR_DESC_STRINGS
#define CO_CONFIG_GTW_ASCII_ERROR_DESC_STRINGS
typedef struct {
    const uint32_t code;
    const char* desc;
} errorDescs_t;

static const errorDescs_t errorDescs[] = {
    {100, "Request not supported."},
    {101, "Syntax error."},
    {102, "Request not processed due to internal state."},
    {103, "Time-out."},
    {104, "No default net set."},
    {105, "No default node set."},
    {106, "Unsupported net."},
    {107, "Unsupported node."},
    {200, "Lost guarding message."},
    {201, "Lost connection."},
    {202, "Heartbeat started."},
    {203, "Heartbeat lost."},
    {204, "Wrong NMT state."},
    {205, "Boot-up."},
    {300, "Error passive."},
    {301, "Bus off."},
    {303, "CAN buffer overflow."},
    {304, "CAN init."},
    {305, "CAN active (at init or start-up)."},
    {400, "PDO already used."},
    {401, "PDO length exceeded."},
    {501, "LSS implementation- / manufacturer-specific error."},
    {502, "LSS node-ID not supported."},
    {503, "LSS bit-rate not supported."},
    {504, "LSS parameter storing failed."},
    {505, "LSS command failed because of media error."},
    {600, "Running out of memory."}
};
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
static const errorDescs_t errorDescsSDO[] = {
    {0x00000000, "No abort."},
    {0x05030000, "Toggle bit not altered."},
    {0x05040000, "SDO protocol timed out."},
    {0x05040001, "Command specifier not valid or unknown."},
    {0x05040002, "Invalid block size in block mode."},
    {0x05040003, "Invalid sequence number in block mode."},
    {0x05040004, "CRC error (block mode only)."},
    {0x05040005, "Out of memory."},
    {0x06010000, "Unsupported access to an object."},
    {0x06010001, "Attempt to read a write only object."},
    {0x06010002, "Attempt to write a read only object."},
    {0x06020000, "Object does not exist."},
    {0x06040041, "Object cannot be mapped to the PDO."},
    {0x06040042, "Number and length of object to be mapped exceeds PDO length."},
    {0x06040043, "General parameter incompatibility reasons."},
    {0x06040047, "General internal incompatibility in device."},
    {0x06060000, "Access failed due to hardware error."},
    {0x06070010, "Data type does not match, length of service parameter does not match."},
    {0x06070012, "Data type does not match, length of service parameter too high."},
    {0x06070013, "Data type does not match, length of service parameter too short."},
    {0x06090011, "Sub index does not exist."},
    {0x06090030, "Invalid value for parameter (download only)."},
    {0x06090031, "Value range of parameter written too high."},
    {0x06090032, "Value range of parameter written too low."},
    {0x06090036, "Maximum value is less than minimum value."},
    {0x060A0023, "Resource not available: SDO connection."},
    {0x08000000, "General error."},
    {0x08000020, "Data cannot be transferred or stored to application."},
    {0x08000021, "Data cannot be transferred or stored to application because of local control."},
    {0x08000022, "Data cannot be transferred or stored to application because of present device state."},
    {0x08000023, "Object dictionary not present or dynamic generation fails."},
    {0x08000024, "No data available."}
};
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */
#endif /* CO_CONFIG_GTW_ASCII_ERROR_DESC_STRINGS */

static void responseWithError(CO_GTWA_t *gtwa,
                              CO_GTWA_respErrorCode_t respErrorCode)
{
    int i;
    int len = sizeof(errorDescs) / sizeof(errorDescs_t);
    const char *desc = "-";

    for (i = 0; i < len; i++) {
        const errorDescs_t *ed = &errorDescs[i];
        if((CO_GTWA_respErrorCode_t)ed->code == respErrorCode) {
            desc = ed->desc;
        }
    }

    gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                  "[%"PRId32"] ERROR:%d #%s\r\n",
                                  gtwa->sequence, respErrorCode, desc);
    respBufTransfer(gtwa);
}

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
static void responseWithErrorSDO(CO_GTWA_t *gtwa,
                                 CO_SDO_abortCode_t abortCode,
                                 bool_t postponed)
{
    int i;
    int len = sizeof(errorDescsSDO) / sizeof(errorDescs_t);
    const char *desc = "-";

    for (i = 0; i < len; i++) {
        const errorDescs_t *ed = &errorDescsSDO[i];
        if((CO_SDO_abortCode_t)ed->code == abortCode) {
            desc = ed->desc;
        }
    }

    if (!postponed) {
        gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                      "[%"PRId32"] ERROR:0x%08X #%s\r\n",
                                      gtwa->sequence, abortCode, desc);
    }
    else {
        gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                      "\n...ERROR:0x%08X #%s\r\n",
                                      abortCode, desc);
    }

    respBufTransfer(gtwa);
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */

#else /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_ERROR_DESC */
static inline void responseWithError(CO_GTWA_t *gtwa,
                                     CO_GTWA_respErrorCode_t respErrorCode)
{
    gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                  "[%"PRId32"] ERROR:%d\r\n",
                                  gtwa->sequence, respErrorCode);
    respBufTransfer(gtwa);
}

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
static inline void responseWithErrorSDO(CO_GTWA_t *gtwa,
                                        CO_SDO_abortCode_t abortCode,
                                        bool_t postponed)
{
    if (!postponed) {
        gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                      "[%"PRId32"] ERROR:0x%08X\r\n",
                                      gtwa->sequence, abortCode);
    }
    else {
        gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                      "\n...ERROR:0x%08X\r\n",
                                      abortCode);
    }

    respBufTransfer(gtwa);
}
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_ERROR_DESC */


static inline void responseWithOK(CO_GTWA_t *gtwa) {
    gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                  "[%"PRId32"] OK\r\n",
                                  gtwa->sequence);
    respBufTransfer(gtwa);
}


static inline void responseWithEmpty(CO_GTWA_t *gtwa) {
    gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                  "\r\n");
    respBufTransfer(gtwa);
}


#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
static void responseLSS(CO_GTWA_t *gtwa, CO_LSSmaster_return_t lss_ret) {
    if (lss_ret == CO_LSSmaster_OK) {
        responseWithOK(gtwa);
    }
    else {
        CO_GTWA_respErrorCode_t respErrorCode;

        if (lss_ret==CO_LSSmaster_TIMEOUT || lss_ret==CO_LSSmaster_SCAN_NOACK) {
            respErrorCode = CO_GTWA_respErrorTimeOut;
        }
        else if (lss_ret == CO_LSSmaster_OK_MANUFACTURER) {
            respErrorCode = CO_GTWA_respErrorLSSmanufacturer;
        }
        else {
            respErrorCode = CO_GTWA_respErrorInternalState;
        }
        responseWithError(gtwa, respErrorCode);
    }
}
#endif


static inline void convertToLower(char *token, size_t maxCount) {
    size_t i;
    char *c = &token[0];

    for (i = 0; i < maxCount; i++) {
        if (*c == 0) {
            break;
        } else {
            *c = (char)tolower((int)*c);
        }
        c++;
    }
}


/*******************************************************************************
 * PROCESS FUNCTION
 ******************************************************************************/
void CO_GTWA_process(CO_GTWA_t *gtwa,
                     bool_t enable,
                     uint32_t timeDifference_us,
                     uint32_t *timerNext_us)
{
    (void)timerNext_us; /* may be unused */

    bool_t err = false; /* syntax or other error, true or false, I/O variable */
    int8_t closed; /* indication of command delimiter, I/O variable */
    CO_GTWA_respErrorCode_t respErrorCode = CO_GTWA_respErrorNone;

    if (gtwa == NULL) {
        return;
    }

    if (!enable) {
        gtwa->state = CO_GTWA_ST_IDLE;
        CO_fifo_reset(&gtwa->commFifo);
        return;
    }

    /* If there is some more output data for application, read them first.
     * Hold on this state, if necessary. */
    if (gtwa->respHold) {
        timeDifference_us += gtwa->timeDifference_us_cumulative;

        respBufTransfer(gtwa);
        if (gtwa->respHold) {
            gtwa->timeDifference_us_cumulative = timeDifference_us;
            return;
        }
        else {
            gtwa->timeDifference_us_cumulative = 0;
        }
    }

    /***************************************************************************
    * COMMAND PARSER
    ***************************************************************************/
    /* if idle, search for new command, skip comments or empty lines */
    while (gtwa->state == CO_GTWA_ST_IDLE
           && CO_fifo_CommSearch(&gtwa->commFifo, false)
    ) {
        char tok[20];
        size_t n;
        uint32_t ui[3];
        int i;
        int32_t net = gtwa->net_default;
        int16_t node = gtwa->node_default;


        /* parse mandatory token '"["<sequence>"]"' */
        closed = -1;
        n = CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
        /* Break if error in token or token was found, but closed with
         * command delimiter. */
        if (err || (n > 0 && closed != 0)) {
            err = true;
            break;
        }
        /* If empty line or just comment, continue with next command */
        else if (n == 0 && closed != 0) {
            responseWithEmpty(gtwa);
            continue;
        }
        if (tok[0] != '[' || tok[strlen(tok)-1] != ']') {
            err = true;
            break;
        }
        tok[strlen(tok)-1] = '\0';
        gtwa->sequence = getU32(tok + 1, 0, 0xFFFFFFFF, &err);
        if (err) break;


        /* parse optional tokens '[[<net>] <node>]', both numerical. Then
         * follows mandatory token <command>, which is not numerical. */
        for (i = 0; i < 3; i++) {
            closed = -1;
            n = CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
            if (err || n == 0) {
                /* empty token, break on error */
                err = true;
                break;
            } else if (isdigit((int)tok[0]) == 0) {
                /* <command> found */
                break;
            } else if (closed != 0) {
                /* numerical value must not be closed */
                err = true;
                break;
            }

            ui[i] = getU32(tok, 0, 0xFFFFFFFF, &err);
            if (err) break;
        }
        if (err) break;

        switch(i) {
        case 0: /* only <command> (pointed by token) */
            break;
        case 1: /* <node> and <command> tokens */
            if (ui[0] > 127) {
                err = true;
                respErrorCode = CO_GTWA_respErrorUnsupportedNode;
            }
            else {
                node = (int16_t) ui[0];
            }
            break;
        case 2: /* <net>, <node> and <command> tokens */
            if (ui[0] > 0xFFFF) {
                err = true;
                respErrorCode = CO_GTWA_respErrorUnsupportedNet;
            }
            else if (ui[1] > 127) {
                err = true;
                respErrorCode = CO_GTWA_respErrorUnsupportedNode;
            }
            else {
                net = (int32_t) ui[0];
                node = (int16_t) ui[1];
            }
            break;
        case 3: /* <command> token contains digit */
            err = true;
            break;
        }
        if (err) break;

        /* command is case insensitive */
        convertToLower(tok, sizeof(tok));

        /* set command - multiple sub commands */
        if (strcmp(tok, "set") == 0) {
            if (closed != 0) {
                err = true;
                break;
            }

            /* command 2 */
            closed = -1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            if (err) break;

            convertToLower(tok, sizeof(tok));
            /* 'set network <value>' */
            if (strcmp(tok, "network") == 0) {
                uint16_t value;

                if (closed != 0) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
                value = (uint16_t)getU32(tok, CO_CONFIG_GTW_NET_MIN,
                                         CO_CONFIG_GTW_NET_MAX, &err);
                if (err) break;

                gtwa->net_default = value;
                responseWithOK(gtwa);
            }
            /* 'set node <value>' */
            else if (strcmp(tok, "node") == 0) {
                bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
                uint8_t value;

                if (closed != 0 || NodeErr) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
                value = (uint8_t)getU32(tok, 1, 127, &err);
                if (err) break;

                gtwa->node_default = value;
                responseWithOK(gtwa);
            }
#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
            /* 'set sdo_timeout <value_ms>' */
            else if (strcmp(tok, "sdo_timeout") == 0) {
                bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
                uint16_t value;

                if (closed != 0 || NodeErr) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
                value = (uint16_t)getU32(tok, 1, 0xFFFF, &err);
                if (err) break;

                gtwa->SDOtimeoutTime = value;
                responseWithOK(gtwa);
            }
            /* 'set sdo_timeout <0|1>' */
            else if (strcmp(tok, "sdo_block") == 0) {
                bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
                uint16_t value;

                if (closed != 0 || NodeErr) {
                    err = true;
                    break;
                }

                /* value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
                value = (uint16_t)getU32(tok, 0, 1, &err);
                if (err) break;

                gtwa->SDOblockTransferEnable = value==1 ? true : false;
                responseWithOK(gtwa);
            }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */
            else {
                respErrorCode = CO_GTWA_respErrorReqNotSupported;
                err = true;
                break;
            }
        }

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
        /* Upload SDO command - 'r[ead] <index> <subindex> <datatype>' */
        else if (strcmp(tok, "r") == 0 || strcmp(tok, "read") == 0) {
            uint16_t idx;
            uint8_t subidx;
            CO_SDO_return_t SDO_ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 1, &respErrorCode);

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* index */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            idx = (uint16_t)getU32(tok, 0, 0xFFFF, &err);
            if (err) break;

            /* subindex */
            closed = -1;
            n = CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
            subidx = (uint8_t)getU32(tok, 0, 0xFF, &err);
            if (err || n == 0) {
                err = true;
                break;
            }

            /* optional data type */
            if (closed == 0) {
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
                convertToLower(tok, sizeof(tok));
                gtwa->SDOdataType = CO_GTWA_getDataType(tok, &err);
                if (err) break;
            }
            else {
                gtwa->SDOdataType = &dataTypes[0]; /* use generic data type */
            }

            /* setup client */
            SDO_ret = CO_SDOclient_setup(gtwa->SDO_C,
                                         CO_CAN_ID_SDO_CLI + gtwa->node,
                                         CO_CAN_ID_SDO_SRV + gtwa->node,
                                         gtwa->node);
            if (SDO_ret != CO_SDO_RT_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* initiate upload */
            SDO_ret = CO_SDOclientUploadInitiate(gtwa->SDO_C, idx, subidx,
                                                 gtwa->SDOtimeoutTime,
                                                 gtwa->SDOblockTransferEnable);
            if (SDO_ret != CO_SDO_RT_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* indicate that gateway response didn't start yet */
            gtwa->SDOdataCopyStatus = false;
            /* continue with state machine */
            timeDifference_us = 0;
            gtwa->state = CO_GTWA_ST_READ;
        }

        /* Download SDO comm. - w[rite] <index> <subindex> <datatype> <value> */
        else if (strcmp(tok, "w") == 0 || strcmp(tok, "write") == 0) {
            uint16_t idx;
            uint8_t subidx;
            CO_fifo_st status;
            CO_SDO_return_t SDO_ret;
            size_t size;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 1, &respErrorCode);

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* index */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            idx = (uint16_t)getU32(tok, 0, 0xFFFF, &err);
            if (err) break;

            /* subindex */
            closed = 0;
            n = CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                                  &closed, &err);
            subidx = (uint8_t)getU32(tok, 0, 0xFF, &err);
            if (err) break;

            /* data type */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok),
                              &closed, &err);
            convertToLower(tok, sizeof(tok));
            gtwa->SDOdataType = CO_GTWA_getDataType(tok, &err);
            if (err) break;

            /* setup client */
            SDO_ret = CO_SDOclient_setup(gtwa->SDO_C,
                                         CO_CAN_ID_SDO_CLI + gtwa->node,
                                         CO_CAN_ID_SDO_SRV + gtwa->node,
                                         gtwa->node);
            if (SDO_ret != CO_SDO_RT_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* initiate download */
            SDO_ret =
            CO_SDOclientDownloadInitiate(gtwa->SDO_C, idx, subidx,
                                         gtwa->SDOdataType->length,
                                         gtwa->SDOtimeoutTime,
                                         gtwa->SDOblockTransferEnable);
            if (SDO_ret != CO_SDO_RT_ok_communicationEnd) {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }

            /* copy data from comm to the SDO buffer, according to data type */
            size = gtwa->SDOdataType->dataTypeScan(&gtwa->SDO_C->bufFifo,
                                                   &gtwa->commFifo,
                                                   &status);
            /* set to true, if command delimiter was found */
            closed = ((status & CO_fifo_st_closed) == 0) ? 0 : 1;
            /* set to true, if data are copied only partially */
            gtwa->SDOdataCopyStatus = (status & CO_fifo_st_partial) != 0;

            /* is syntax error in command or size is zero or not the last token
             * in command */
            if ((status & CO_fifo_st_errMask) != 0 || size == 0
                || (gtwa->SDOdataCopyStatus == false && closed != 1)
            ) {
                err = true;
                break;
            }

            /* if data size was not known before and is known now, update SDO */
            if (gtwa->SDOdataType->length == 0 && !gtwa->SDOdataCopyStatus) {
                CO_SDOclientDownloadInitiateSize(gtwa->SDO_C, size);
            }

            /* continue with state machine */
            gtwa->stateTimeoutTmr = 0;
            timeDifference_us = 0;
            gtwa->state = CO_GTWA_ST_WRITE;
        }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT
        /* NMT start node - 'start' */
        else if (strcmp(tok, "start") == 0) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2 = CO_NMT_ENTER_OPERATIONAL;

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }
            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* NMT stop node - 'stop' */
        else if (strcmp(tok, "stop") == 0) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2 = CO_NMT_ENTER_STOPPED;

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }
            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* NMT Set node to pre-operational - 'preop[erational]' */
        else if (strcmp(tok, "preop") == 0 ||
                 strcmp(tok, "preoperational") == 0
        ) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2 = CO_NMT_ENTER_PRE_OPERATIONAL;

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }
            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }

        /* NMT reset (node or communication) - 'reset <node|comm[unication]>'*/
        else if (strcmp(tok, "reset") == 0) {
            CO_ReturnError_t ret;
            bool_t NodeErr = checkNetNode(gtwa, net, node, 0, &respErrorCode);
            CO_NMT_command_t command2;

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* command 2 */
            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            if (err) break;

            convertToLower(tok, sizeof(tok));
            if (strcmp(tok, "node") == 0) {
                command2 = CO_NMT_RESET_NODE;
            } else if (strcmp(tok, "comm") == 0 ||
                       strcmp(tok, "communication") == 0
            ) {
                command2 = CO_NMT_RESET_COMMUNICATION;
            } else {
                err = true;
                break;
            }

            ret = CO_NMT_sendCommand(gtwa->NMT, command2, gtwa->node);

            if (ret == CO_ERROR_NO) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_NMT */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
        /* Switch state global command - 'lss_switch_glob <0|1>' */
        else if (strcmp(tok, "lss_switch_glob") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
            uint8_t select;

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* get value */
            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            select = (uint8_t)getU32(tok, 0, 1, &err);
            if (err) break;

            if (select == 0) {
                /* send non-confirmed message */
                CO_LSSmaster_return_t ret;
                ret = CO_LSSmaster_switchStateDeselect(gtwa->LSSmaster);
                if (ret == CO_LSSmaster_OK) {
                    responseWithOK(gtwa);
                }
                else {
                    respErrorCode = CO_GTWA_respErrorInternalState;
                    err = true;
                    break;
                }
            }
            else {
                /* continue with state machine */
                gtwa->state = CO_GTWA_ST_LSS_SWITCH_GLOB;
            }
        }
        /* Switch state selective command -
         * 'lss_switch_sel <vendorID> <product code> <revisionNo> <serialNo>' */
        else if (strcmp(tok, "lss_switch_sel") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
            CO_LSS_address_t *addr = &gtwa->lssAddress;

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* get values */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            addr->identity.vendorID = getU32(tok, 0, 0xFFFFFFFF, &err);
            if (err) break;

            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            addr->identity.productCode = getU32(tok, 0, 0xFFFFFFFF, &err);
            if (err) break;

            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            addr->identity.revisionNumber = getU32(tok, 0, 0xFFFFFFFF, &err);
            if (err) break;

            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            addr->identity.serialNumber = getU32(tok, 0, 0xFFFFFFFF, &err);
            if (err) break;

            /* continue with state machine */
            gtwa->state = CO_GTWA_ST_LSS_SWITCH_SEL;
        }
        /* LSS configure node-ID command - 'lss_set_node <node>' */
        else if (strcmp(tok, "lss_set_node") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* get value */
            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            gtwa->lssNID = (uint8_t)getU32(tok, 0, 0xFF, &err);
            if (gtwa->lssNID > 0x7F && gtwa->lssNID < 0xFF) err = true;
            if (err) break;

            /* continue with state machine */
            gtwa->state = CO_GTWA_ST_LSS_SET_NODE;
        }
        /* LSS configure bit-rate command -
         * 'lss_conf_bitrate <table_selector=0> <table_index>'
         * table_index: 0=1000 kbit/s, 1=800 kbit/s, 2=500 kbit/s, 3=250 kbit/s,
         *   4=125 kbit/s, 6=50 kbit/s, 7=20 kbit/s, 8=10 kbit/s, 9=auto */
        else if (strcmp(tok, "lss_conf_bitrate") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
            uint8_t tableIndex;
            int maxIndex = (sizeof(CO_LSS_bitTimingTableLookup) /
                            sizeof(CO_LSS_bitTimingTableLookup[0])) - 1;

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* First parameter is table selector. We only support the CiA
             * bit timing table from CiA301 ("0") */
            closed = 0;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            (void)getU32(tok, 0, 0, &err);

            /* get value */
            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            tableIndex = (uint8_t)getU32(tok, 0, maxIndex, &err);
            if (tableIndex == 5) err = true;
            if (err) break;
            gtwa->lssBitrate = CO_LSS_bitTimingTableLookup[tableIndex];

            /* continue with state machine */
            gtwa->state = CO_GTWA_ST_LSS_CONF_BITRATE;
        }
        /* LSS activate new bit-rate command -
         * 'lss_activate_bitrate <switch_delay_ms>' */
        else if (strcmp(tok, "lss_activate_bitrate") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
            uint16_t switchDelay;
            CO_LSSmaster_return_t ret;

            if (closed != 0 || NodeErr) {
                err = true;
                break;
            }

            /* get value */
            closed = 1;
            CO_fifo_readToken(&gtwa->commFifo, tok, sizeof(tok), &closed, &err);
            switchDelay = (uint16_t)getU32(tok, 0, 0xFFFF, &err);
            if (err) break;

            /* send non-confirmed message */
            ret = CO_LSSmaster_ActivateBit(gtwa->LSSmaster, switchDelay);
            if (ret == CO_LSSmaster_OK) {
                responseWithOK(gtwa);
            }
            else {
                respErrorCode = CO_GTWA_respErrorInternalState;
                err = true;
                break;
            }
        }
        /* LSS store configuration command - 'lss_store' */
        else if (strcmp(tok, "lss_store") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }

            /* continue with state machine */
            gtwa->state = CO_GTWA_ST_LSS_STORE;
        }
        /* Inquire LSS address command - 'lss_inquire_addr [<LSSSUB=0..3>]' */
        else if (strcmp(tok, "lss_inquire_addr") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);

            if (NodeErr) {
                err = true;
                break;
            }

            if (closed == 0) {
                uint8_t lsssub;
                /* get value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                lsssub = (uint8_t)getU32(tok, 0, 3, &err);
                if (err) break;
                switch (lsssub) {
                    case 0: gtwa->lssInquireCs = CO_LSS_INQUIRE_VENDOR; break;
                    case 1: gtwa->lssInquireCs = CO_LSS_INQUIRE_PRODUCT; break;
                    case 2: gtwa->lssInquireCs = CO_LSS_INQUIRE_REV; break;
                    default: gtwa->lssInquireCs = CO_LSS_INQUIRE_SERIAL; break;
                }

                /* continue with state machine */
                gtwa->state = CO_GTWA_ST_LSS_INQUIRE;
            }
            else {
                /* continue with state machine */
                gtwa->state = CO_GTWA_ST_LSS_INQUIRE_ADDR_ALL;
            }
        }
        /* LSS inquire node-ID command - 'lss_get_node'*/
        else if (strcmp(tok, "lss_get_node") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);

            if (closed != 1 || NodeErr) {
                err = true;
                break;
            }

            /* continue with state machine */
            gtwa->lssInquireCs = CO_LSS_INQUIRE_NODE_ID;
            gtwa->state = CO_GTWA_ST_LSS_INQUIRE;
        }
        /* LSS identify fastscan. This is a manufacturer specific command as
         * the one in DSP309 is quite useless - '_lss_fastscan [<timeout_ms>]'*/
        else if (strcmp(tok, "_lss_fastscan") == 0) {
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
            uint16_t timeout_ms = 0;

            if (NodeErr) {
                err = true;
                break;
            }

            if (closed == 0) {
                /* get value */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                timeout_ms = (uint16_t)getU32(tok, 0, 0xFFFF, &err);
                if (err) break;
            }

            /* If timeout not specified, use 100ms. Should work in most cases */
            if (timeout_ms == 0) {
                timeout_ms = 100;
            }
            CO_LSSmaster_changeTimeout(gtwa->LSSmaster, timeout_ms);

            /* prepare lssFastscan, all zero */
            memset(&gtwa->lssFastscan, 0, sizeof(gtwa->lssFastscan));

            /* continue with state machine */
            gtwa->state = CO_GTWA_ST__LSS_FASTSCAN;
        }
        /* LSS complete node-ID configuration command - 'lss_allnodes
         * [<timeout_ms> [<nodeStart=1..127> <store=0|1>
         * <scanType0=0..2> <vendorId> <scanType1=0..2> <productCode>
         * <scanType2=0..2> <revisionNo> <scanType3=0..2> <serialNo>]]' */
        else if (strcmp(tok, "lss_allnodes") == 0) {
            /* Request node enumeration by LSS identify fastscan.
             * This initiates node enumeration by the means of LSS fastscan
             * mechanism. When this function is finished:
             * - All nodes that match the given criteria are assigned a node ID
             *   beginning with nodeId. If 127 is reached, the process
             *   is stopped, no matter if there are nodes remaining or not.
             * - No IDs are assigned because:
             *   - the given criteria do not match any node,
             *   - all nodes are already configured.
             * This function needs that no node is selected when starting the
             * scan process. */
            bool_t NodeErr = checkNet(gtwa, net, &respErrorCode);
            uint16_t timeout_ms = 0;

            if (NodeErr) {
                err = true;
                break;
            }

            if (closed == 0) {
                /* get optional token timeout (non standard) */
                closed = -1;
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                timeout_ms = (uint16_t)getU32(tok, 0, 0xFFFF, &err);
                if (err) break;

            }
            /* If timeout not specified, use 100ms. Should work in most cases */
            gtwa->lssTimeout_ms = timeout_ms == 0 ? 100 : timeout_ms;
            CO_LSSmaster_changeTimeout(gtwa->LSSmaster, gtwa->lssTimeout_ms);
            gtwa->lssNodeCount = 0;
            gtwa->lssSubState = 0;

            if (closed == 1) {
                /* No other arguments, as by CiA specification for this command.
                 * Do full scan. */
                /* use start node ID 2. Should work in most cases */
                gtwa->lssNID = 2;
                /* store node ID in node's NVM */
                gtwa->lssStore = true;
                /* prepare lssFastscan, all zero */
                memset(&gtwa->lssFastscan, 0, sizeof(gtwa->lssFastscan));
            }
            if (closed == 0) {
                /* more arguments follow */
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                gtwa->lssNID = (uint8_t)getU32(tok, 1, 127, &err);
                if (err) break;

                closed = -1;
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                gtwa->lssStore = (bool_t)getU32(tok, 0, 1, &err);
                if (err) break;

                if (closed == 1) {
                    /* No other arguments, prepare lssFastscan, all zero */
                    memset(&gtwa->lssFastscan, 0, sizeof(gtwa->lssFastscan));
                }
            }
            if (closed == 0) {
                /* more arguments follow */
                CO_LSSmaster_fastscan_t *fs = &gtwa->lssFastscan;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->scan[CO_LSS_FASTSCAN_VENDOR_ID] = getU32(tok, 0, 2, &err);
                if (err) break;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->match.identity.vendorID = getU32(tok, 0, 0xFFFFFFFF, &err);
                if (err) break;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->scan[CO_LSS_FASTSCAN_PRODUCT] = getU32(tok, 0, 2, &err);
                if (err) break;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->match.identity.productCode = getU32(tok,0,0xFFFFFFFF, &err);
                if (err) break;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->scan[CO_LSS_FASTSCAN_REV] = getU32(tok, 0, 2, &err);
                if (err) break;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->match.identity.revisionNumber=getU32(tok,0,0xFFFFFFFF,&err);
                if (err) break;

                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->scan[CO_LSS_FASTSCAN_SERIAL] = getU32(tok, 0, 2, &err);
                if (err) break;

                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                fs->match.identity.serialNumber = getU32(tok,0,0xFFFFFFFF,&err);
                if (err) break;
            }

            /* continue with state machine */
            gtwa->state = CO_GTWA_ST_LSS_ALLNODES;
        }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG
        /* Print message log */
        else if (strcmp(tok, "log") == 0) {
            if (closed == 0) {
                err = true;
                break;
            }
            gtwa->state = CO_GTWA_ST_LOG;
        }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP
        /* Print help */
        else if (strcmp(tok, "help") == 0) {
            if (closed == 1) {
                gtwa->helpString = CO_GTWA_helpString;
            }
            else {
                /* get second token */
                closed = 1;
                CO_fifo_readToken(&gtwa->commFifo,tok,sizeof(tok),&closed,&err);
                if (err) break;

                convertToLower(tok, sizeof(tok));
                if (strcmp(tok, "datatype") == 0) {
                    gtwa->helpString = CO_GTWA_helpStringDatatypes;
                }
                else if (strcmp(tok, "lss") == 0) {
                    gtwa->helpString = CO_GTWA_helpStringLss;
                }
                else {
                    err = true;
                    break;
                }
            }
            /* continue with state machine */
            gtwa->helpStringOffset = 0;
            gtwa->state = CO_GTWA_ST_HELP;
        }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
        /* Print status led diodes */
        else if (strcmp(tok, "led") == 0) {
            if (closed == 0) {
                err = true;
                break;
            }
            gtwa->ledStringPreviousIndex = 0xFF;
            gtwa->state = CO_GTWA_ST_LED;
        }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS */

        /* Unrecognized command */
        else {
            respErrorCode = CO_GTWA_respErrorReqNotSupported;
            err = true;
            break;
        }
    } /* while CO_GTWA_ST_IDLE && CO_fifo_CommSearch */



    /***************************************************************************
    * STATE MACHINE
    ***************************************************************************/
    /* If error, generate error response */
    if (err) {
        if (respErrorCode == CO_GTWA_respErrorNone) {
            respErrorCode = CO_GTWA_respErrorSyntax;
        }
        responseWithError(gtwa, respErrorCode);

        /* delete command, if it was only partially read */
        if(closed == 0) {
            CO_fifo_CommSearch(&gtwa->commFifo, true);
        }
        gtwa->state = CO_GTWA_ST_IDLE;
    }

    else switch (gtwa->state) {
    case CO_GTWA_ST_IDLE: {
        return; /* skip timerNext_us calculation */
    }

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO
    /* SDO upload state */
    case CO_GTWA_ST_READ: {
        CO_SDO_abortCode_t abortCode;
        size_t sizeTransferred;
        CO_SDO_return_t ret;

        ret = CO_SDOclientUpload(gtwa->SDO_C,
                                 timeDifference_us,
                                 false,
                                 &abortCode,
                                 NULL,
                                 &sizeTransferred,
                                 timerNext_us);

        if (ret < 0) {
            responseWithErrorSDO(gtwa, abortCode, gtwa->SDOdataCopyStatus);
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        /* Response data must be read, partially or whole */
        else if (ret == CO_SDO_RT_uploadDataBufferFull
                 || ret == CO_SDO_RT_ok_communicationEnd
        ) {
            size_t fifoRemain;

            /* write response head first */
            if (!gtwa->SDOdataCopyStatus) {
                gtwa->respBufCount = snprintf(gtwa->respBuf,
                                              CO_GTWA_RESP_BUF_SIZE - 2,
                                              "[%"PRId32"] ",
                                              gtwa->sequence);
                gtwa->SDOdataCopyStatus = true;
            }

            /* Empty SDO fifo buffer in multiple cycles. Repeat until
             * application runs out of space (respHold) or fifo empty. */
            do {
                /* read SDO fifo (partially) and print specific data type as
                 * ascii into intermediate respBuf */
                gtwa->respBufCount += gtwa->SDOdataType->dataTypePrint(
                    &gtwa->SDO_C->bufFifo,
                    &gtwa->respBuf[gtwa->respBufCount],
                    CO_GTWA_RESP_BUF_SIZE - 2 - gtwa->respBufCount,
                    ret == CO_SDO_RT_ok_communicationEnd);
                fifoRemain = CO_fifo_getOccupied(&gtwa->SDO_C->bufFifo);

                /* end of communication, print newline and enter idle state */
                if (ret == CO_SDO_RT_ok_communicationEnd && fifoRemain == 0) {
                    gtwa->respBufCount +=
                        sprintf(&gtwa->respBuf[gtwa->respBufCount], "\r\n");
                    gtwa->state = CO_GTWA_ST_IDLE;
                }

                /* transfer response to the application */
                if (respBufTransfer(gtwa) == false) {
                    /* broken communication, send SDO abort and force finish. */
                    abortCode = CO_SDO_AB_DATA_TRANSF;
                    CO_SDOclientUpload(gtwa->SDO_C,
                                       0,
                                       true,
                                       &abortCode,
                                       NULL,
                                       NULL,
                                       NULL);
                    gtwa->state = CO_GTWA_ST_IDLE;
                    break;
                }
            } while (gtwa->respHold == false && fifoRemain > 0);
        }
        break;
    }

    /* SDO download state */
    case CO_GTWA_ST_WRITE:
    case CO_GTWA_ST_WRITE_ABORTED: {
        CO_SDO_abortCode_t abortCode;
        size_t sizeTransferred;
        bool_t abort = false;
        bool_t hold = false;
        CO_SDO_return_t ret;

        /* copy data to the SDO buffer if previous dataTypeScan was partial */
        if (gtwa->SDOdataCopyStatus) {
            CO_fifo_st status;
            gtwa->SDOdataType->dataTypeScan(&gtwa->SDO_C->bufFifo,
                                            &gtwa->commFifo,
                                            &status);
            /* set to true, if command delimiter was found */
            closed = ((status & CO_fifo_st_closed) == 0) ? 0 : 1;
            /* set to true, if data are copied only partially */
            gtwa->SDOdataCopyStatus = (status & CO_fifo_st_partial) != 0;

            /* is syntax error in command or not the last token in command */
            if ((status & CO_fifo_st_errMask) != 0
                || (gtwa->SDOdataCopyStatus == false && closed != 1)
            ) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                abort = true; /* abort SDO communication */
                /* clear the rest of the command, if necessary */
                if (closed != 1)
                    CO_fifo_CommSearch(&gtwa->commFifo, true);
            }
            if (gtwa->state == CO_GTWA_ST_WRITE_ABORTED) {
                /* Stay in this state, until all data transferred via commFifo
                 * will be purged. */
                if (!CO_fifo_purge(&gtwa->SDO_C->bufFifo) || closed == 1) {
                    gtwa->state = CO_GTWA_ST_IDLE;
                }
                break;
            }
        }
        /* If not all data were transferred, make sure, there is enough data in
         * SDO buffer, to continue communication. Otherwise wait and check for
         * timeout */
        if (gtwa->SDOdataCopyStatus
            && CO_fifo_getOccupied(&gtwa->SDO_C->bufFifo) <
               (CO_CONFIG_GTW_BLOCK_DL_LOOP * 7)
        ) {
            if (gtwa->stateTimeoutTmr > CO_GTWA_STATE_TIMEOUT_TIME_US) {
                abortCode = CO_SDO_AB_DEVICE_INCOMPAT;
                abort = true;
            }
            else {
                gtwa->stateTimeoutTmr += timeDifference_us;
                hold = true;
            }
        }
        if (!hold || abort) {
            /* if OS has CANtx queue, speedup block transfer */
            int loop = 0;
            do {
                ret = CO_SDOclientDownload(gtwa->SDO_C,
                                           timeDifference_us,
                                           abort,
                                           gtwa->SDOdataCopyStatus,
                                           &abortCode,
                                           &sizeTransferred,
                                           timerNext_us);
                if (++loop >= CO_CONFIG_GTW_BLOCK_DL_LOOP) {
                    break;
                }
            } while (ret == CO_SDO_RT_blockDownldInProgress);

            /* send response in case of error or finish */
            if (ret < 0) {
                responseWithErrorSDO(gtwa, abortCode, false);
                /* purge remaining data if necessary */
                gtwa->state = gtwa->SDOdataCopyStatus
                              ? CO_GTWA_ST_WRITE_ABORTED
                              : CO_GTWA_ST_IDLE;
            }
            else if (ret == CO_SDO_RT_ok_communicationEnd) {
                responseWithOK(gtwa);
                gtwa->state = CO_GTWA_ST_IDLE;
            }
        }
        break;
    }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_SDO */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS
    case CO_GTWA_ST_LSS_SWITCH_GLOB: {
        CO_LSSmaster_return_t ret;
        ret = CO_LSSmaster_switchStateSelect(gtwa->LSSmaster,
                                             timeDifference_us,
                                             NULL);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            responseLSS(gtwa, ret);
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_SWITCH_SEL: {
        CO_LSSmaster_return_t ret;
        ret = CO_LSSmaster_switchStateSelect(gtwa->LSSmaster,
                                             timeDifference_us,
                                             &gtwa->lssAddress);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            responseLSS(gtwa, ret);
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_SET_NODE: {
        CO_LSSmaster_return_t ret;
        ret = CO_LSSmaster_configureNodeId(gtwa->LSSmaster,
                                           timeDifference_us,
                                           gtwa->lssNID);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            if (ret == CO_LSSmaster_OK_ILLEGAL_ARGUMENT) {
                respErrorCode = CO_GTWA_respErrorLSSnodeIdNotSupported;
                responseWithError(gtwa, respErrorCode);
            }
            else {
                responseLSS(gtwa, ret);
            }
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_CONF_BITRATE: {
        CO_LSSmaster_return_t ret;
        ret = CO_LSSmaster_configureBitTiming(gtwa->LSSmaster,
                                              timeDifference_us,
                                              gtwa->lssBitrate);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            if (ret == CO_LSSmaster_OK_ILLEGAL_ARGUMENT) {
                respErrorCode = CO_GTWA_respErrorLSSbitRateNotSupported;
                responseWithError(gtwa, respErrorCode);
            }
            else {
                responseLSS(gtwa, ret);
            }
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_STORE: {
        CO_LSSmaster_return_t ret;

        ret = CO_LSSmaster_configureStore(gtwa->LSSmaster, timeDifference_us);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            if (ret == CO_LSSmaster_OK_ILLEGAL_ARGUMENT) {
                respErrorCode = CO_GTWA_respErrorLSSparameterStoringFailed;
                responseWithError(gtwa, respErrorCode);
            }
            else {
                responseLSS(gtwa, ret);
            }
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_INQUIRE: {
        CO_LSSmaster_return_t ret;
        uint32_t value;

        ret = CO_LSSmaster_Inquire(gtwa->LSSmaster, timeDifference_us,
                                   gtwa->lssInquireCs, &value);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            if (ret == CO_LSSmaster_OK) {
                if (gtwa->lssInquireCs == CO_LSS_INQUIRE_NODE_ID) {
                    gtwa->respBufCount =
                        snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                 "[%"PRId32"] 0x%02"PRIX32"\r\n",
                                 gtwa->sequence, value & 0xFF);
                } else {
                    gtwa->respBufCount =
                        snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                 "[%"PRId32"] 0x%08"PRIX32"\r\n",
                                 gtwa->sequence, value);
                }
                respBufTransfer(gtwa);
            }
            else {
                responseLSS(gtwa, ret);
            }
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_INQUIRE_ADDR_ALL: {
        CO_LSSmaster_return_t ret;

        ret = CO_LSSmaster_InquireLssAddress(gtwa->LSSmaster, timeDifference_us,
                                             &gtwa->lssAddress);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            if (ret == CO_LSSmaster_OK) {
                gtwa->respBufCount =
                    snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                             "[%"PRId32"] 0x%08"PRIX32" 0x%08"PRIX32 \
                             " 0x%08"PRIX32" 0x%08"PRIX32"\r\n",
                             gtwa->sequence,
                             gtwa->lssAddress.identity.vendorID,
                             gtwa->lssAddress.identity.productCode,
                             gtwa->lssAddress.identity.revisionNumber,
                             gtwa->lssAddress.identity.serialNumber);
                respBufTransfer(gtwa);
            }
            else {
                responseLSS(gtwa, ret);
            }
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST__LSS_FASTSCAN: {
        CO_LSSmaster_return_t ret;

        ret = CO_LSSmaster_IdentifyFastscan(gtwa->LSSmaster, timeDifference_us,
                                            &gtwa->lssFastscan);
        if (ret != CO_LSSmaster_WAIT_SLAVE) {
            if (ret == CO_LSSmaster_OK || ret == CO_LSSmaster_SCAN_FINISHED) {
                gtwa->respBufCount =
                    snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                             "[%"PRId32"] 0x%08"PRIX32" 0x%08"PRIX32 \
                             " 0x%08"PRIX32" 0x%08"PRIX32"\r\n",
                             gtwa->sequence,
                             gtwa->lssFastscan.found.identity.vendorID,
                             gtwa->lssFastscan.found.identity.productCode,
                             gtwa->lssFastscan.found.identity.revisionNumber,
                             gtwa->lssFastscan.found.identity.serialNumber);
                respBufTransfer(gtwa);
            }
            else {
                responseLSS(gtwa, ret);
            }
            CO_LSSmaster_changeTimeout(gtwa->LSSmaster,
                                       CO_LSSmaster_DEFAULT_TIMEOUT);
            gtwa->state = CO_GTWA_ST_IDLE;
        }
        break;
    }
    case CO_GTWA_ST_LSS_ALLNODES: {
        CO_LSSmaster_return_t ret;
        if (gtwa->lssSubState == 0) { /* _lss_fastscan */
            ret = CO_LSSmaster_IdentifyFastscan(gtwa->LSSmaster,
                                                timeDifference_us,
                                                &gtwa->lssFastscan);
            if (ret != CO_LSSmaster_WAIT_SLAVE) {
                CO_LSSmaster_changeTimeout(gtwa->LSSmaster,
                                           CO_LSSmaster_DEFAULT_TIMEOUT);

                if (ret == CO_LSSmaster_OK || ret == CO_LSSmaster_SCAN_NOACK) {
                    /* no (more) nodes found, send report sum and finish */
                    gtwa->respBufCount =
                        snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                 "# Found %d nodes, search finished.\n" \
                                 "[%"PRId32"] OK\r\n",
                                 gtwa->lssNodeCount,
                                 gtwa->sequence);
                    respBufTransfer(gtwa);
                    gtwa->state = CO_GTWA_ST_IDLE;
                }
                else if (ret == CO_LSSmaster_SCAN_FINISHED) {
                    /* next sub-step */
                    gtwa->lssSubState++;
                }
                else {
                    /* error occurred */
                    responseLSS(gtwa, ret);
                    gtwa->state = CO_GTWA_ST_IDLE;
                }
            }
        }
        if (gtwa->lssSubState == 1) { /* lss_set_node */
            ret = CO_LSSmaster_configureNodeId(gtwa->LSSmaster,
                                               timeDifference_us,
                                               gtwa->lssNID);
            if (ret != CO_LSSmaster_WAIT_SLAVE) {
                if (ret == CO_LSSmaster_OK) {
                    /* next sub-step */
                    gtwa->lssSubState += gtwa->lssStore ? 1 : 2;
                }
                else {
                    /* error occurred */
                    if (ret == CO_LSSmaster_OK_ILLEGAL_ARGUMENT) {
                        respErrorCode = CO_GTWA_respErrorLSSnodeIdNotSupported;
                        responseWithError(gtwa, respErrorCode);
                    }
                    else {
                        responseLSS(gtwa, ret);
                    }
                    gtwa->state = CO_GTWA_ST_IDLE;
                }
            }
        }
        if (gtwa->lssSubState == 2) { /* lss_store */
            ret = CO_LSSmaster_configureStore(gtwa->LSSmaster,
                                              timeDifference_us);
            if (ret != CO_LSSmaster_WAIT_SLAVE) {
                if (ret == CO_LSSmaster_OK) {
                    /* next sub-step */
                    gtwa->lssSubState++;
                }
                else {
                    /* error occurred */
                    if (ret == CO_LSSmaster_OK_ILLEGAL_ARGUMENT) {
                        respErrorCode =
                            CO_GTWA_respErrorLSSparameterStoringFailed;
                        responseWithError(gtwa, respErrorCode);
                    }
                    else {
                        responseLSS(gtwa, ret);
                    }
                    gtwa->state = CO_GTWA_ST_IDLE;
                }
            }
        }
        if (gtwa->lssSubState >= 3) { /* lss_switch_glob 0 */
            /* send non-confirmed message */
            ret = CO_LSSmaster_switchStateDeselect(gtwa->LSSmaster);
            if (ret != CO_LSSmaster_OK) {
                /* error occurred */
                responseLSS(gtwa, ret);
                gtwa->state = CO_GTWA_ST_IDLE;
            }
            else {
                /* cycle finished successfully, send report */
                uint8_t lssNidAssigned = gtwa->lssNID;
                const char msg2Fmt[] = "# Not all nodes scanned!\n" \
                                       "[%"PRId32"] OK\r\n";
                char msg2[sizeof(msg2Fmt)+10] = {0};

                /* increment variables, check end-of-nodeId */
                gtwa->lssNodeCount++;
                if (gtwa->lssNID < 127) {
                    /* repeat cycle with next node-id */
                    gtwa->lssNID++;
                    CO_LSSmaster_changeTimeout(gtwa->LSSmaster,
                                               gtwa->lssTimeout_ms);
                    gtwa->lssSubState = 0;
                }
                else {
                    /* If we can't assign more node IDs, quit scanning */
                    sprintf(msg2, msg2Fmt, gtwa->sequence);
                    gtwa->state = CO_GTWA_ST_IDLE;
                }

                /* send report */
                gtwa->respBufCount =
                    snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                             "# Node-ID %d assigned to: 0x%08"PRIX32" 0x%08" \
                             PRIX32" 0x%08"PRIX32" 0x%08"PRIX32"\n%s",
                             lssNidAssigned,
                             gtwa->lssFastscan.found.identity.vendorID,
                             gtwa->lssFastscan.found.identity.productCode,
                             gtwa->lssFastscan.found.identity.revisionNumber,
                             gtwa->lssFastscan.found.identity.serialNumber,
                             msg2);
                respBufTransfer(gtwa);
            }
        }
        break;
    } /* CO_GTWA_ST_LSS_ALLNODES */
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LSS */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG
    /* print message log */
    case CO_GTWA_ST_LOG: {
        do {
            gtwa->respBufCount = CO_fifo_read(&gtwa->logFifo,
                                              (uint8_t *)gtwa->respBuf,
                                              CO_GTWA_RESP_BUF_SIZE, NULL);
            respBufTransfer(gtwa);

            if (CO_fifo_getOccupied(&gtwa->logFifo) == 0) {
                gtwa->state = CO_GTWA_ST_IDLE;
                break;
            }
        } while (gtwa->respHold == false);
        break;
    }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_LOG */

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_HELP
    /* Print help string (in multiple segments if necessary) */
    case CO_GTWA_ST_HELP: {
        size_t lenBuf = CO_GTWA_RESP_BUF_SIZE;
        size_t lenHelp = strlen(gtwa->helpString);

        do {
            size_t lenHelpRemain = lenHelp - gtwa->helpStringOffset;
            size_t lenCopied = lenBuf < lenHelpRemain ? lenBuf : lenHelpRemain;

            memcpy(gtwa->respBuf,
                   &gtwa->helpString[gtwa->helpStringOffset],
                   lenCopied);

            gtwa->respBufCount = lenCopied;
            gtwa->helpStringOffset += lenCopied;
            respBufTransfer(gtwa);

            if (gtwa->helpStringOffset == lenHelp) {
                gtwa->state = CO_GTWA_ST_IDLE;
                break;
            }
        } while (gtwa->respHold == false);
        break;
    }
#endif

#if (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS
    /* print CANopen status LED diodes */
    case CO_GTWA_ST_LED: {
        uint8_t i;

        if (CO_fifo_CommSearch(&gtwa->commFifo, false)) {
            gtwa->state = CO_GTWA_ST_IDLE;
            i = 4;
        }
        else {
            i = CO_LED_RED(gtwa->LEDs, CO_LED_CANopen) * 2 +
                CO_LED_GREEN(gtwa->LEDs, CO_LED_CANopen);
        }
        if (i > (CO_GTWA_LED_PRINTOUTS_SIZE - 1))
            i = CO_GTWA_LED_PRINTOUTS_SIZE - 1;

        if (i != gtwa->ledStringPreviousIndex) {
            gtwa->respBufCount = snprintf(gtwa->respBuf, CO_GTWA_RESP_BUF_SIZE,
                                          "%s", CO_GTWA_LED_PRINTOUTS[i]);
            respBufTransfer(gtwa);
            gtwa->ledStringPreviousIndex = i;
        }
        break;
    }
#endif /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII_PRINT_LEDS */

    /* illegal state */
    default: {
        respErrorCode = CO_GTWA_respErrorInternalState;
        responseWithError(gtwa, respErrorCode);
        gtwa->state = CO_GTWA_ST_IDLE;
        break;
    }
    } /* switch (gtwa->state) */

    /* execute next CANopen processing immediately, if idle and more commands
     * available */
    if (timerNext_us != NULL && gtwa->state == CO_GTWA_ST_IDLE
        && CO_fifo_CommSearch(&gtwa->commFifo, false)
    ) {
        *timerNext_us = 0;
    }
}

#endif  /* (CO_CONFIG_GTW) & CO_CONFIG_GTW_ASCII */
